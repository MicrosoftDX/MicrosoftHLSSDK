#include "pch.h"
#include "RawCaptionDataSubset.h"


using namespace Microsoft::CC608;

RawCaptionDataSubset::RawCaptionDataSubset(unsigned long long ts, const Platform::Array<byte_t>^ bytes) : _ts(ts), _bytes(ref new Platform::Array<byte_t>(bytes->Data, bytes->Length))
{
}


unsigned long long RawCaptionDataSubset::GetTs()
{
  return _ts;
}

Platform::Array<byte_t>^ RawCaptionDataSubset::GetBytes()
{
  return _bytes;
}
