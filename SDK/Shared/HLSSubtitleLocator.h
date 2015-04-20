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

#include <string>
#include "Interfaces.h" 

using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSSubtitleLocator sealed : public IHLSSubtitleLocator
      {
      private:
        std::wstring _location;
        float _duration, _startposition;
        unsigned int _index;

      internal:
        HLSSubtitleLocator(std::wstring location, unsigned int index, float duration, float startposition) :
          _location(location), _duration(duration), _startposition(startposition), _index(index)
        {
        }


      public:


        property Platform::String^ Location
        {
          virtual Platform::String^ get() { return ref new Platform::String(_location.data()); }
        }
        property float Duration
        {
          virtual float get() { return _duration; };
        }
        property float StartPosition
        {
          virtual float get() { return _startposition; };
        }
        property unsigned int Index
        {
          virtual unsigned int get() { return _index; };
        }
      };

    }
  }
} 