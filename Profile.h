#ifdef __PROFILE_H__
#define __PROFILE_H__

#include <string>
#include <map>

namespace i2p
{
	namespace stats
	{
		class RouterProfile
		{
		  public:
		  	RouterProfile ();
		  	~RouterProfile ();
		  private:
		  	// Time stats (Epoch format)
		  	long m_FirstHeardAbout;
		  	long m_LastHeardAbout;
		  	long m_lastSentToSuccessfully;
		  	long m_lastFailedSend;
		  	long m_lastHeardFrom;
		  	long m_tunnelTestResponseTimeAvg;
		};

		class Profiler
		{
		  public:
			Profiler ();
			~Profiler ();

		  private:
		  	// String would be GetIdentHashBase64 from RIs
		  	std::map<std::string,RouterProfile> m_RouterProfiles;
		};
	}
}


#endif
