#if !defined _PESPacket_h
#define _PESPacket_h

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

#endif