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
#include <map> 
#include "TSConstants.h"

#include "Interfaces.h" 

using namespace std;
using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      ref class HLSController;
      ref class HLSVariantStream;
      class Playlist;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSPlaylist sealed : public IHLSPlaylist
      {
      private:
        HLSController^ _controller;
        unique_ptr<unsigned int> _bitratekey;
        std::vector<HLSVariantStream^> _variants;


        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ _BitrateSwitchSuggested;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ _BitrateSwitchCancelled;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ _BitrateSwitchCompleted;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>^ _StreamSelectionChanged;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>^ _SegmentSwitched;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegment^>^ _SegmentDataLoaded;
        event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSlidingWindow^>^ _SlidingWindowChanged;
      internal:

        HLSPlaylist(HLSController^ controller, bool IsVariantChild, unsigned int bitratekey);
        void RaiseSegmentDataLoaded(Playlist *from, unsigned int SequenceNumber);
        ///<summary>Raise bitrate switch suggested event</summary> 
        void RaiseSegmentSwitched(Playlist *from, Playlist *to, unsigned int fromseq, unsigned int toseq);
        ///<summary>Raise bitrate switch suggested event</summary> 
        void RaiseBitrateSwitchSuggested(unsigned int From, unsigned int To, unsigned int LastMeasured, bool& Cancel);
        ///<summary>Raise bitrate switch completed event</summary> 
        void RaiseBitrateSwitchCompleted(ContentType type, unsigned int From, unsigned int To);
        ///<summary>Raise bitrate switch cancelled event</summary> 
        void RaiseBitrateSwitchCancelled(unsigned int From, unsigned int To);
        ///<summary>Raise stream selection changed event</summary> 
        void RaiseStreamSelectionChanged(TrackType from, TrackType to);
        ///<summary>Raise sliding window changed event</summary> 
        void RaiseSlidingWindowChanged(Playlist *from);

      public:
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchSuggested
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler)
          {
            return _BitrateSwitchSuggested += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _BitrateSwitchSuggested -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args)
          { 
            return _BitrateSwitchSuggested(sender, args);
          }
        }
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchCancelled
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler)
          {
            return _BitrateSwitchCancelled += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _BitrateSwitchCancelled -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args)
          {
            return _BitrateSwitchCancelled(sender, args);
          }
        }

        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchCompleted
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler)
          {
            return _BitrateSwitchCompleted += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _BitrateSwitchCompleted -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args)
          {
            return _BitrateSwitchCompleted(sender, args);
          }
        }
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>^ StreamSelectionChanged
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>^ handler)
          {
            return _StreamSelectionChanged += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _StreamSelectionChanged -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSStreamSelectionChangedEventArgs^ args)
          {
            return _StreamSelectionChanged(sender, args);
          }
        }
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>^ SegmentSwitched
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>^ handler)
          {
            return _SegmentSwitched += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _SegmentSwitched -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSSegmentSwitchEventArgs^ args)
          {
            return _SegmentSwitched(sender, args);
          }
        }
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegment^>^ SegmentDataLoaded
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegment^>^ handler)
          {
            return _SegmentDataLoaded += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _SegmentDataLoaded -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSSegment^ args)
          {
            return _SegmentDataLoaded(sender, args);
          }
        }
        virtual event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSlidingWindow^>^ SlidingWindowChanged
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSlidingWindow^>^ handler)
          {
            return _SlidingWindowChanged += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            _SlidingWindowChanged -= token;
          }
          void raise(IHLSPlaylist^ sender, IHLSSlidingWindow^ args)
          {
            return _SlidingWindowChanged(sender, args);
          }
        }



        property bool IsMaster
        {
          virtual bool get();
        }
        property Platform::String^ Url
        {
          virtual Platform::String^ get();
        }
        property bool IsLive
        {
          virtual bool get();
        }

        
        property IHLSVariantStream^ ActiveVariantStream
        {
          virtual IHLSVariantStream^ get();
        }
        property unsigned int MaximumAllowedBitrate
        {
          virtual unsigned int get();
          virtual void set(unsigned int val);
        }
        property unsigned int MinimumAllowedBitrate
        {
          virtual unsigned int get();
          virtual void set(unsigned int val);
        }
        property unsigned int StartBitrate
        {
          virtual unsigned int get();
          virtual void set(unsigned int val);
        }
        property HLSPlaylistType PlaylistType
        {
          virtual HLSPlaylistType get();
        }
        property Windows::Foundation::TimeSpan DerivedTargetDuration
        {
          virtual Windows::Foundation::TimeSpan get();
        }
        property Windows::Foundation::TimeSpan PlaylistTargetDuration
        {
          virtual Windows::Foundation::TimeSpan get();
        }
        property unsigned int BaseSequenceNumber
        {
          virtual unsigned int get();
        }
        property unsigned int SegmentCount
        {
          virtual unsigned int get();
        }
        property IHLSVariantStream^ ParentStream
        {
          virtual IHLSVariantStream^ get();
        }
        property IHLSSlidingWindow^ SlidingWindow
        {
          virtual IHLSSlidingWindow^ get();
        }

        virtual Windows::Foundation::Collections::IVector<IHLSVariantStream^>^ GetVariantStreams();
        virtual IHLSSegment^ GetSegmentBySequenceNumber(unsigned int SequenceNumber);
        virtual void SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter);
        virtual void ResetPIDFilter(TrackType forType);
        virtual void ResetPIDFilter();
        virtual Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ GetPIDFilter();
        virtual void ResetBitrateLock();
        virtual Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ GetPlaylistBatchItemDurations();



      };
    }
  }
} 