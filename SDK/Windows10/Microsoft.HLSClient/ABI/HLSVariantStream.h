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

#include <memory>
#include <vector>
#include "Interfaces.h"  

using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      //forward declare
      ref class HLSController;
      ref class HLSPlaylist;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSVariantStream sealed : public IHLSVariantStream
      {
      private:
        HLSController^ _controller;
        HLSPlaylist^ _playlist;
        unsigned int _index;
        unsigned int _bitratekey;
        Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ _audioRenditions, ^_videoRenditions;
        Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ _subtitleRenditions;

      internal:
        HLSVariantStream(HLSController^ controller, unsigned int index, bool IsIndexBitrate);


      public:


        property unsigned int Bitrate
        {
          virtual unsigned int get() {
            if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
            return _bitratekey;
          }
        }
        property bool IsActive
        {
          virtual bool get();
        }
        property bool HasResolution
        {
          virtual bool get();
        }
        property unsigned int HorizontalResolution
        {
          virtual unsigned int get();
        }
        property unsigned int VerticalResolution
        {
          virtual unsigned int get();
        }
        property Platform::String^ Url
        {
          virtual Platform::String^ get();
        }
        property IHLSPlaylist^ Playlist
        {
          virtual IHLSPlaylist^ get();
        }

        virtual Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetAudioRenditions();
        virtual Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetVideoRenditions();
        virtual Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetSubtitleRenditions();
        virtual Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ GetPlaylistBatchItemDurations();
        virtual void LockBitrate();

      };
    }
  }
} 