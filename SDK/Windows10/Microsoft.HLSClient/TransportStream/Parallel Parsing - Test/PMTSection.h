#if !defined _PMTSection_h
#define _PMTSection_h

#pragma once
#include <map>
#include <wtypes.h>
#include "Utilities\BitOp.h"

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      class PMTSection
      {
      public:
        unsigned short CurrentSectionNumber;
        unsigned short MaxSectionNumber;
        std::map<unsigned short, BYTE> MapData;
        bool HasPCR;
        unsigned int PCRPID;
        PMTSection();
        ~PMTSection();
        static bool IsPMTSection(const BYTE *pesdata);
        static std::shared_ptr<PMTSection> Parse(const BYTE *pesdata);
        static bool IsHLSTimedMetadata(const BYTE * es_info_loop, unsigned int length);
      };

    }
  }
}

#endif