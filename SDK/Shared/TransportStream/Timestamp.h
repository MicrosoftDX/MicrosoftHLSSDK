/*********************************************************************************************************************
Microsft HLS SDK for Windows

Copyright (c) Microsoft Corporation

All rights reserved.

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy,
modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***********************************************************************************************************************/

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