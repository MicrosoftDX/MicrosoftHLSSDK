#pragma once

#include <vector>

#include "Timestamp.h"
#include "RawCaptionData.h"

namespace Microsoft { namespace CC608 {

// Very simple class used to convert from RawCaptionData^ to the internal representation
class RawCaptionDataInternal
{
public:
  RawCaptionDataInternal(RawCaptionData^ data);
  RawCaptionDataInternal(Windows::Foundation::Collections::IMap<unsigned long long, Windows::Foundation::Collections::IVector<byte>^>^ data);
  RawCaptionDataInternal(Windows::Foundation::Collections::IMap<unsigned long long, const Platform::Array<byte>^>^ data);

  ~RawCaptionDataInternal(void);

  // public data member
  std::vector<std::pair<Timestamp, std::vector<byte_t>>> Data;
};


}}