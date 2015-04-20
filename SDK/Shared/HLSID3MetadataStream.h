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

#include <vector>
#include "Interfaces.h"
#include "HLSID3MetadataPayload.h" 

using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSID3MetadataStream sealed : public IHLSID3MetadataStream
      {
      private:
        unsigned int _streamID;
        Windows::Foundation::Collections::IVector<IHLSID3MetadataPayload^>^ _payloads;

      internal:

        HLSID3MetadataStream(unsigned int streamID, std::vector<HLSID3MetadataPayload^> payloads) : _streamID(streamID)
        {
          if (payloads.size() == 0)
            _payloads = nullptr;
          else
          {
            _payloads = ref new Platform::Collections::Vector<IHLSID3MetadataPayload^>((unsigned int)payloads.size());
            std::copy(begin(payloads), end(payloads), begin(_payloads));
          }
        }

      public:
       

        property unsigned int StreamID
        {
          virtual unsigned int get()
          {
            return _streamID;
          }
        }

        virtual Windows::Foundation::Collections::IVector<IHLSID3MetadataPayload^>^ GetPayloads()
        { 
          return _payloads;
        }
      };
    }
  }
}
 