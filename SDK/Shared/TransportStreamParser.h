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
#include <deque> 
#include <tuple>
#include <map>
#include <vector>
#include <memory>
#include <algorithm>
#include <wtypes.h>
#include "SampleData.h"
#include "PESPacket.h"
#include "TransportPacket.h"
#include "PATSection.h"
#include "PMTSection.h" 
#include "FileLogger.h" 
#include "TSConstants.h"
#include "TransportPacket.h" 
#include "AVCParser.h"

using namespace std;

namespace Microsoft {
  namespace HLSClient {
    namespace Private { 

      class PATSection;
      class PMTSection;
      class AdaptationField;
      class PESPacket;
      class TransportPacket;
      class TransportStreamParser;
      class Timestamp;
      class SampleData;

      class TransportStreamParser
      {
        friend class AdaptationField;
        friend class MediaSegment;
        friend class PESPacket;
        friend class TransportPacket;
      private:
        
        std::map<unsigned short, std::vector<std::shared_ptr<TransportPacket>>> SampleBuilder;
        std::map<unsigned short, unsigned short> PAT;
        std::map<unsigned short, BYTE> PMT;
        bool HasPCR;
        unsigned short PCRPID;
        std::vector<const BYTE*> OutOfOrderTSP;
        static bool IsTransportStream(const BYTE *tsd);
        AVCParser avcparser;

        void Parse(const BYTE *tsdata, ULONG size,
          std::map<ContentType, unsigned short>& MediaTypePIDMap,
          std::map<ContentType, unsigned short> PIDFilter,
          std::vector<unsigned short>& MetadataStreams,
          std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
          std::vector<std::shared_ptr<Timestamp>>& Timeline,
          std::vector<shared_ptr<SampleData>>& CCSamples );



        HRESULT BuildSample(unsigned short PID,
          std::map<ContentType, unsigned short>& MediaTypePIDMap,
          std::vector<unsigned short>& MetadataStreams,
          std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
          std::vector<shared_ptr<SampleData>>& CCSamples);

        void Clear()
        {
          SampleBuilder.clear();
          PAT.clear();
          PMT.clear(); 
        }
      public:
        TransportStreamParser();
        ~TransportStreamParser()
        {
          Clear();
        }

      };
    }
  }
} 