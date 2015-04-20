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

#include <mutex> 
#include "Interfaces.h"
#include "HLSMediaSource.h"

using namespace Windows::Media;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      ref class HLSPlaylist;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSController sealed: public IHLSController
      {
      private:
        CHLSMediaSource *_pmediaSource;
        std::wstring _id;
        std::unique_lock<std::recursive_mutex> _lockclient;
        unsigned int _prepareresrequestsubscriptioncount;
        unsigned int _initialbitrateselectedsubscriptioncount;
        bool _isPlaylistBatched;
        HLSPlaylist^ _playlist;
      internal:
        HLSController(CHLSMediaSource *pSource, std::wstring ControllerID = L"");
        HLSPlaylist^ GetPlaylist();

        property CHLSMediaSource *MediaSource
        {
          CHLSMediaSource* get()
          {
            return _pmediaSource;
          }
        }

        void Invalidate()
        {
          _pmediaSource = nullptr;
        }

        void RaisePrepareResourceRequest(ResourceType restype,
          wstring& szUrl,
          std::vector<shared_ptr<Cookie>>& cookies,
          std::map<wstring,
          wstring>& headers,
          IHLSContentDownloader^* externaldownloader);

        
        void RaiseInitialBitrateSelected();

        virtual event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSResourceRequestEventArgs^>^ _prepareResourceRequest;
        
        virtual event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSInitialBitrateSelectedEventArgs^>^ _initialBitrateSelected;
      public: 
        bool IsPrepareResourceRequestSubscribed() { return _prepareresrequestsubscriptioncount > 0; }
        bool IsInitialBitrateSelectedSubscribed() { return _initialbitrateselectedsubscriptioncount > 0; }
        virtual event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSResourceRequestEventArgs^>^ PrepareResourceRequest
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSController^, IHLSResourceRequestEventArgs^>^ handler)
          {
            InterlockedIncrement(&_prepareresrequestsubscriptioncount);
            return _prepareResourceRequest += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            InterlockedDecrement(&_prepareresrequestsubscriptioncount);
            _prepareResourceRequest -= token;
          }
          void raise(IHLSController^ sender, IHLSResourceRequestEventArgs^ args)
          {
            return _prepareResourceRequest(sender, args);
          }
        }

  

        virtual event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSInitialBitrateSelectedEventArgs^>^ InitialBitrateSelected
        {
          Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSController^, IHLSInitialBitrateSelectedEventArgs^>^ handler)
          {
            InterlockedIncrement(&_initialbitrateselectedsubscriptioncount);
            return _initialBitrateSelected += handler;
          }
          void remove(Windows::Foundation::EventRegistrationToken token)
          {
            InterlockedDecrement(&_initialbitrateselectedsubscriptioncount);
            _initialBitrateSelected -= token;
          }
          void raise(IHLSController^ sender, IHLSInitialBitrateSelectedEventArgs^ args)
          {
            return _initialBitrateSelected(sender, args);
          }
        }
 

        property bool IsValid
        {
          virtual bool get();
        }

        property Platform::String^ ID
        {
          virtual Platform::String^ get();
        }

        property TrackType TrackTypeFilter
        {
          virtual TrackType get();
          virtual void set(TrackType val);
        }
        property SegmentMatchCriterion MatchSegmentsUsing
        {
          virtual SegmentMatchCriterion get();
          virtual void set(SegmentMatchCriterion val);
        }
        property Windows::Foundation::TimeSpan MinimumBufferLength
        {
          virtual Windows::Foundation::TimeSpan get();
          virtual void set(Windows::Foundation::TimeSpan val);
        }

        property Windows::Foundation::TimeSpan MinimumLiveLatency
        {
          virtual Windows::Foundation::TimeSpan get();
          virtual void set(Windows::Foundation::TimeSpan val);
        }

        property Windows::Foundation::TimeSpan BitrateChangeNotificationInterval
        {
          virtual Windows::Foundation::TimeSpan get();
          virtual void set(Windows::Foundation::TimeSpan val);
        }

        property Windows::Foundation::TimeSpan PrefetchDuration
        {
          virtual Windows::Foundation::TimeSpan get();
          virtual void set(Windows::Foundation::TimeSpan val);
        }

        property bool EnableAdaptiveBitrateMonitor
        {
          virtual bool get();
          virtual void set(bool val);
        }


        property bool ResumeLiveFromPausedOrEarliest
        {
          virtual bool get();
          virtual void set(bool val);
        }
 
        property unsigned int SegmentTryLimitOnBitrateSwitch
        {
          virtual unsigned int get();
          virtual void set(unsigned int val);
        } 
 
        property IHLSPlaylist^ Playlist
        {
          virtual IHLSPlaylist^ get();
        }
         
        property float MinimumPaddingForBitrateUpshift
        {
          virtual float get();
          virtual void set(float val);
        }

        property float MaximumToleranceForBitrateDownshift
        {
          virtual float get();
          virtual void set(float val);
        }

        property bool UseTimeAveragedNetworkMeasure
        {
          virtual bool get();
          virtual void set(bool val);
        }

        property bool UpshiftBitrateInSteps
        {
          virtual bool get();
          virtual void set(bool val);
        }

        property bool AllowSegmentSkipOnSegmentFailure
        {
          virtual bool get();
          virtual void set(bool val);
        }

        property bool ForceKeyFrameMatchOnSeek
        {
          virtual bool get();
          virtual void set(bool val);
        }
 
       /* property bool ForceKeyFrameMatchOnBitrateSwitch
        {
          virtual bool get();
          virtual void set(bool val);
        }*/

        property bool AutoAdjustScrubbingBitrate
        {
          virtual bool get();
          virtual void set(bool val);
        }

        property bool AutoAdjustTrickPlayBitrate
        {
          virtual bool get();
          virtual void set(bool val);
        }
        virtual bool TryLock();
        virtual void Lock();
        virtual void Unlock();
        virtual void BatchPlaylists(Windows::Foundation::Collections::IVector<Platform::String^>^ BatchUrls);
        virtual unsigned int GetLastMeasuredBandwidth();

      };
    }
  }
} 