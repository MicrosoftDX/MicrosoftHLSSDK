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
#include <tuple>
#include "Timestamp.h"


using namespace std;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class SampleData
      {
      public:
        std::vector<tuple<const BYTE*, unsigned int>> elemData;
        shared_ptr<tuple<shared_ptr<BYTE>, unsigned int>> spInBandCC;
        bool IsSampleIDR;
        std::shared_ptr<Timestamp> SamplePTS;
        std::shared_ptr<Timestamp> DiscontinousTS;
        unsigned int TotalLen;
        bool CCRead;
        bool IsTick;
        bool ForceSampleDiscontinuity;
        unsigned int Index;
        SampleData() : IsSampleIDR(false), TotalLen(0), CCRead(false), DiscontinousTS(nullptr), IsTick(false), ForceSampleDiscontinuity(false),Index(0){}

        SampleData(SampleData&& src) : 
          CCRead(src.CCRead), IsTick(src.IsTick), TotalLen(src.TotalLen), 
          IsSampleIDR(src.IsSampleIDR), SamplePTS(src.SamplePTS), 
          DiscontinousTS(src.DiscontinousTS)
        {
          elemData = std::move(src.elemData);
          spInBandCC = std::move(src.spInBandCC);
        }

        ~SampleData()
        {
          elemData.clear();
          SamplePTS.reset();
          spInBandCC.reset();
        }

        void SetCCRead(bool val)
        {
          CCRead = val;
        }

        bool GetCCRead()
        {
          return CCRead;
        }

      };

    }
  }
} 