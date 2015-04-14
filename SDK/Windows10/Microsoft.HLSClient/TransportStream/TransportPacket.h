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
#include <map>
#include <vector> 
#include "Timestamp.h"
#include "PESPacket.h"
#include "PATSection.h"
#include "PMTSection.h"
#include "AdaptationField.h"
#include "SampleData.h"
#include "TransportStreamParser.h"
#include "Utilities\BitOp.h"
#include "TSConstants.h" 

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      class TransportStreamParser;
      class AdaptationField;
      class PATSection;
      class PMTSection;
      class PESPacket;
      class Timestamp;
      class SampleData;

      class TransportPacket
      {

      public:
        bool IsPATSection;
        bool IsPMTSection;
        bool HasPayload;
        bool HasAdaptationField;
        BYTE PayloadUnitStartIndicator;
        USHORT PID;
        BYTE ContinuityCounter;
        unsigned int PayloadLength;
        ContentType MediaType;
        std::shared_ptr<AdaptationField> spAdaptationField;
        std::shared_ptr<PATSection> spPATSection;
        std::shared_ptr<PMTSection> spPMTSection;
        std::shared_ptr<PESPacket> spPESPacket;

        TransportStreamParser *pParentParser;
        TransportPacket(TransportStreamParser *parent);
        ~TransportPacket();
        static bool IsTransportPacket(const BYTE* tsPacket);
        static const shared_ptr<TransportPacket> Parse(const BYTE* tsPacket,
          TransportStreamParser *parent,
          std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
          std::map<ContentType, unsigned short>& MediaTypePIDMap,
          std::map<ContentType, unsigned short> PIDFilter,
          std::vector<unsigned short>& MetadataStreams,
          std::vector<std::shared_ptr<Timestamp>>& Timeline,
          std::vector<shared_ptr<SampleData>>& CCSamples , bool ParsingOutOfOrder = false);

      };

    }
  }
} 