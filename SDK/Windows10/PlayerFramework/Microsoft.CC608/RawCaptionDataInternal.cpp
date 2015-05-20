#include "pch.h"
#include "RawCaptionDataInternal.h"
#include "RawCaptionDataSubset.h"

using namespace Microsoft::CC608;

using namespace Windows::Foundation::Collections;
using namespace Platform;

using namespace std;

RawCaptionDataInternal::RawCaptionDataInternal(RawCaptionData^ data)
{
  IVector<RawCaptionDataSubset^>^ v = data->GetData();

  unsigned int size = v->Size;

  for (unsigned int i = 0; i != size; ++i)
  {
    RawCaptionDataSubset^ s = v->GetAt(i);

    unsigned long long ts = s->GetTs();
    auto bytes = s->GetBytes();

    // now copy the byte codes to an std::vector
    auto bytesInStdVector = vector<byte_t>();
    for(auto b : bytes)
    {
      bytesInStdVector.push_back(b);
    }

    // store the data
    Data.push_back(make_pair(Timestamp(ts), bytesInStdVector));
  }
}

RawCaptionDataInternal::RawCaptionDataInternal(Windows::Foundation::Collections::IMap<unsigned long long, Windows::Foundation::Collections::IVector<byte>^>^ data)
{
  for (auto iter = data->First(); iter->HasCurrent; iter->MoveNext())
  {
    auto item = iter->Current;

    unsigned long long ts = item->Key;
    IVector<byte_t>^ bytes = item->Value;

    auto bytesInStdVector = std::vector<byte_t>();

    unsigned int size = bytes->Size;
    for(unsigned int i = 0; i != size; ++i)
    {
      bytesInStdVector.push_back(bytes->GetAt(i));
    }

    Data.push_back(make_pair(Timestamp(ts), bytesInStdVector));
  }
}

RawCaptionDataInternal::RawCaptionDataInternal(Windows::Foundation::Collections::IMap<unsigned long long, const Platform::Array<byte>^>^ data)
{
  for (auto iter = data->First(); iter->HasCurrent; iter->MoveNext())
  {
    auto item = iter->Current;

    unsigned long long ts = item->Key;
    auto bytes = item->Value;

    auto bytesInStdVector = std::vector<byte_t>();

    for(const auto& b : bytes)
    {
      bytesInStdVector.push_back(b);
    }

    Data.push_back(make_pair(Timestamp(ts), bytesInStdVector));
  }
}

RawCaptionDataInternal::~RawCaptionDataInternal(void)
{
}
