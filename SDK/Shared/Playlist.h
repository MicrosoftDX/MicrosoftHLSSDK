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
#include <pch.h>
#include <limits>
#include <map> 
#include <vector>
#include <mutex>
#include <algorithm>
#include <memory>
#include <ppltasks.h>
#include <wrl.h>
#include "Interfaces.h" 
#include "TSConstants.h"
#include "HLSMediaSource.h"
#include "MediaSegment.h"
#include "StreamInfo.h"
#include "StopWatch.h"  
#include "TaskRegistry.h"


using namespace Concurrency;
using namespace Microsoft::WRL;


namespace Microsoft {
  namespace HLSClient {

    ref class HLSControllerFactory;
    class CHLSPlaylistHandler;

    namespace Private {



      //forward declare
      class CHLSMediaSource;
      class CMFVideoStream;
      class CMFAudioStream;
      class Playlist;
      class StreamInfo;
      class MediaSegment;
      class Rendition;
      class Cookie;
      class ContentDownloadRegistry;
      ref class DefaultContentDownloader;

      class Playlist
      {

        friend class CMFAudioStream;
        friend class CMFVideoStream;
        friend class StreamInfo;
        friend class CHLSPlaylistHandler;
        friend class Rendition;
        typedef std::vector<shared_ptr<MediaSegment>> SEGMENTVECTOR;
        typedef std::map<unsigned int, std::shared_ptr<StreamInfo>> VARIANTMAP;
        typedef std::map<std::wstring, std::shared_ptr<std::vector<std::shared_ptr<Rendition>>>> RENDITIONMAP;


      private:
        TaskRegistry<tuple<HRESULT, unsigned int>> taskRegistry; 
        std::map<ContentType, shared_ptr<MediaSegment>> CurrentSegmentTracker;

        //Is anything being currently downloaded ? 
        bool IsBuffering; 
        std::recursive_mutex lockPlaylistRefreshStopWatch;
        shared_ptr<std::map<ContentType, unsigned short>> spPIDFilter;
        ///<summary>Gets the length of the look ahead buffer starting at a specific segment</summary>
        ///<param name='CurSegIdx'>The segment to start calculating from</param>
        ///<returns>The look ahead buffer in ticks</returns>


        ///<summary>Playlist tag parser</summary>
        ///<param name='lines'>A vector of strings - where each string represents a non empty line in the playlist</param>
        void ParseTags(std::vector<std::wstring>& lines);

        ///<summary>Returns the segment that contains the specified timepoint</summary>
        ///<param name='timeinticks'>The timepoint in ticks measured from the start of the presentation</param>
        ///<param name='retrycount'>The number of times the method retries. Each retry reduces the timestamp by an eighth of a second. Value maxes out at 4 and is zero based i.e. 4 = 5 retries.</param>
        ///<returns>The matching segment</returns>
        shared_ptr<MediaSegment> GetSegmentAtTime(unsigned long long TimeInTicks, unsigned short RetryCount = 0);

        ///<summary>Associates rendition information from the parent playlist to individual variant playlists</summary>
        void AssignRenditions();

        Playlist() {}
      public:
        bool StartLiveFromCurrentPos;
        //holds the string data to be parsed (temporarily - cleared on parsing completion)
        std::wstring szData;
        shared_ptr<EncryptionKey> LastCachedKey;
        shared_ptr<StopWatch> spswPlaylistRefresh;
        shared_ptr<StopWatch> spswVideoStreamTick;
        shared_ptr<Playlist> spPlaylistRefresh;
        shared_ptr<ContentDownloadRegistry> spDownloadRegistry;
        shared_ptr<task_completion_event<HRESULT>> sptceWaitPlaylistRefreshPlaybackResume;
        //the original start timestamp
        shared_ptr<Timestamp> StartPTSOriginal;
        //pointer to our custom media source
        CHLSMediaSource* cpMediaSource;
        //pointer to the parent stream - only valid for a child playlist
        StreamInfo *pParentStream;
        //pointer to a parent rendition if this playlist is for an alternate rendition
        Rendition *pParentRendition;
        //Playlist attributes 
        bool IsValid;
        std::wstring BaseUri;
        std::wstring FileName;
        unsigned int Version;
        //HLS spec says this is an integral number of seconds - howevere we have seen playlists using ultra short segment lengths (< 1 s) that generally omit the TargteDuration field (spec violation). In cases like that TG will be subsecond and fractional.
        unsigned long long DerivedTargetDuration;
        unsigned long long PlaylistTargetDuration;

        unsigned int BaseSequenceNumber;
        bool AllowCache;
        
        wstring LastModified;
        wstring ETag;
        Microsoft::HLSClient::HLSPlaylistType PlaylistType;
        shared_ptr<Timestamp> SlidingWindowStart, SlidingWindowEnd;
        //collection of all segments - only valid for a child playlist
        SEGMENTVECTOR Segments;
        //total duration - computed during parsing 
        unsigned long long TotalDuration;
        //variant playlist atributes
        bool IsVariant; //tru if variant parent , false otherwise
        //indicates if this is a live presentation
        bool IsLive;
        bool LastLiveRefreshProcessed;
        //collection of all alternative renditions - valid only for a variant parent
        RENDITIONMAP AudioRenditions, VideoRenditions, SubtitleRenditions;
        std::vector<unsigned int> BitratesInPlaylistOrder;
        //collection of all variants - keyed by bandwidth - valid only for a variant parent
        VARIANTMAP Variants;
        //the currently active bandwidth
        StreamInfo *ActiveVariant;
        //control access to the playlist and the download registry
        recursive_mutex LockClient, LockMerge, LockSegmentList, LockSegmentTracking, LockCookie;
        bool PauseBufferBuilding;
        
        //The maximum and minimum allowed bitrate that a player would allow on this playlist
        unsigned int MaxAllowedBitrate, MinAllowedBitrate;
        //The bitrate (or closest lower) to start with
        unsigned int StartBitrate;

        function<void()> OnPlaylistRefresh;
        function<void()> OnVideoStreamTick;

        void StartVideoStreamTick();
        void StopVideoStreamTick();

        void CancelDownloads();
        //task<void> CancelDownloadsAndWaitForCompletion();
        void CancelDownloadsAndWaitForCompletion();
        shared_ptr<Timestamp> FindApproximateLivePosition();
        unsigned int FindLiveStartSegmentSequenceNumber();
        unsigned int FindLiveStartSegmentSequenceNumber(unsigned int& offsetFromTail, unsigned long long& liveWindowDuration);
        unsigned int FindAltRenditionMatchingSegment(Playlist* mainPlaylist, unsigned int SequenceNumber, bool& EndsBeforeMain);
        unsigned int FindAltRenditionMatchingSegment(Playlist* mainPlaylist, unsigned int SequenceNumber);
        shared_ptr<MediaSegment> GetBitrateSwitchTarget(Playlist *fromPlaylist, bool IgnoreBuffer);
        shared_ptr<MediaSegment> GetBitrateSwitchTarget(Playlist *fromPlaylist, MediaSegment *fromSegment, bool IgnoreBuffer);
        unsigned long long GetCurrentLABLength(unsigned int CurSegSeqNum, bool IncludeDownloading = false, unsigned long long StopIfBeyond = 0);
        void SetLABThreshold();
        ///<summary>Checks the state of in memory buffer starting at a given segment and starts downloading as needed</summary>
        ///<param name='CurSegIdx'>The index of the segment to start checking from</param>
        ///<param name='ForceWait'>Forces waiting on a download operation if one is issued</param>
        ///<returns>Tuple. First value is success/failure indicator. Second value is the sequence number of the segment from which a download commences.</returns>
        task<tuple<HRESULT, unsigned int>> CheckAndBufferIfNeeded(unsigned int CurSegSeqNum, bool ForceWait = false, bool segswitch = false);
        HRESULT CheckAndBufferForBRSwitch(unsigned int CurSegSeqNum);
        HRESULT BulkFetch(shared_ptr<MediaSegment> StartAt, unsigned int FetchCount, MFRATE_DIRECTION direction = MFRATE_FORWARD);
        void CheckAndSetUpStreamTickCountersOnBRSwitch(Playlist* pPlaylist, shared_ptr<MediaSegment> targetseg, shared_ptr<MediaSegment> srcseg);
        ///<summary>Downloads a playlist</summary>  
        ///<param name='URL'>The playlist URL</param>
        ///<param name='spPlaylist'>The downloaded playlist</param>
        ///<param name='tcePlaylistDownloaded'>TCE to signal completion </param>
        ///<returns>Task to wait on</returns>
        static  task<HRESULT> DownloadPlaylistAsync(CHLSMediaSource *ms,
          const std::wstring& URL,
          std::shared_ptr<Playlist>& ppPlaylist,
          task_completion_event<HRESULT> tcePlaylistDownloaded = task_completion_event<HRESULT>());


        static  task<HRESULT> DownloadPlaylistAsync(HLSControllerFactory ^ pFactory,
          DefaultContentDownloader^ spDownloader,
          const std::wstring& URL,
          std::shared_ptr<Playlist>& ppPlaylist,
          task_completion_event<HRESULT> tcePlaylistDownloaded = task_completion_event<HRESULT>());

        static HRESULT OnPlaylistDownloadCompleted(
          const std::wstring& Url,
          std::vector<BYTE> memorycache,
          std::shared_ptr<Playlist>& ppPlaylist,
          task_completion_event<HRESULT> tcePlaylistDownloaded);

        void WaitPlaylistRefreshPlaybackResume();

        ///<summary>Parse a playlist</summary>
        void Parse();

        shared_ptr<StreamInfo> ActivateStream(unsigned int desiredbitrate, bool failfast = false, short searchdirectionincaseoffailure = 0/*0 = both directions,-1 = lower only,+1 higher only*/,bool TestSegmentDownload = false);
        shared_ptr<StreamInfo> DownloadVariantStreamPlaylist(unsigned int desiredbitrate, bool failfast = false, short searchdirectionincaseoffailure = 0/*0 = both directions,-1 = lower only,+1 higher only*/,bool TestSegmentDownload = false);

        ///<summary>Starts downloading segments of a stream. Can perform both in a chained (keep downloading until buffer requirements are met) or non-chained(just download the segment requested) mode.</summary>
        ///<param name='pPlaylist'>The playlist that needs to be streamed</param>
        ///<param name='SequenceNumber'>The segment sequence number to start streaming at</param>
        ///<param name='Chained'>If true streaming will continue until LAB threshold is met or exceeded, if not it stops after one segment download</param>
        ///<param name='WaitNotifyActualDownloadOrEndOfSegments'>Notify on actual download. If set to false, the method returns success if it encounters an already downloaded segment. If set to true it waits to return until an actual segment download is finished.</param>
        ///<param name='Notifier'>TCE used to signal completion </param>
        ///<returns>Returns a tuple containing HRESULT indicating success or failure and the sequence number of the segment associated with the download.</returns>
        static task<tuple<HRESULT, unsigned int>> StartStreamingAsync(Playlist *pPlaylist,
          unsigned int StartAtSequenceNumber = 0UL,
          bool Chained = true,
          bool WaitNotifyActualDownloadOrEndOfSegments = false, bool ForceNonActive = false,
          task_completion_event<tuple<HRESULT, unsigned int>> Notifier = task_completion_event<tuple<HRESULT, unsigned int>>(),
          task_completion_event<tuple<HRESULT, unsigned int>> AltAudioNotifier = task_completion_event<tuple<HRESULT, unsigned int>>(),
          task_completion_event<tuple<HRESULT, unsigned int>> AltVideoNotifier = task_completion_event<tuple<HRESULT, unsigned int>>());

        ///<summary>Prepares a playlist for a pending bitrate transfer</summary>
        ///<param name='pTarget'>Target playlist to transfer to</param>
        ///<returns>Indicates if a switch s possible</returns>
        bool PrepareBitrateTransfer(Playlist *pTargetPlaylist, bool IgnoreBuffer);

        void RaiseStreamSelectionChanged(TrackType from, TrackType to);
        bool CheckAndSwitchBitrate(Playlist *&pPlaylist, ContentType type, unsigned short PID);
        bool CheckAndSwitchAudioRendition(Playlist *&pPlaylist, bool FollowingBitrateSwitch = false);
        //void CheckAndRaiseMetadataEvent(shared_ptr<MediaSegment> spSegment,unsigned int PID);

        ///<summary>Invoked by the MF streams to request a sample during playback</summary>
        ///<param name='pPlaylist'>The target playlist</param>
        ///<param name='type'>The content type for the sample (audio or video)</param>
        ///<param name='ppSample'>The returned sample</param>
        ///<returns>HRESULT indicating success or failure</returns>
        static HRESULT RequestSample(Playlist *pPlaylist, ContentType type, IMFSample** ppSample);
        unsigned long long LiveVideoPlaybackCumulativeDuration;
        unsigned long long LiveAudioPlaybackCumulativeDuration;
        ///<summary>Returns a video sample</summary>
        ///<param name='pPlaylist'>The target playlist</param> 
        ///<param name='ppSample'>The returned sample</param>
        ///<returns>HRESULT indicating success or failure</returns>
        static HRESULT RequestVideoSample(Playlist *pPlaylist, IMFSample** ppSample);

        ///<summary>Returns an audio sample</summary>
        ///<param name='pPlaylist'>The target playlist</param> 
        ///<param name='ppSample'>The returned sample</param>
        ///<returns>HRESULT indicating success or failure</returns>
        static HRESULT RequestAudioSample(Playlist *pPlaylist, IMFSample** ppSample);

        std::map<ContentType, unsigned short> GetPIDFilter();
        void SetPIDFilter(shared_ptr<std::map<ContentType, unsigned short>> pidfilter = nullptr);
        void ResetPIDFilter(ContentType forType);
        void ResetPIDFilter();

        static void AdjustSegmentForVideoThinning(Playlist *pPlaylist, shared_ptr<MediaSegment> curSegment, unsigned short PID, bool& SegmentPIDEOS);
        //void PlayerWindowVisibilityChanged(bool Visible);

        ///<summary>Set the current position on all playing streams on the given playlist. Position is set to the nearest timestamp.</summary>
        ///<param name='pPlaylist'>The target playlist</param>
        ///<param name='PositionInTicks'>The desired position in ticks</param>
        ///<returns>Tuple. First value is the success indicator. Second value is a map of the final positions set for each content type keyed by content type.</returns>
        static tuple<HRESULT, std::map<ContentType, unsigned long long>> SetCurrentPositionVOD(Playlist *pPlaylist, unsigned long long PositionInTicks);
        static tuple<HRESULT, std::map<ContentType, unsigned long long>> SetCurrentPositionLive(Playlist *pPlaylist, unsigned long long PositionInTicks);
        static tuple<HRESULT, std::map<ContentType, unsigned long long>> SetCurrentPositionLiveAlternateAudio(Playlist *pPlaylist, unsigned long long AbsolutePositionInTicks, unsigned int MainPlaylistSegmentSeq);

        void ApplyStreamTick(ContentType type, shared_ptr<MediaSegment> curSegment, shared_ptr<MediaSegment> oldSrcSeg, IMFSample* pSample);
        //void OnPlaylistRefresh();
        bool MergeLivePlaylistChild();
        bool MergeLivePlaylistMaster();
        bool MergeAlternateRenditionPlaylist();
        bool CombinePlaylistBatch(shared_ptr<Playlist> spPlaylist);
        void SetStopwatchForNextPlaylistRefresh(unsigned long long refreshIntervalInTicks,bool lock = true);
       
        void SetupStreamTick(ContentType type, shared_ptr<MediaSegment> curSegment, shared_ptr<MediaSegment> oldSrcSeg);
        /** Playlist Batch functionality*/
        std::map<wstring, shared_ptr<Playlist>> PlaylistBatch;
        HRESULT AddToBatch(std::wstring Uri);

        void InitializePlaylistRefreshCallback();
        void InitializeVideoStreamTickCallback();

        void MakeDiscontinous();
        ///<summary>Playlist ctor - used for variant master</summary>
        ///<param name='data'>The playlist data as a string</param>
        ///<param name='baseuri'>The base URL</param>
        ///<param name='filename'>The file name for the .m3u8</param>
        Playlist(std::wstring& data, std::wstring& baseuri, std::wstring& fileName)
          : szData(data),
          BaseUri(baseuri),
          IsValid(false),
          IsVariant(false),
          IsLive(true),
          LastLiveRefreshProcessed(false),
          FileName(fileName),
          ActiveVariant(nullptr),
          IsBuffering(false),
          pParentStream(nullptr),
          pParentRendition(nullptr),
          BaseSequenceNumber(0),
          DerivedTargetDuration(0),
          AllowCache(false),
          Version(0),
          TotalDuration(0) ,
          MaxAllowedBitrate(UINT32_MAX),
          MinAllowedBitrate(0),
          StartBitrate(0), 
          LastModified(L""),
          ETag(L""),
          SlidingWindowEnd(nullptr),
          SlidingWindowStart(nullptr),
          PauseBufferBuilding(false),
          LiveVideoPlaybackCumulativeDuration(0),
          LiveAudioPlaybackCumulativeDuration(0),
          LastCachedKey(nullptr),
          PlaylistType(Microsoft::HLSClient::HLSPlaylistType::UNKNOWN),
          StartLiveFromCurrentPos(false) 
        {
          spDownloadRegistry = make_shared<ContentDownloadRegistry>();

          InitializePlaylistRefreshCallback();
          InitializeVideoStreamTickCallback();
        }

        ///<summary>Playlist ctor - used for child playlist</summary>
        ///<param name='data'>The playlist data as a string</param>
        ///<param name='baseuri'>The base URL</param>
        ///<param name='filename'>The file name for the .m3u8</param>
        ///<param name='parentstream'>Parent StreamInfo instance</param>
        Playlist(std::wstring& data, std::wstring& baseuri, std::wstring& fileName, StreamInfo *parentstream)
          : szData(data),
          BaseUri(baseuri),
          IsValid(false),
          IsVariant(false),
          IsLive(true),
          LastLiveRefreshProcessed(false),
          FileName(fileName),
          ActiveVariant(nullptr),
          IsBuffering(false),
          pParentStream(parentstream),
          pParentRendition(nullptr),
          BaseSequenceNumber(0),
          DerivedTargetDuration(0),
          AllowCache(false),
          Version(0),
          TotalDuration(0),
          MaxAllowedBitrate(UINT32_MAX),
          MinAllowedBitrate(0),
          StartBitrate(0), 
          LastModified(L""),
          ETag(L""),
          SlidingWindowEnd(nullptr),
          SlidingWindowStart(nullptr),
          PauseBufferBuilding(false),
          LiveVideoPlaybackCumulativeDuration(0),
          LiveAudioPlaybackCumulativeDuration(0),
          LastCachedKey(nullptr),
          PlaylistType(Microsoft::HLSClient::HLSPlaylistType::UNKNOWN),
          StartLiveFromCurrentPos(false) 
        {
          spDownloadRegistry = make_shared<ContentDownloadRegistry>();


          InitializePlaylistRefreshCallback();
          InitializeVideoStreamTickCallback();
        }

        ///<summary>Playlist ctor - used for alternate rendition playlist</summary>
        ///<param name='data'>The playlist data as a string</param>
        ///<param name='baseuri'>The base URL</param>
        ///<param name='filename'>The file name for the .m3u8</param>
        ///<param name='parentrendition'>Parent StreamInfo instance</param>
        Playlist(std::wstring& data, std::wstring& baseuri, std::wstring& fileName, Rendition *parentrendition)
          : szData(data),
          BaseUri(baseuri),
          IsValid(false),
          IsVariant(false),
          IsLive(true),
          LastLiveRefreshProcessed(false),
          FileName(fileName),
          ActiveVariant(nullptr),
          IsBuffering(false),
          pParentStream(nullptr),
          pParentRendition(parentrendition),
          BaseSequenceNumber(0),
          DerivedTargetDuration(0),
          AllowCache(false),
          Version(0),
          TotalDuration(0),
          MaxAllowedBitrate(UINT32_MAX),
          MinAllowedBitrate(0),
          StartBitrate(0), 
          LastModified(L""),
          ETag(L""),
          SlidingWindowEnd(nullptr),
          SlidingWindowStart(nullptr),
          PauseBufferBuilding(false),
          LiveVideoPlaybackCumulativeDuration(0),
          LiveAudioPlaybackCumulativeDuration(0),
          LastCachedKey(nullptr),
          PlaylistType(Microsoft::HLSClient::HLSPlaylistType::UNKNOWN),
          StartLiveFromCurrentPos(false) 
        {
          spDownloadRegistry = make_shared<ContentDownloadRegistry>();

          InitializePlaylistRefreshCallback();
          InitializeVideoStreamTickCallback();
        }

        ///<summary>Destructor</summary>
        ~Playlist();

        

        

        ///<summary>Rewinds an entire playlist</summary>
        void Rewind()
        {
          //if variant
          if (IsVariant)
          {
            //call rewind on each variant
            for (auto itr : this->Variants)
            {
              if (itr.second->spPlaylist != nullptr)
                itr.second->spPlaylist->Rewind();
            }
          }
          else
          {
            std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
            if (IsLive) listlock.lock();

            if (Segments.size() > 0)
            {
              //rewind each in memory segment
              for (auto itr : Segments)
              {
                if (itr->GetCurrentState() == MediaSegmentState::INMEMORYCACHE)
                  itr->RewindInCacheSegment();
              }

              //set current indices for each content type to 0
              for (auto itr : CurrentSegmentTracker)
                SetCurrentSegmentTracker(itr.first, Segments.front());
            }
          }
        }

        ///<summary>Returns the parent StreamInfo (for a variant child playlist)</summary>
        StreamInfo *GetParentStream()
        {
          return pParentStream;
        };

        ///<summary>Associates the MF media source with the playlist</summary>
        void AttachMediaSource(CHLSMediaSource *pMS)
        {
          cpMediaSource = pMS;
        }

        void SetLastModifiedSince(wstring lastmod, wstring etag)
        {
          LastModified = lastmod;
          ETag = etag;
        }
        ///<summary>Returns an ascending order list of bandwidths supported by the master playlist</summary>
        std::vector<unsigned int> GetBitrateList()
        {
          return BitratesInPlaylistOrder;
        }

        ///<summary>Checks ti see if we are at the end of the playlist</summary>
        bool IsEOS();


        ///<summary>Checks EOS for a specific stream</summary>
        ///<param name='mediaType'>Content type for the stream to check</param>
        bool IsEOS(ContentType mediaType);

        shared_ptr<MediaSegment> GetSegment(unsigned int SeqNum)
        {
          std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
          if (IsLive) listlock.lock();
          auto itrMatch = std::find_if(Segments.begin(), Segments.end(), [SeqNum](shared_ptr<MediaSegment> seg)
          {
            return seg->SequenceNumber == SeqNum;
          });
          return itrMatch == Segments.end() ? nullptr : *itrMatch;
        }

        shared_ptr<MediaSegment> GetNextSegment(unsigned int SeqNum, MFRATE_DIRECTION dir)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegmentList);
          if (dir == MFRATE_FORWARD)
          {
            auto itrMatch = std::find_if(Segments.begin(), Segments.end(), [SeqNum](shared_ptr<MediaSegment> seg)
            {
              return seg->SequenceNumber >= SeqNum + 1;// && 
              //!(seg->GetCurrentState() == INMEMORYCACHE && seg->LengthInBytes == 0); //skip zero length segments
            });
            return itrMatch == Segments.end() ? nullptr : *itrMatch;
          }
          else
          {
            if (SeqNum == 0)
              return nullptr;

            auto itrMatch = std::find_if(Segments.rbegin(), Segments.rend(), [SeqNum](shared_ptr<MediaSegment> seg)
            {
              return seg->SequenceNumber <= SeqNum - 1;// && 
              //!(seg->GetCurrentState() == INMEMORYCACHE && seg->LengthInBytes == 0); //skip zero length segments
            });
            return itrMatch == Segments.rend() ? nullptr : *itrMatch;
          }
        }

        ///<summary>Returns the maximum current segment index value across all content types</summary>
        shared_ptr<MediaSegment> MaxCurrentSegment()
        {
          shared_ptr<MediaSegment> ret = nullptr;
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);

          auto curvidseg = GetCurrentSegmentTracker(VIDEO);
          auto curaudseg = GetCurrentSegmentTracker(AUDIO);
          if (curvidseg != nullptr && curaudseg != nullptr)
            ret = (curvidseg->SequenceNumber > curaudseg->SequenceNumber ? curvidseg : curaudseg);
          else if (curvidseg != nullptr)
            ret = curvidseg;
          else if (curaudseg != nullptr)
            ret = curaudseg;



          return ret;
        }

        shared_ptr<MediaSegment> MaxBufferedSegment()
        {
          auto maxseg = MaxCurrentSegment();

          std::lock_guard<std::recursive_mutex> lockTrack(LockSegmentTracking);


          std::unique_lock<std::recursive_mutex> lockList(LockSegmentList, std::defer_lock);
          std::unique_lock<std::recursive_mutex> lockMerge(LockMerge, std::defer_lock);

          if (IsLive)
          {
            lockList.lock();
            lockMerge.lock();
          }

          if ((IsLive && lockList.owns_lock() && lockMerge.owns_lock()) || (!IsLive))
          {

            auto seg = std::find_if(Segments.begin(), Segments.end(), [this, maxseg](shared_ptr<MediaSegment> ms){
              return ms->SequenceNumber > maxseg->SequenceNumber && ms->GetCurrentState() != INMEMORYCACHE;
            });
            if (seg == Segments.end())
              return Segments.back();
            else
              return GetNextSegment((*seg)->GetSequenceNumber(), MFRATE_REVERSE);
          }
          else
          {
            return maxseg;
          }
        }

        ///<summary>Returns the maximum current segment index value across all content types</summary>
        shared_ptr<MediaSegment> MinCurrentSegment()
        {
          shared_ptr<MediaSegment> ret = nullptr;
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);

          auto curvidseg = GetCurrentSegmentTracker(VIDEO);
          auto curaudseg = GetCurrentSegmentTracker(AUDIO);
          if (curvidseg != nullptr && curaudseg != nullptr)
            ret = (curvidseg->SequenceNumber < curaudseg->SequenceNumber ? curvidseg : curaudseg);
          else if (curvidseg != nullptr)
            ret = curvidseg;
          else if (curaudseg != nullptr)
            ret = curaudseg;

          return ret;
        }

        bool HasCurrentSegmentTracker(ContentType type)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);
          return CurrentSegmentTracker.find(type) != CurrentSegmentTracker.end() && CurrentSegmentTracker[type] != nullptr;
        }

        shared_ptr<MediaSegment> GetCurrentSegmentTracker(ContentType type)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);
          if (CurrentSegmentTracker.find(type) != CurrentSegmentTracker.end())
            return CurrentSegmentTracker[type];
          else
            return nullptr;
        }

        shared_ptr<Timestamp> GetPlaylistStartTimestamp();

        bool IsSegmentPlayingBack(int SequenceNumber)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);
          return std::find_if(CurrentSegmentTracker.begin(), CurrentSegmentTracker.end(), [SequenceNumber](std::pair < ContentType, shared_ptr<MediaSegment>> p)
          {
            return p.second != nullptr && p.second->GetSequenceNumber() == SequenceNumber;
          }) != CurrentSegmentTracker.end();
        }


        shared_ptr<MediaSegment> SetCurrentSegmentTracker(ContentType type, shared_ptr<MediaSegment> seg)
        {
          std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);
          CurrentSegmentTracker[type] = seg;
          return seg;
        }

        ///<summary>Normalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to normalize</param>
        ///<returns>The normalized timestamp</returns>
        ///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
        //and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
        ///that begins with a tiemstamp = 0</remarks>
        shared_ptr<Timestamp> TSAbsoluteToRelative(shared_ptr<Timestamp> ts)
        {
          //clamp to absolute start
          return make_shared<Timestamp>(ts->ValueInTicks < StartPTSOriginal->ValueInTicks ? 0 : ts->ValueInTicks - StartPTSOriginal->ValueInTicks);
        }

        ///<summary>Normalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to normalize</param>
        ///<returns>The normalized timestamp</returns>
        ///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
        //and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
        ///that begins with a tiemstamp = 0</remarks>
        shared_ptr<Timestamp> TSAbsoluteToRelative(unsigned long long tsval)
        {
          //clamp to absolute start
          return make_shared<Timestamp>(tsval < StartPTSOriginal->ValueInTicks ? 0 : tsval - StartPTSOriginal->ValueInTicks);
        }

        ///<summary>Denormalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to denormalize</param>
        ///<returns>The denormalized timestamp</returns>
        ///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
        shared_ptr<Timestamp> TSRelativeToAbsolute(shared_ptr<Timestamp> ts)
        {
          return make_shared<Timestamp>(ts->ValueInTicks + StartPTSOriginal->ValueInTicks);
        }

        ///<summary>Denormalizes a timestamp</summary>
        ///<param name='ts'>The timestamp to denormalize</param>
        ///<returns>The denormalized timestamp</returns>
        ///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
        shared_ptr<Timestamp> TSRelativeToAbsolute(unsigned long long tsval)
        {
          return make_shared<Timestamp>(tsval + StartPTSOriginal->ValueInTicks);
        }

        shared_ptr<Timestamp> GetSlidingWindowStart()
        {
          /*std::lock_guard<std::recursive_mutex> lock(LockMerge);*/
          return SlidingWindowStart;
        }
        shared_ptr<Timestamp> GetSlidingWindowEnd()
        {
          /*std::lock_guard<std::recursive_mutex> lock(LockMerge);*/
          return SlidingWindowEnd;
        }

        int NeedsPreFetch();

      };
    }
  }
} 