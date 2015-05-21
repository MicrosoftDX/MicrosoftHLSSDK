#pragma once
#include "pch.h"

#include <collection.h>

namespace Microsoft { namespace CC608 {

// Not for use by end-users! 
// This is a simple data-holder class for 608 caption data at a specific timestamp. It is used by the RawCaptionData component.
// This class can accept byte code data as either an IPropertyValue (with a UInt8 array) or a Vector<byte_t>.
public ref class RawCaptionDataSubset sealed
{
public:
  RawCaptionDataSubset(unsigned long long ts, const Platform::Array<byte_t>^ bytes);

  unsigned long long GetTs();
  Platform::Array<byte_t>^ GetBytes();

private:
  unsigned long long _ts;
  Platform::Array<byte_t>^ _bytes;
};


}}