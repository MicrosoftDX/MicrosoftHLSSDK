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

#include <vector>
#include <tuple>
#include <collection.h>
#include "Utilities\ID3TagParser.h"  
#include "HLSID3MetadataPayload.h"
#include "HLSID3TagFrame.h"

using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

HLSID3MetadataPayload::HLSID3MetadataPayload(unsigned long long timestamp, vector<tuple<const BYTE*, unsigned int>>& payloadchunks) :_timestamp(timestamp), _parsedFrames(nullptr)
{
  for (auto itr = payloadchunks.begin(); itr != payloadchunks.end(); itr++)
  {
    auto data = std::get<0>(*itr);
    auto len = std::get<1>(*itr);
    if (len == 0)
      continue;

    auto oldsize = _payload.size();
    _payload.resize(_payload.size() + len);
    memcpy_s(&(*(_payload.begin() + oldsize)), len, data, len);
  }

  //validate for  ID3 and readjust
  ULONG correctsize = 0;
  ULONG startat = 0;
  bool valid = ID3TagParser::AdjustPayloadToID3Tag(&(*(this->_payload.begin())), (ULONG)this->_payload.size(), startat, correctsize);
  if (!valid)
    throw ref new Platform::InvalidArgumentException();

  if (startat > 0)
  {
    for (ULONG i = 0; i < startat; i++)
      this->_payload.erase(this->_payload.begin());
  }
  if (correctsize > _payload.size())
    throw ref new Platform::InvalidArgumentException();
  else if (correctsize < _payload.size())
    _payload.resize(correctsize);
}

Windows::Foundation::Collections::IVector<IHLSID3TagFrame^>^ HLSID3MetadataPayload::ParseFrames()
{
  if (_parsedFrames == nullptr && _payload.size() > 0)
  {
    auto frames = ID3TagParser::Parse(&(*(_payload.begin())), (ULONG) _payload.size());
    if (frames.size() > 0)
    { 
      _parsedFrames = ref new Platform::Collections::Vector<IHLSID3TagFrame^>((unsigned int)frames.size());

      std::transform(begin(frames), end(frames), begin(_parsedFrames), [](std::shared_ptr<std::tuple<string, vector<BYTE>>> frame)
      {
        string frameid = std::get<0>(*(frame.get()));
        unique_ptr<wchar_t[]> wzframeid(new wchar_t[9]);
        ZeroMemory(wzframeid.get(), 9);
        ::MultiByteToWideChar(CP_UTF8, 0, frameid.c_str(), (int) frameid.size() + 1/*for null term*/, wzframeid.get(), 9);

        return ref new HLSID3TagFrame(wstring(wzframeid.get()), std::get<1>(*(frame.get())));
      });
    }
  }

  return _parsedFrames;
}