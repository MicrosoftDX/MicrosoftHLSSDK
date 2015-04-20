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

#include "pch.h"
#include <wtypes.h>
#include <vector>
#include <map>
#include <memory>
#include "TSConstants.h" 

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      class TransportPacket;
      class Timestamp;

      class PESPacket
      {

      private:
        //TransportPacket *pParentTSP;
      public:
        bool HasHeader;
        BYTE StreamID;
        bool HasElementaryStreamRate;
        unsigned int ElementaryStreamRate;
        unsigned short PacketDataLength;
        std::shared_ptr<Timestamp> PresentationTimestamp, DecoderTimestamp;
        PESPacket();
        ~PESPacket();
        static const std::shared_ptr<PESPacket> Parse(const BYTE *pesdata,
          unsigned short NumBytes,
          //shared_ptr<TransportPacket> pParent,
          TransportPacket * pParent,
          std::map<ContentType, unsigned short>& MediaTypePIDMap,
          std::map<ContentType, unsigned short> PIDFilter,
          std::vector<unsigned short>& MetadataStreams,
          std::vector<std::shared_ptr<Timestamp>>& Timeline);
        ContentType GetContentType(TransportPacket *pParent, std::map<ContentType, unsigned short> PIDFilter);
        const BYTE *upPacketData;
      };
    }
  }
} 