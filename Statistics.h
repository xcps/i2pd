#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include <string>

#include "Profile.h"

namespace i2p
{
namespace stats
{
	class BufferedStatLog : StatLog
	{
	  public:
	  	BufferedStatLog ();
	  	~BufferedStatLog ();
		void addData(std::string scope, std::string stat, long value, long duration);
	};
}
}

#endif