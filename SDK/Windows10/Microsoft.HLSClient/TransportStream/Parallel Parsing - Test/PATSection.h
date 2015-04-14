#if !defined _PATSection_h
#define _PATSection_h

#pragma once

#include <map>
#include <memory>
#include "Utilities\BitOp.h"

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      class PATSection
      {
      public:
        unsigned short CurrentSectionNumber;
        unsigned short MaxSectionNumber;
        std::map<unsigned short, unsigned short> AssociationData;

        PATSection();
        ~PATSection();
        static bool IsPATSection(const BYTE *pesdata);
        static std::shared_ptr<PATSection> Parse(const BYTE *pesdata);
      };

    }
  }
}

#endif