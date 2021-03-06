#ifndef UTIL_H
#define UTIL_H

#include <map>
#include <string>
#include <iostream>
#include <boost/asio.hpp>

namespace i2p
{
namespace util
{
	namespace http
	{
		// in (lower case)
		const char ETAG[] = "etag"; // ETag
		const char LAST_MODIFIED[] = "last-modified"; // Last-Modified
		const char TRANSFER_ENCODING[] = "transfer-encoding"; // Transfer-Encoding
		const char CONTENT_ENCODING[] = "content-encoding"; // Content-Encoding
		// out
		const char IF_NONE_MATCH[] = "If-None-Match";
		const char IF_MODIFIED_SINCE[] = "If-Modified-Since";	
	
		std::string GetHttpContent (std::istream& response);
		void MergeChunkedResponse (std::istream& response, std::ostream& merged);
		std::string urlDecode(const std::string& data);
		
		struct url {
    			url(const std::string& url_s); // omitted copy, ==, accessors, ...
			private:
    			void parse(const std::string& url_s);
			public:
				std::string protocol_, host_, path_, query_;
				std::string portstr_;
				unsigned int port_;
				std::string user_;
				std::string pass_;
		};
	}

	namespace net
	{
		int GetMTU (const boost::asio::ip::address& localAddress);
	}
}
}


#endif
