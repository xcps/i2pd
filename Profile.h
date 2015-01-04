#ifndef __PROFILE_H__
#define __PROFILE_H__

#include <string>
#include <map>
#include <vector>

#include "Identity.h"
#include "NetDb.h"
#include "RouterInfo.h"

using namespace i2p::data;

namespace i2p
{
    namespace stats
    {
        // zzz.i2p/topics/969
        // List created based on the Press Freedom Index. Those countries with a score of higher than 50 are included:
        // http://en.wikipedia.org/wiki/Press_Freedom_Index
        // Except (quote):
        // I don't really think that is usage of I2P is dangerous in countries from CIS
        // General situation is really bad (like in Russia) but people here doesn't have problems with Ecnryption usage.
        enum badCountries
        {
            /* Afghanistan */ AF = 0x01,
            /* Bahrain */ BH = 0x02,
            /* Brunei */ BN = 0x03,
            /* Burma */ MM = 0x04,
            /* China */ CN = 0x05,
            /* Colombia */ CO = 0x06,
            /* Cuba */ CU = 0x07,
            /* Democratic Republic of the Congo */ CD = 0x08,
            /* Equatorial Guinea */ GQ = 0x09,
            /* Eritrea */ ER = 0x0A,
            /* Ethiopia */ ET = 0x0B,
            /* Fiji */ FJ = 0x0C,
            /* Honduras */ HN = 0x0D,
            /* Iran */ IR = 0x0E,
            /* Laos */ LA = 0x0F,
            /* Libya */ LY = 0x10,
            /* Malaysia */ MY = 0x11,
            /* Nigeria */ NG = 0x12,
            /* North Korea */ KP = 0x13,
            /* Pakistan */ PK = 0x14,
            /* Palestinian Territories */ PS = 0x15,
            /* Philippines */ PH = 0x16,
            /* Rwanda */ RW = 0x17,
            /* Saudi Arabia */ SA = 0x18,
            /* Somalia */ SO = 0x19,
            /* Sri Lanka */ LK = 0x1A,
            /* Sudan */ SD = 0x1B,
            /* Swaziland */ SZ = 0x1C,
            /* Syria */ SY = 0x1D,
            /* Thailand */ TH = 0x1E,
            /* Tunisia */ TN = 0x1F,
            /* Vietnam */ VN = 0x20,
            /* Yemen */ YE = 0x21
        };

        /**
         *  Countries with more than about a 2% share of the netdb.
         *  Only routers in these countries will use a same-country metric.
         *  Yes this is an arbitrary cutoff.
         */
        enum bigCountries
        {
            FR = 0x01,
            DE = 0x02,
            RU = 0x03,
            UA = 0x04,
            US = 0x05
        };

        class StatLog
        {
          public:
            //virtual void addData(std::string scope, std::string stat, long value, long duration);
        };

        class Rate
        {
          public:
            Rate ();
            ~Rate ();
          private:
            // Currents
            long m_CurrentTotalValue;
            int m_CurrentEventCount;
            long m_CurrentTotalEventTime;

            // Lasts
            long m_LastTotalValue;
            int m_LastEventCount;
            long m_LastTotalEventTime;

            // Extremes
            long m_ExtremeTotalValue;
            int m_ExtremeEventCount;
            long m_ExtremeTotalEventTime;

            // Lifetime values
            long m_LifetimeTotalValue;
            long m_LifetimeEventCount;
            long m_LifetimeTotalEventTime;

            // Rate stats
            RateStat *m_Stat;

            long m_LastCoalesceDate;
            long m_CreationDate;
            int m_Period;
        };

        class RateStat
        {
          public:
            RateStat ();
            ~RateStat ();
          private:
            std::string m_StatName;
            std::string m_GroupName;
            std::string m_Description;

            std::vector<Rate> m_Rates;
            StatLog m_StatLog;
        };

        class TunnelHistory
        {
          public:
            TunnelHistory ();
            ~TunnelHistory ();
          private:
            long m_LifetimeAgreedTo;
            long m_LifetimeRejected;
            long m_LifetimeFailed;

            long m_LastAgreedTo;
            long m_LastRejectedCritical;
            long m_LastRejectedBandwidth;
            long m_LastRejectedTransient;
            long m_LastRejectedProbabalistic;
            long m_LastFailed;

            RateStat m_RejectRate;
            RateStat m_FailRate;
            std::string m_StatGroup;
        };

        class DBHistory
        {
          public:
            DBHistory ();
            ~DBHistory ();
          private:
            RateStat m_FailedLookupRate;
            RateStat m_InvalidReplyRate;
            // Lookup stats
            long m_SuccessfulLookups;
            long m_FailedLookups;
            long m_LookupReplyNew;
            long m_LookupReplyOld;
            long m_LookupReplyDuplicate;
            long m_LookupReplyInvalid;
            long m_LookupsReceived;

            long m_AvgDelayBetweenLookupsReceived;
            long m_LastLookupReceived;
            long m_LastLookupSuccessful;
            long m_LastLookupFailed;

            long m_LastStoreSuccessful;
            long m_LastStoreFailed;
            long m_UnpromptedDbStoreNew;
            long m_UnpromptedDbStoreOld;
            std::string m_StatGroup;
        };

        class RouterProfile
        {
          public:
            RouterProfile (i2p::data::IdentHash& identHash);
            ~RouterProfile ();
          private:
            // Time stats (Epoch format)
            long m_FirstHeardAbout;
            long m_LastHeardAbout;
            long m_LastSentToSuccessfully;
            long m_LastFailedSend;
            long m_LastHeardFrom;
            long m_TunnelTestResponseTimeAvg;

            // Rate stats
            RateStat m_DbResponseTime;
            RateStat m_TunnelCreateResponseTime;
            RateStat m_TunnelTestResponseTime;
            RateStat m_DbIntroduction;

            // calculation bonuses
            long m_SpeedBonus;
            long m_CapacityBonus;
            long m_IntegrationBonus;

            // Speed stats
            long m_SpeedValue;
            long m_CapacityValue;
            long m_IntegrationValue;
            bool m_IsFailing;
            // new calculation values, to be updated
            long m_SpeedValueNew;
            long m_CapacityValueNew;
            // are we in coalescing state?
            bool m_Coalescing;
            // good vs bad behavior
            TunnelHistory m_TunnelHistory;
            DBHistory m_DbHistory;
            // does this peer profile contain expanded data, or just the basics?
            bool m_Expanded;
            bool m_ExpandedDB;
            //private int _consecutiveBanlists;
            int m_Distance;
            const i2p::data::IdentHash m_Peer;

          public:
            int getXORDistance () { return m_Distance; }
            const i2p::data::IdentHash& getPeer() { return m_Peer; }
            bool getIsExpanded() { return m_Expanded; }
            bool getIsExpandedDB() { return m_ExpandedDB; }
            bool isEstablished();
            bool wasUnreachable();
            bool isSameCountry();
            bool getIsActive(long period);

            /**
             *  When did we first hear about this peer?
             *  Currently unused, candidate for removal.
             */
            long getFirstHeardAbout() { return m_FirstHeardAbout; }
            void setFirstHeardAbout(long when) { m_FirstHeardAbout = when; }

            /** when did we last hear about this peer? */
            long getLastHeardAbout() { return m_LastHeardAbout; }
            void setLastHeardAbout(long when) { m_LastHeardAbout = when; }
    
            /** when did we last send to this peer successfully? */
            long getLastSendSuccessful() { return m_LastSentToSuccessfully; }
            void setLastSendSuccessful(long when) { m_LastSentToSuccessfully = when; }
    
            /** when did we last have a problem sending to this peer? */
            long getLastSendFailed() { return m_LastFailedSend; }
            void setLastSendFailed(long when) { m_LastFailedSend = when; }
    
            /** when did we last hear from the peer? */
            long getLastHeardFrom() { return m_LastHeardFrom; }
            void setLastHeardFrom(long when) { m_LastHeardFrom = when; }
    
            /** history of tunnel activity with the peer
                Warning - may return null if !getIsExpanded() */
            TunnelHistory getTunnelHistory() { return m_TunnelHistory; }
            void setTunnelHistory(TunnelHistory history) { m_TunnelHistory = history; }
    
            /** history of db activity with the peer
                Warning - may return null if !getIsExpandedDB() */
            DBHistory getDBHistory() { return m_DbHistory; }
            void setDBHistory(DBHistory hist) { m_DbHistory = hist; }

            /** how large successfully sent messages are, calculated over a 1 minute, 1 hour, and 1 day period */
            //public RateStat getSendSuccessSize() { return _sendSuccessSize; }
            /** how large received messages are, calculated over a 1 minute, 1 hour, and 1 day period */
            //public RateStat getReceiveSize() { return _receiveSize; }
            /** how long it takes to get a db response from the peer (in milliseconds), calculated over a 1 minute, 1 hour, and 1 day period
                Warning - may return null if !getIsExpandedDB() */
            RateStat getDbResponseTime() { return m_DbResponseTime; }
            /** how long it takes to get a tunnel create response from the peer (in milliseconds), calculated over a 1 minute, 1 hour, and 1 day period
                Warning - may return null if !getIsExpanded() */
            RateStat getTunnelCreateResponseTime() { return m_TunnelCreateResponseTime; }
            /** how long it takes to successfully test a tunnel this peer participates in (in milliseconds), calculated over a 10 minute, 1 hour, and 1 day period
                Warning - may return null if !getIsExpanded() */
            RateStat getTunnelTestResponseTime() { return m_TunnelTestResponseTime; }
            /** how many new peers we get from dbSearchReplyMessages or dbStore messages, calculated over a 1 hour, 1 day, and 1 week period
                Warning - may return null if !getIsExpandedDB() */
            RateStat getDbIntroduction() { return m_DbIntroduction; }

            /**
             * extra factor added to the speed ranking - this can be updated in the profile
             * written to disk to affect how the algorithm ranks speed.  Negative values are
             * penalties
             */
            long getSpeedBonus() { return m_SpeedBonus; }
            void setSpeedBonus(long bonus) { m_SpeedBonus = bonus; }
    
            /**
             * extra factor added to the capacity ranking - this can be updated in the profile
             * written to disk to affect how the algorithm ranks capacity.  Negative values are
             * penalties
             */
            long getCapacityBonus() { return m_CapacityBonus; }
            void setCapacityBonus(long bonus) { m_CapacityBonus = bonus; }
    
            /**
             * extra factor added to the integration ranking - this can be updated in the profile
             * written to disk to affect how the algorithm ranks integration.  Negative values are
             * penalties
             */
            long getIntegrationBonus() { return m_IntegrationBonus; }
            void setIntegrationBonus(long bonus) { m_IntegrationBonus = bonus; }
    
            /**
             * How fast is the peer, taking into consideration both throughput and latency.
             * This may even be made to take into consideration current rates vs. estimated
             * (or measured) max rates, allowing this speed to reflect the speed /available/.
             *
             */
            long getSpeedValue() { return m_SpeedValue; }
            /**
             * How many tunnels do we think this peer can handle over the next hour? 
             *
             */
            long getCapacityValue() { return m_CapacityValue; }
            /**
             * How well integrated into the network is this peer (as measured by how much they've
             * told us that we didn't already know).  Higher numbers means better integrated
             *
             */
            long getIntegrationValue() { return m_IntegrationValue; }
            /**
             * is this peer actively failing (aka not worth touching)?
             * deprecated - unused - always false
             */
            bool getIsFailing() { return m_IsFailing; }

            long getTunnelTestTimeAverage() { return m_TunnelTestResponseTimeAvg; }
            void setTunnelTestTimeAverage(long avg) { m_TunnelTestResponseTimeAvg = avg; }
        };

        class Profiler
        {
          public:
            Profiler ();
            ~Profiler ();

          private:
            std::map<i2p::data::IdentHash,RouterProfile> m_RouterProfiles;
        };
    }
}


#endif
