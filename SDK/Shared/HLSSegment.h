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
#include <vector>
#include <memory> 
#include "Interfaces.h" 

using namespace Windows::Foundation::Collections;
using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      ref class HLSController;
      ref class HLSPlaylist;
      ref class HLSInbandCCPayload;
      ref class HLSID3MetadataPayload;

      class MediaSegment;
      class Rendition;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSSegment sealed : public IHLSSegment
      {
        friend ref class HLSPlaylist;

      private:
        HLSController^ _controller;
        HLSPlaylist^ _playlist;
        unsigned int _forBitrate;
        unsigned int _sequenceNumber;
        TrackType _mediaType;
        std::wstring _url;
        SegmentState _segmentState;
        IVector<IHLSInbandCCPayload^>^ _ccpayloads;
        IVector<IHLSID3MetadataStream^>^ _metadatastreams;
        IVector<Platform::String^>^ _unprocessedTagsPRE, ^_unprocessedTagsPOST, ^_unprocessedTagsWITHIN;

        static std::shared_ptr<MediaSegment> FindMatch(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum);
        static std::shared_ptr<MediaSegment> FindMatch(HLSController^ controller, Rendition* pRendition, unsigned int seqNum);
        void LoadCC(std::shared_ptr<MediaSegment> found);
        void LoadCC(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum);
        void LoadMetadata(std::shared_ptr<MediaSegment> found);
        void LoadMetadata(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum);

      internal:

        HLSSegment(HLSController^ controller, unsigned int bitrate, std::shared_ptr<MediaSegment> segment);
       

      public:

        virtual  ~HLSSegment();

        property IHLSPlaylist^ ParentPlaylist
        {
          virtual IHLSPlaylist^ get();
        }

        property unsigned int ForVariant
        {
          virtual unsigned int get()
          {
            return _forBitrate;
          }
        }

        property TrackType MediaType
        {
          virtual TrackType get()
          {
            return _mediaType;
          }
        }

        property Platform::String^ Url
        {
          virtual Platform::String^ get()
          {
            return ref new Platform::String(_url.data());
          }
        }

        property unsigned int SequenceNumber
        {
          virtual unsigned int get()
          {
            return _sequenceNumber;
          }
        }

        property SegmentState LoadState
        {
          virtual SegmentState get();
        }

        virtual IVector<IHLSInbandCCPayload^>^ GetInbandCCUnits();

        virtual IVector<IHLSID3MetadataStream^>^ GetMetadataStreams();

        virtual IVector<Platform::String^>^ GetUnprocessedTags(UnprocessedTagPlacement placement);

        virtual Windows::Foundation::IAsyncAction^ SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter);
        virtual Windows::Foundation::IAsyncAction^ ResetPIDFilter(TrackType forType);
        virtual Windows::Foundation::IAsyncAction^ ResetPIDFilter();
        virtual Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ GetPIDFilter();
      };

    }
  }
} 