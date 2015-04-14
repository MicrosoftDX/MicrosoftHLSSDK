#if !defined _Timestamp_h
#define _Timestamp_h

#pragma once
#include <memory>
#include "Utilities\BitOp.h"
#include "TSConstants.h"

using namespace std;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class Timestamp
      {
      public:
        TimestampType Type;
        unsigned long long ValueInTicks;
        Timestamp();
        Timestamp(unsigned long long value, TimestampType type = TimestampType::PTS) : ValueInTicks(value), Type(type){ };
        static std::shared_ptr<Timestamp> Parse(const BYTE* pcrdata, TimestampType type);

        static inline unsigned long T2S(unsigned long long Ticks) { return (unsigned long) Ticks / 10000000; }
        static inline unsigned long long S2T(unsigned  Seconds) { return (unsigned long long)Seconds * 10000000; }
        static inline unsigned long long T2MS(unsigned long long Ticks) { return (unsigned long long)Ticks / 10000; }
        static inline unsigned long long MS2T(unsigned long long MilliSeconds) { return MilliSeconds * 10000; }

        Timestamp(const Timestamp& src) : ValueInTicks(src.ValueInTicks), Type(src.Type)
        {
           
        }
        Timestamp(Timestamp&& src) : ValueInTicks(src.ValueInTicks), Type(src.Type)
        {
          
        }


        Timestamp& operator=(Timestamp& src)
        {
          ValueInTicks = src.ValueInTicks;
          Type = src.Type;
          return *this;
        }
        Timestamp& operator=(Timestamp&& src)
        {
          ValueInTicks = src.ValueInTicks;
          Type = src.Type;
          return *this;
        }
        

        Timestamp& operator+=(const Timestamp& other)
        {
          ValueInTicks += other.ValueInTicks;
          return *this;
        }

        Timestamp& operator+=(unsigned long long other)
        {
          ValueInTicks += other;
          return *this;
        }

        Timestamp& operator-=(const Timestamp& other)
        {
          ValueInTicks -= other.ValueInTicks;
          return *this;
        }

        Timestamp& operator-=(unsigned long long other)
        {
          ValueInTicks -= other;
          return *this;
        }

        Timestamp operator+(const Timestamp& ts2)
        {
          return *this += ts2;
        }

        Timestamp operator+(unsigned long long ts2)
        {
          return (*this) += ts2;
        }

        Timestamp operator-(const Timestamp& ts2)
        {
          return *this -= ts2;
        }

        Timestamp operator-(unsigned long long ts2)
        {
          return *this -= ts2;
        }
      };

    }
  }
}

#endif