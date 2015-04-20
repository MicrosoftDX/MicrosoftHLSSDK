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


//Defines all the ABI interfaces used to expose the HLS Command & Control API to the app developer


#pragma once

#include <wtypes.h> 

namespace Microsoft {
  namespace HLSClient {



    //forward declarations
    interface class  IHLSControllerFactory;
    interface class  IHLSController;
    interface class  IHLSPlaylist;
    interface class  IHLSVariantStream;
    interface class  IHLSSegment;
    interface class  IHLSAlternateRendition;
    interface class  IHLSSubtitleRendition;
    interface class  IHLSSubtitleLocator;
    interface class  IHLSInbandCCPayload;
    interface class  IHLSErrorReport;
    interface class  IHLSStreamSelectionChangedEventArgs;
    interface class  IHLSBitrateSwitchEventArgs;
    interface class  IHLSSegmentSwitchEventArgs;
    interface class  IHLSHttpHeaderEntry;
    interface class  IHLSResourceRequestEventArgs;
    interface class  IHLSID3MetadataPayload;
    interface class  IHLSID3MetadataStream;
    interface class  IHLSID3TagFrame;
    interface class  IHLSSlidingWindow;
    interface class  IHLSContentDownloader;
    interface class  IHLSInitialBitrateSelectedEventArgs;

    public enum class ResourceType : int
    {
      PLAYLIST, SEGMENT, KEY, ALTERNATERENDITIONPLAYLIST
    };

    public enum class TrackType : int
    {
      VIDEO, AUDIO, BOTH, METADATA
    };

    public enum class HLSPlaylistType : int
    {
      EVENT, VOD, SLIDINGWINDOW, UNKNOWN
    };

    public enum class SegmentState : int
    {
      NOTLOADED, LOADED, UNAVAILABLE
    };

    public enum class ErrorReportType : int
    {
      SYNC, ASYNC, NONE
    };


    public enum class UnprocessedTagPlacement : int
    {
      PRE, POST, WITHIN
    };

    public enum class SegmentMatchCriterion : int
    {
      SEQUENCENUMBER, PROGRAMDATETIME
    };

    public interface class IHLSBitrateSwitchEventArgs
    {
      property unsigned int FromBitrate {unsigned int get(); };
      property unsigned int ToBitrate {unsigned int get(); };
      property unsigned int LastMeasuredBandwidth {unsigned int get(); };
      property TrackType ForTrackType {TrackType get(); };
      property bool Cancel;
    };


    public interface class IHLSStreamSelectionChangedEventArgs
    {
      property TrackType From { TrackType get(); };
      property TrackType To { TrackType get(); };
    };


    public interface class IHLSSegmentSwitchEventArgs
    {
      property unsigned int FromBitrate { unsigned int get(); };
      property IHLSSegment^ FromSegment { IHLSSegment^ get(); };
      property unsigned int ToBitrate { unsigned int get(); };
      property IHLSSegment^ ToSegment { IHLSSegment^ get(); };
    };


    public interface class IHLSHttpHeaderEntry
    {
      property Platform::String^ Key { Platform::String^ get(); };
      property Platform::String^ Value { Platform::String^ get(); };
    };


    public interface class IHLSResourceRequestEventArgs
    {
      property ResourceType Type { ResourceType get(); };
      property Platform::String^ ResourceUrl;
      Windows::Foundation::Collections::IVector<IHLSHttpHeaderEntry^>^ GetHeaders();
      Windows::Foundation::Collections::IVector<IHLSHttpHeaderEntry^>^ GetCookies();
      void SetHeader(Platform::String^ key, Platform::String^ value);
      Platform::String^ RemoveHeader(Platform::String^ key);
      void SetCookie(Platform::String^ key, Platform::String^ value, Windows::Foundation::TimeSpan ExpirationUTCTime, bool IsHttpOnly, bool IsPersistent);
      Platform::String^ RemoveCookie(Platform::String^ key);
      void SetDownloader(IHLSContentDownloader^ downloader);
      void Submit();
    };

    public interface class IHLSInitialBitrateSelectedEventArgs
    {
      void Submit();
    };

    public interface class IHLSInbandCCPayload
    {
      property Windows::Foundation::TimeSpan Timestamp { Windows::Foundation::TimeSpan get(); };
      Platform::Array<byte>^ GetPayload();
    };


    public interface class IHLSID3TagFrame
    {
      property Platform::String^ Identifier { Platform::String^ get(); }
      Platform::Array<byte>^ GetFrameData();
    };


    public interface class IHLSID3MetadataPayload
    {
      property Windows::Foundation::TimeSpan Timestamp { Windows::Foundation::TimeSpan get(); };
      Platform::Array<byte>^ GetPayload();
      Windows::Foundation::Collections::IVector<IHLSID3TagFrame^>^ ParseFrames();
    };


    public interface class IHLSID3MetadataStream
    {
      property unsigned int StreamID { unsigned int get(); };
      Windows::Foundation::Collections::IVector<IHLSID3MetadataPayload^>^ GetPayloads();
    };

    public interface class IHLSSegment
    {
      property IHLSPlaylist^ ParentPlaylist { IHLSPlaylist^ get(); };
      property unsigned int ForVariant { unsigned int get(); };
      property TrackType MediaType { TrackType get(); };
      property Platform::String^ Url {Platform::String^ get(); };
      property unsigned int SequenceNumber {unsigned int get(); };
      property SegmentState LoadState { SegmentState get(); }
      Windows::Foundation::Collections::IVector<IHLSInbandCCPayload^>^ GetInbandCCUnits();
      Windows::Foundation::Collections::IVector<IHLSID3MetadataStream^>^ GetMetadataStreams();
      Windows::Foundation::Collections::IVector<Platform::String^>^ GetUnprocessedTags(UnprocessedTagPlacement placement);
      Windows::Foundation::IAsyncAction^ SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter);
      Windows::Foundation::IAsyncAction^ ResetPIDFilter(TrackType forType);
      Windows::Foundation::IAsyncAction^ ResetPIDFilter();
      Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ GetPIDFilter();
    };


    public interface class IHLSSubtitleLocator
    {
      property unsigned int Index {unsigned int get(); };
      property Platform::String^ Location {Platform::String^ get(); };
      property float StartPosition {float get(); };
      property float Duration {float get(); };
    };


    public interface class IHLSSubtitleRendition
    {
      Windows::Foundation::IAsyncOperation<unsigned int>^ RefreshAsync();
      Windows::Foundation::Collections::IVector<IHLSSubtitleLocator^>^ GetSubtitleLocators();
    };


    public interface class IHLSAlternateRendition
    {
      property bool IsDefault {bool get(); };
      property bool IsAutoSelect {bool get(); };
      property bool IsForced {bool get(); };
      property Platform::String^ Url {Platform::String^ get(); };
      property Platform::String^ Type {Platform::String^ get(); };
      property Platform::String^ GroupID {Platform::String^ get(); };
      property Platform::String^ Language {Platform::String^ get(); };
      property Platform::String^ Name {Platform::String^ get(); };
      property bool IsActive;
    };


    public interface class IHLSVariantStream
    {
      property bool IsActive {bool get(); };
      property bool HasResolution {bool get(); };
      property unsigned int HorizontalResolution {unsigned int get(); };
      property unsigned int VerticalResolution {unsigned int get(); };
      property unsigned int Bitrate {unsigned int get(); };
      property Platform::String^ Url {Platform::String^ get(); };
      property IHLSPlaylist^ Playlist {IHLSPlaylist^ get(); };

      void LockBitrate();
      Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetAudioRenditions();
      Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetVideoRenditions();
      Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ GetSubtitleRenditions();
      Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ GetPlaylistBatchItemDurations();
    };

    public interface class IHLSSlidingWindow
    {
      property Windows::Foundation::TimeSpan StartTimestamp { Windows::Foundation::TimeSpan get(); }
      property Windows::Foundation::TimeSpan EndTimestamp { Windows::Foundation::TimeSpan get(); }
      property Windows::Foundation::TimeSpan LivePosition { Windows::Foundation::TimeSpan get(); }
    };


    public interface class IHLSPlaylist
    {
      property Platform::String^ Url { Platform::String^ get(); };
      property bool IsLive { bool get(); };
      property bool IsMaster {bool get(); };

      property IHLSVariantStream^ ActiveVariantStream {IHLSVariantStream^ get(); };
      property unsigned int MaximumAllowedBitrate;
      property unsigned int MinimumAllowedBitrate;
      property unsigned int StartBitrate;
      property Windows::Foundation::TimeSpan PlaylistTargetDuration {Windows::Foundation::TimeSpan get(); };
      property Windows::Foundation::TimeSpan DerivedTargetDuration {Windows::Foundation::TimeSpan get(); }; 
      property unsigned int BaseSequenceNumber {unsigned int get(); };
      property unsigned int SegmentCount {unsigned int get(); };
      
      property IHLSVariantStream^ ParentStream {IHLSVariantStream^ get(); };
      property IHLSSlidingWindow^ SlidingWindow {IHLSSlidingWindow^ get(); };
      property HLSPlaylistType PlaylistType{ HLSPlaylistType get(); };

      Windows::Foundation::Collections::IVector<IHLSVariantStream^>^ GetVariantStreams();
      IHLSSegment^ GetSegmentBySequenceNumber(unsigned int SequenceNumber);
      void SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter);
      void ResetPIDFilter(TrackType forType);
      void ResetPIDFilter();
      Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ GetPIDFilter();
      void ResetBitrateLock();
      Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ GetPlaylistBatchItemDurations();

      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchSuggested
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchCancelled
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ BitrateSwitchCompleted
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>^ StreamSelectionChanged
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSStreamSelectionChangedEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>^ SegmentSwitched
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSSegmentSwitchEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegment^>^ SegmentDataLoaded
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegment^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSSegment^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSlidingWindow^>^ SlidingWindowChanged
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSlidingWindow^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSPlaylist^ sender, IHLSSlidingWindow^ args);
      };
    };
 

    public interface class IHLSController
    {
      property bool IsValid { bool get();  };
      property Platform::String^ ID {Platform::String^ get(); };
      property IHLSPlaylist^ Playlist { IHLSPlaylist^ get(); };
      property Windows::Foundation::TimeSpan MinimumBufferLength; 
      property Windows::Foundation::TimeSpan BitrateChangeNotificationInterval;
      property Windows::Foundation::TimeSpan MinimumLiveLatency;
      property bool EnableAdaptiveBitrateMonitor; 
      property bool UseTimeAveragedNetworkMeasure;
      property unsigned int SegmentTryLimitOnBitrateSwitch; 
      property float MinimumPaddingForBitrateUpshift;
      property float MaximumToleranceForBitrateDownshift;
      property bool AllowSegmentSkipOnSegmentFailure;
      property bool ForceKeyFrameMatchOnSeek; 
      property bool ResumeLiveFromPausedOrEarliest;
      //property bool ForceKeyFrameMatchOnBitrateSwitch;
      property bool AutoAdjustScrubbingBitrate;
      property bool AutoAdjustTrickPlayBitrate;
      property bool UpshiftBitrateInSteps;
      property SegmentMatchCriterion MatchSegmentsUsing;
      property Windows::Foundation::TimeSpan PrefetchDuration;
      property TrackType TrackTypeFilter;
      event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSResourceRequestEventArgs^>^ PrepareResourceRequest
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSController^, IHLSResourceRequestEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSController^ sender, IHLSResourceRequestEventArgs^ args);
      };
    
      event Windows::Foundation::TypedEventHandler<IHLSController^, IHLSInitialBitrateSelectedEventArgs^>^ InitialBitrateSelected
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSController^, IHLSInitialBitrateSelectedEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSController^ sender, IHLSInitialBitrateSelectedEventArgs^ args);
      };
      bool TryLock();
      void Lock();
      void Unlock();
      void BatchPlaylists(Windows::Foundation::Collections::IVector<Platform::String^>^ BatchUrls);
      unsigned int GetLastMeasuredBandwidth();
    };

    public interface class IHLSControllerFactory
    {
      event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSResourceRequestEventArgs^>^ PrepareResourceRequest
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSResourceRequestEventArgs^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSControllerFactory^ sender, IHLSResourceRequestEventArgs^ args);
      };
      event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>^ HLSControllerReady
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>^ handler);
        void remove(Windows::Foundation::EventRegistrationToken token);
        void raise(IHLSControllerFactory^ sender, IHLSController^ args);
      };
    };


    public interface class IHLSContentDownloadErrorArgs
    {
      property Windows::Web::Http::HttpStatusCode StatusCode { Windows::Web::Http::HttpStatusCode get(); }
    };


    public interface class IHLSContentDownloadCompletedArgs
    {
      property Windows::Foundation::Collections::IMapView<Platform::String^, Platform::String^>^ ResponseHeaders
      {
        Windows::Foundation::Collections::IMapView<Platform::String^, Platform::String^>^ get();
      };
      property Windows::Storage::Streams::IBuffer^ Content{ Windows::Storage::Streams::IBuffer^ get(); };
      property Windows::Web::Http::HttpStatusCode StatusCode { Windows::Web::Http::HttpStatusCode get(); }
      property Windows::Foundation::Uri^ ContentUri { Windows::Foundation::Uri^ get(); }
      property bool IsSuccessStatusCode { bool get();  }
    };


    public interface class IHLSContentDownloader
    {
      event Windows::Foundation::TypedEventHandler<IHLSContentDownloader^, IHLSContentDownloadCompletedArgs^>^ Completed;

      event Windows::Foundation::TypedEventHandler<IHLSContentDownloader^, IHLSContentDownloadErrorArgs^>^ Error;

      void Initialize(Platform::String^ ContentUri);
      void Cancel();
      Windows::Foundation::IAsyncAction^ DownloadAsync();
      property bool SupportsCancellation { bool get(); }
      property bool IsBusy { bool get();  }

    };


    struct TSEqual : public std::binary_function<const Windows::Foundation::TimeSpan, const Windows::Foundation::TimeSpan, bool>
    {
      bool operator()(const Windows::Foundation::TimeSpan& _Left, const Windows::Foundation::TimeSpan& _Right) const
      {
        return _Left.Duration == _Right.Duration;
      };
    };
  }
} 