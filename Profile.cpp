#include "Profile.h"
#include "RouterInfo.h"
#include "NetDb.h"
#include "Identity.h"

namespace i2p
{
namespace stats
{

	Rate::Rate ()
	{
	}

	Rate::~Rate ()
	{
	}

	RateStat::RateStat ()
	{
	}

	RateStat::~RateStat ()
	{
	}

	TunnelHistory::TunnelHistory ()
	{
	}

	TunnelHistory::~TunnelHistory ()
	{
	}

	DBHistory::DBHistory ()
	{
	}

	DBHistory::~DBHistory ()
	{
	}

	RouterProfile::RouterProfile (i2p::data::IdentHash& identHash) : m_Peer(identHash)
	{
	}

	RouterProfile::~RouterProfile ()
	{
	}

	Profiler::Profiler ()
	{
	}


} // Namespace
} // Namespace
