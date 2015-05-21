#include "pch.h"

#include "RawCaptionData.h"

using namespace Microsoft::CC608;

RawCaptionData::RawCaptionData(void) : _vector(ref new Platform::Collections::Vector<RawCaptionDataSubset^>())
{
}

void RawCaptionData::AddByArray(unsigned long long ts, const Platform::Array<byte_t>^ bytes)
{
  if (bytes->Length == 0)
  {
    // no usable data here
    return;
  }

  RawCaptionDataSubset^ s = ref new RawCaptionDataSubset(ts, bytes);
  _vector->Append(s);
}

Windows::Foundation::Collections::IVector<RawCaptionDataSubset^>^ RawCaptionData::GetData()
{
  return _vector;
}

