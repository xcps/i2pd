#include <stdio.h>
#include <sstream>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ini_parser.hpp>

// There is bug in boost 1.49 with gcc 4.7 coming with Debian Wheezy
#define GCC47_BOOST149 ((BOOST_VERSION == 104900) && (__GNUC__ == 4) && (__GNUC_MINOR__ >= 7))
#if !GCC47_BOOST149
#include <boost/property_tree/json_parser.hpp>
#endif

#include "FS.h"
#include "Log.h"
#include "Config.h"
#include "NetDb.h"
#include "RouterContext.h"
#include "Daemon.h"
#include "Tunnel.h"
#include "Timestamp.h"
#include "Transports.h"
#include "version.h"
#include "I2PControl.h"

namespace i2p
{
namespace client
{
	I2PControlService::I2PControlService (const std::string& address, int port):
		m_IsRunning (false), m_Thread (nullptr),
		m_Acceptor (m_Service, boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(address), port)),
		m_SSLContext (m_Service, boost::asio::ssl::context::sslv23),
		m_ShutdownTimer (m_Service)
	{
		i2p::config::GetOption("i2pcontrol.password", m_Password);

		// certificate / keys
		std::string i2pcp_crt; i2p::config::GetOption("i2pcontrol.cert", i2pcp_crt);
		std::string i2pcp_key; i2p::config::GetOption("i2pcontrol.key",  i2pcp_key);

		if (i2pcp_crt.at(0) != '/')
			i2pcp_crt = i2p::fs::DataDirPath(i2pcp_crt);
		if (i2pcp_key.at(0) != '/')
			i2pcp_key = i2p::fs::DataDirPath(i2pcp_key);
		if (!i2p::fs::Exists (i2pcp_crt) || !i2p::fs::Exists (i2pcp_key)) {
			LogPrint (eLogInfo, "I2PControl: creating new certificate for control connection");
			CreateCertificate (i2pcp_crt.c_str(), i2pcp_key.c_str());
		} else {
			LogPrint(eLogDebug, "I2PControl: using cert from ", i2pcp_crt);
		}
		m_SSLContext.set_options (boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
		m_SSLContext.use_certificate_file (i2pcp_crt, boost::asio::ssl::context::pem);
		m_SSLContext.use_private_key_file (i2pcp_key, boost::asio::ssl::context::pem);

		// handlers
		m_MethodHandlers["Authenticate"]   = &I2PControlService::AuthenticateHandler;
		m_MethodHandlers["Echo"]           = &I2PControlService::EchoHandler;
		m_MethodHandlers["I2PControl"]     = &I2PControlService::I2PControlHandler;
		m_MethodHandlers["RouterInfo"]     = &I2PControlService::RouterInfoHandler;
		m_MethodHandlers["RouterManager"]  = &I2PControlService::RouterManagerHandler;
		m_MethodHandlers["NetworkSetting"] = &I2PControlService::NetworkSettingHandler;

		// I2PControl
		m_I2PControlHandlers["i2pcontrol.password"] = &I2PControlService::PasswordHandler; 

		// RouterInfo
		m_RouterInfoHandlers["i2p.router.uptime"]  = &I2PControlService::UptimeHandler;
		m_RouterInfoHandlers["i2p.router.version"] = &I2PControlService::VersionHandler;
		m_RouterInfoHandlers["i2p.router.status"]  = &I2PControlService::StatusHandler;
		m_RouterInfoHandlers["i2p.router.netdb.knownpeers"]   = &I2PControlService::NetDbKnownPeersHandler;
		m_RouterInfoHandlers["i2p.router.netdb.activepeers"]  = &I2PControlService::NetDbActivePeersHandler;
		m_RouterInfoHandlers["i2p.router.net.bw.inbound.1s"]  = &I2PControlService::InboundBandwidth1S;
		m_RouterInfoHandlers["i2p.router.net.bw.outbound.1s"] = &I2PControlService::OutboundBandwidth1S;
		m_RouterInfoHandlers["i2p.router.net.status"]         = &I2PControlService::NetStatusHandler;
		m_RouterInfoHandlers["i2p.router.net.tunnels.participating"] = &I2PControlService::TunnelsParticipatingHandler;
		m_RouterInfoHandlers["i2p.router.net.total.received.bytes"]  = &I2PControlService::NetTotalReceivedBytes;
		m_RouterInfoHandlers["i2p.router.net.total.sent.bytes"]      = &I2PControlService::NetTotalSentBytes;

		// RouterManager	
		m_RouterManagerHandlers["Reseed"]           = &I2PControlService::ReseedHandler;
		m_RouterManagerHandlers["Shutdown"]         = &I2PControlService::ShutdownHandler; 
		m_RouterManagerHandlers["ShutdownGraceful"] = &I2PControlService::ShutdownGracefulHandler;
	}

	I2PControlService::~I2PControlService ()
	{
		Stop ();
	}

	void I2PControlService::Start ()
	{
		if (!m_IsRunning)
		{
			Accept ();
			m_IsRunning = true;
			m_Thread = new std::thread (std::bind (&I2PControlService::Run, this));
		}
	}

	void I2PControlService::Stop ()
	{
		if (m_IsRunning)
		{
			m_IsRunning = false;
			m_Acceptor.cancel ();
			m_Service.stop ();
			if (m_Thread)
			{
				m_Thread->join ();
				delete m_Thread;
				m_Thread = nullptr;
			}
		}
	}

	void I2PControlService::Run ()
	{
		while (m_IsRunning)
		{
			try {
				m_Service.run ();
			} catch (std::exception& ex) {
				LogPrint (eLogError, "I2PControl: runtime exception: ", ex.what ());
			}	
		}	
	}

	void I2PControlService::Accept ()
	{
		auto newSocket = std::make_shared<ssl_socket> (m_Service, m_SSLContext);
		m_Acceptor.async_accept (newSocket->lowest_layer(), std::bind (&I2PControlService::HandleAccept, this,
			std::placeholders::_1, newSocket));
	}

	void I2PControlService::HandleAccept(const boost::system::error_code& ecode, std::shared_ptr<ssl_socket> socket)
	{
		if (ecode != boost::asio::error::operation_aborted)
			Accept ();

		if (ecode) {
			LogPrint (eLogError, "I2PControl: accept error: ",  ecode.message ());
			return;
		}
		LogPrint (eLogDebug, "I2PControl: new request from ", socket->lowest_layer ().remote_endpoint ());
		Handshake (socket);
	}

	void I2PControlService::Handshake (std::shared_ptr<ssl_socket> socket)
	{
		socket->async_handshake(boost::asio::ssl::stream_base::server,
        	std::bind( &I2PControlService::HandleHandshake, this, std::placeholders::_1, socket));
	}

	void I2PControlService::HandleHandshake (const boost::system::error_code& ecode, std::shared_ptr<ssl_socket> socket)
	{
		if (ecode) {
			LogPrint (eLogError, "I2PControl: handshake error: ", ecode.message ());
			return;
		}	
		//std::this_thread::sleep_for (std::chrono::milliseconds(5));
		ReadRequest (socket);
	}

	void I2PControlService::ReadRequest (std::shared_ptr<ssl_socket> socket)
	{
		auto request = std::make_shared<I2PControlBuffer>();
		socket->async_read_some (
#if defined(BOOST_ASIO_HAS_STD_ARRAY)
			boost::asio::buffer (*request),
#else
			boost::asio::buffer (request->data (), request->size ()),
#endif
			std::bind(&I2PControlService::HandleRequestReceived, this,
			std::placeholders::_1, std::placeholders::_2, socket, request));
	}

	void I2PControlService::HandleRequestReceived (const boost::system::error_code& ecode,
 		size_t bytes_transferred, std::shared_ptr<ssl_socket> socket,
		std::shared_ptr<I2PControlBuffer> buf)
	{
		if (ecode) {
			LogPrint (eLogError, "I2PControl: read error: ", ecode.message ());
			return;
		} else {
			try
			{
				bool isHtml = !memcmp (buf->data (), "POST", 4);
				std::stringstream ss;
				ss.write (buf->data (), bytes_transferred);
				if (isHtml)
				{
					std::string header;
					size_t contentLength = 0;
					while (!ss.eof () && header != "\r")
					{
						std::getline(ss, header);
						auto colon = header.find (':');
						if (colon != std::string::npos && header.substr (0, colon) == "Content-Length")
							contentLength = std::stoi (header.substr (colon + 1));
					}
					if (ss.eof ())
					{
						LogPrint (eLogError, "I2PControl: malformed request, HTTP header expected");
						return; // TODO:
					}
					std::streamoff rem = contentLength + ss.tellg () - bytes_transferred; // more bytes to read
					if (rem > 0)
					{
						bytes_transferred = boost::asio::read (*socket, boost::asio::buffer (buf->data (), rem));
						ss.write (buf->data (), bytes_transferred);
					}
				}
				std::ostringstream response;
#if GCC47_BOOST149
				LogPrint (eLogError, "I2PControl: json_read is not supported due bug in boost 1.49 with gcc 4.7");
				response << "{\"id\":null,\"error\":";
				response << "{\"code\":-32603,\"message\":\"JSON requests is not supported with this version of boost\"},";
				response << "\"jsonrpc\":\"2.0\"}";
#else
				boost::property_tree::ptree pt;
				boost::property_tree::read_json (ss, pt);

				std::string id     = pt.get<std::string>("id");
				std::string method = pt.get<std::string>("method");
				auto it = m_MethodHandlers.find (method);
				if (it != m_MethodHandlers.end ())
				{
					response << "{\"id\":" << id << ",\"result\":{";
					(this->*(it->second))(pt.get_child ("params"), response);
					response << "},\"jsonrpc\":\"2.0\"}";
				} else {
					LogPrint (eLogWarning, "I2PControl: unknown method ", method);
					response << "{\"id\":null,\"error\":";
					response << "{\"code\":-32601,\"message\":\"Method not found\"},";
					response << "\"jsonrpc\":\"2.0\"}";
				}
#endif
				SendResponse (socket, buf, response, isHtml);
			}
			catch (std::exception& ex)
			{
				LogPrint (eLogError, "I2PControl: exception when handle request: ", ex.what ());
			}
			catch (...)
			{
				LogPrint (eLogError, "I2PControl: handle request unknown exception");
			}
		}
	}

	void I2PControlService::InsertParam (std::ostringstream& ss, const std::string& name, int value) const
	{
		ss << "\"" << name << "\":" << value;
	}

	void I2PControlService::InsertParam (std::ostringstream& ss, const std::string& name, const std::string& value) const
	{
		ss << "\"" << name << "\":";
		if (value.length () > 0)
			ss << "\"" << value << "\"";
		else
			ss << "null";
	}

	void I2PControlService::InsertParam (std::ostringstream& ss, const std::string& name, double value) const
	{
		ss << "\"" << name << "\":" << std::fixed << std::setprecision(2) << value;
	}

	void I2PControlService::SendResponse (std::shared_ptr<ssl_socket> socket,
		std::shared_ptr<I2PControlBuffer> buf, std::ostringstream& response, bool isHtml)
	{
		size_t len = response.str ().length (), offset = 0;
		if (isHtml)
		{
			std::ostringstream header;
			header << "HTTP/1.1 200 OK\r\n";
			header << "Connection: close\r\n";
			header << "Content-Length: " << boost::lexical_cast<std::string>(len) << "\r\n";
			header << "Content-Type: application/json\r\n";
			header << "Date: ";
			auto facet = new boost::local_time::local_time_facet ("%a, %d %b %Y %H:%M:%S GMT");
			header.imbue(std::locale (header.getloc(), facet));
			header << boost::posix_time::second_clock::local_time() << "\r\n";
			header << "\r\n";
			offset = header.str ().size ();
			memcpy (buf->data (), header.str ().c_str (), offset);
		}
		memcpy (buf->data () + offset, response.str ().c_str (), len);
		boost::asio::async_write (*socket, boost::asio::buffer (buf->data (), offset + len),
			boost::asio::transfer_all (),
			std::bind(&I2PControlService::HandleResponseSent, this,
				std::placeholders::_1, std::placeholders::_2, socket, buf));
	}

	void I2PControlService::HandleResponseSent (const boost::system::error_code& ecode, std::size_t bytes_transferred,
		std::shared_ptr<ssl_socket> socket, std::shared_ptr<I2PControlBuffer> buf)
	{
		if (ecode) {
			LogPrint (eLogError, "I2PControl: write error: ", ecode.message ());
		}
	}

// handlers

	void I2PControlService::AuthenticateHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		int api       = params.get<int> ("API");
		auto password = params.get<std::string> ("Password");
		LogPrint (eLogDebug, "I2PControl: Authenticate API=", api, " Password=", password);
		if (password != m_Password) {
			LogPrint (eLogError, "I2PControl: Authenticate - Invalid password: ", password);
			return;
		}
		InsertParam (results, "API", api);
		results << ",";
		std::string token = boost::lexical_cast<std::string>(i2p::util::GetSecondsSinceEpoch ());
		m_Tokens.insert (token);	
		InsertParam (results, "Token", token);
	}	

	void I2PControlService::EchoHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		auto echo = params.get<std::string> ("Echo");
		LogPrint (eLogDebug, "I2PControl Echo Echo=", echo);
		InsertParam (results, "Result", echo);
	}


// I2PControl

	void I2PControlService::I2PControlHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		for (auto& it: params)
		{
			LogPrint (eLogDebug, "I2PControl: I2PControl request: ", it.first);
			auto it1 = m_I2PControlHandlers.find (it.first);
			if (it1 != m_I2PControlHandlers.end ())
			{
				(this->*(it1->second))(it.second.data ());
				InsertParam (results, it.first, "");
			}
			else
				LogPrint (eLogError, "I2PControl: I2PControl unknown request: ", it.first);
		}	
	}

	void I2PControlService::PasswordHandler (const std::string& value)
	{
		LogPrint (eLogWarning, "I2PControl: new password=", value, ", to make it persistent you should update your config!");
		m_Password = value;
		m_Tokens.clear ();
	}

// RouterInfo

	void I2PControlService::RouterInfoHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		for (auto it = params.begin (); it != params.end (); it++)
		{
			LogPrint (eLogDebug, "I2PControl: RouterInfo request: ", it->first);
			auto it1 = m_RouterInfoHandlers.find (it->first);
			if (it1 != m_RouterInfoHandlers.end ())
			{
				if (it != params.begin ()) results << ",";
				(this->*(it1->second))(results);
			}
			else
				LogPrint (eLogError, "I2PControl: RouterInfo unknown request ", it->first);
		}
	}

	void I2PControlService::UptimeHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.uptime", (int)i2p::context.GetUptime ()*1000);	
	}

	void I2PControlService::VersionHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.version", VERSION);
	}	

	void I2PControlService::StatusHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.status", "???"); // TODO:
	}

	void I2PControlService::NetDbKnownPeersHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.netdb.knownpeers", i2p::data::netdb.GetNumRouters ());	
	}

	void I2PControlService::NetDbActivePeersHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.netdb.activepeers", (int)i2p::transport::transports.GetPeers ().size ());	
	}

	void I2PControlService::NetStatusHandler (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.net.status", (int)i2p::context.GetStatus ());
	}

	void I2PControlService::TunnelsParticipatingHandler (std::ostringstream& results)
	{
		int transit = i2p::tunnel::tunnels.GetTransitTunnels ().size ();
		InsertParam (results, "i2p.router.net.tunnels.participating", transit);
	}

	void I2PControlService::InboundBandwidth1S (std::ostringstream& results)
	{
		double bw = i2p::transport::transports.GetInBandwidth ();
		InsertParam (results, "i2p.router.net.bw.inbound.1s", bw);
	}

	void I2PControlService::OutboundBandwidth1S (std::ostringstream& results)
	{
		double bw = i2p::transport::transports.GetOutBandwidth ();
		InsertParam (results, "i2p.router.net.bw.outbound.1s", bw);
	}

	void I2PControlService::NetTotalReceivedBytes (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.net.total.received.bytes", (double)i2p::transport::transports.GetTotalReceivedBytes ());
	}

	void I2PControlService::NetTotalSentBytes (std::ostringstream& results)
	{
		InsertParam (results, "i2p.router.net.total.sent.bytes",     (double)i2p::transport::transports.GetTotalSentBytes ());
	}

// RouterManager

	void I2PControlService::RouterManagerHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		for (auto it = params.begin (); it != params.end (); it++)
		{
			if (it != params.begin ()) results << ",";	
			LogPrint (eLogDebug, "I2PControl: RouterManager request: ", it->first);
			auto it1 = m_RouterManagerHandlers.find (it->first);
			if (it1 != m_RouterManagerHandlers.end ()) {
				(this->*(it1->second))(results);	
			} else
				LogPrint (eLogError, "I2PControl: RouterManager unknown request: ", it->first);
		}
	}


	void I2PControlService::ShutdownHandler (std::ostringstream& results)
	{
		LogPrint (eLogInfo, "I2PControl: Shutdown requested");
		InsertParam (results, "Shutdown", "");
		m_ShutdownTimer.expires_from_now (boost::posix_time::seconds(1)); // 1 second to make sure response has been sent
		m_ShutdownTimer.async_wait (
			[](const boost::system::error_code& ecode)
		    {
				Daemon.running = 0;
			});
	}

	void I2PControlService::ShutdownGracefulHandler (std::ostringstream& results)
	{
		i2p::context.SetAcceptsTunnels (false);
		int timeout = i2p::tunnel::tunnels.GetTransitTunnelsExpirationTimeout ();
		LogPrint (eLogInfo, "I2PControl: Graceful shutdown requested, ", timeout, " seconds remains");
		InsertParam (results, "ShutdownGraceful", "");
		m_ShutdownTimer.expires_from_now (boost::posix_time::seconds(timeout + 1)); // + 1 second
		m_ShutdownTimer.async_wait (
			[](const boost::system::error_code& ecode)
		    {
				Daemon.running = 0;
			});
	}

	void I2PControlService::ReseedHandler (std::ostringstream& results)
	{
		LogPrint (eLogInfo, "I2PControl: Reseed requested");
		InsertParam (results, "Reseed", "");
		i2p::data::netdb.Reseed ();
	}

// network setting
	void I2PControlService::NetworkSettingHandler (const boost::property_tree::ptree& params, std::ostringstream& results)
	{
		for (auto it = params.begin (); it != params.end (); it++)
		{
			if (it != params.begin ()) results << ",";	
			LogPrint (eLogDebug, "I2PControl: NetworkSetting request: ", it->first);
			auto it1 = m_NetworkSettingHandlers.find (it->first);
			if (it1 != m_NetworkSettingHandlers.end ()) {
				(this->*(it1->second))(it->second.data (), results);	
			} else
				LogPrint (eLogError, "I2PControl: NetworkSetting unknown request: ", it->first);
		}
	}

	// certificate	
	void I2PControlService::CreateCertificate (const char *crt_path, const char *key_path)
	{
		FILE *f = NULL;
		EVP_PKEY * pkey = EVP_PKEY_new ();
		RSA * rsa = RSA_new ();
		BIGNUM * e = BN_dup (i2p::crypto::GetRSAE ());
		RSA_generate_key_ex (rsa, 4096, e, NULL);
		BN_free (e);
		if (rsa)
		{
			EVP_PKEY_assign_RSA (pkey, rsa);
			X509 * x509 = X509_new ();
			ASN1_INTEGER_set (X509_get_serialNumber (x509), 1);
			X509_gmtime_adj (X509_get_notBefore (x509), 0);
			X509_gmtime_adj (X509_get_notAfter (x509), I2P_CONTROL_CERTIFICATE_VALIDITY*24*60*60); // expiration
			X509_set_pubkey (x509, pkey); // public key
			X509_NAME * name = X509_get_subject_name (x509);
			X509_NAME_add_entry_by_txt (name, "C",  MBSTRING_ASC, (unsigned char *)"RU", -1, -1, 0); // country (Russia by default)
			X509_NAME_add_entry_by_txt (name, "O",  MBSTRING_ASC, (unsigned char *)I2P_CONTROL_CERTIFICATE_ORGANIZATION, -1, -1, 0); // organization
			X509_NAME_add_entry_by_txt (name, "CN", MBSTRING_ASC, (unsigned char *)I2P_CONTROL_CERTIFICATE_COMMON_NAME, -1, -1, 0); // common name
			X509_set_issuer_name (x509, name); // set issuer to ourselves
			X509_sign (x509, pkey, EVP_sha1 ()); // sign

			// save cert
			if ((f = fopen (crt_path, "wb")) != NULL) {
				LogPrint (eLogInfo, "I2PControl: saving new cert to ", crt_path);
				PEM_write_X509 (f, x509);
				fclose (f);
			} else {
				LogPrint (eLogError, "I2PControl: can't write cert: ", strerror(errno));
			}

			// save key
			if ((f = fopen (key_path, "wb")) != NULL) {
				LogPrint (eLogInfo, "I2PControl: saving cert key to ", key_path);
				PEM_write_PrivateKey (f, pkey, NULL, NULL, 0, NULL, NULL);
				fclose (f);
			} else {
				LogPrint (eLogError, "I2PControl: can't write key: ", strerror(errno));
			}

			X509_free (x509);
		} else {
			LogPrint (eLogError, "I2PControl: can't create RSA key for certificate");
		}
		EVP_PKEY_free (pkey);
	}
}
}
