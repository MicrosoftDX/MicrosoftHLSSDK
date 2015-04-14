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

      ref class HLSController;
      ref class HLSSubtitleRendition;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSAlternateRendition sealed: public IHLSAlternateRendition
      {
        friend ref class HLSSubtitleRendition;
      private:
        HLSController^ _controller;
        unsigned int _index;
        unsigned int _bitratekey;
        std::wstring _renditiontype;
      internal:
        HLSAlternateRendition(HLSController^ controller, unsigned int idx, const std::wstring renditionType, unsigned int ForBandwidth);
      public: 

        property bool IsDefault
        {
          virtual bool get();
        }
        property bool IsAutoSelect
        {
          virtual bool get();
        }
        property bool IsForced
        {
          virtual bool get();
        }
        property Platform::String^ Url
        {
          virtual Platform::String^ get();
        }
        property Platform::String^ Type
        {
          virtual Platform::String^ get();
        }
        property Platform::String^ GroupID
        {
          virtual Platform::String^ get();
        }
        property Platform::String^ Language
        {
          virtual Platform::String^ get();
        }
        property Platform::String^ Name
        {
          virtual Platform::String^ get();
        }

        property bool IsActive
        {
          virtual bool get();
          virtual void set(bool val);
        }
      };

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      ref class HLSSubtitleRendition sealed: public IHLSAlternateRendition, public IHLSSubtitleRendition
      {
      private:
        HLSAlternateRendition^ _internalRenditionImpl;
      internal:
        HLSSubtitleRendition(HLSController^ controller, unsigned int idx, unsigned int ForBandwidth);
      public:
       
        property bool IsDefault
        {
          virtual bool get() { return _internalRenditionImpl->IsDefault; }
        }
        property bool IsAutoSelect
        {
          virtual bool get() { return _internalRenditionImpl->IsAutoSelect; }
        }
        property bool IsForced
        {
          virtual bool get() { return _internalRenditionImpl->IsForced; }
        }
        property Platform::String^ Url
        {
          virtual Platform::String^ get() { return _internalRenditionImpl->Url; }
        }
        property Platform::String^ Type
        {
          virtual Platform::String^ get()  { return _internalRenditionImpl->Type; }
        }
        property Platform::String^ GroupID
        {
          virtual Platform::String^ get() { return _internalRenditionImpl->GroupID; }
        }
        property Platform::String^ Language
        {
          virtual Platform::String^ get() { return _internalRenditionImpl->Language; }
        }
        property Platform::String^ Name
        {
          virtual Platform::String^ get() { return _internalRenditionImpl->Name; }
        }

        property bool IsActive
        {
          virtual bool get() { return _internalRenditionImpl->IsActive; }
          virtual void set(bool val) { _internalRenditionImpl->IsActive = val; }
        }

        virtual Windows::Foundation::IAsyncOperation<unsigned int>^ RefreshAsync();
        virtual Windows::Foundation::Collections::IVector<IHLSSubtitleLocator^>^ GetSubtitleLocators();
      };
    }
  }
}
 