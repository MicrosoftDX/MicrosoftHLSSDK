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


#include "pch.h"  
#include <wrl.h>
#include <mfapi.h>
#include <mfidl.h>
#include <Mferror.h>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include "Playlist\PlaylistOM.h"   
#include "Adaptive\AdaptiveHeuristics.h"  
#include "Downloader\ContentDownloadRegistry.h"
#include "MFAudioStream.h"
#include "MFVideoStream.h" 
#include "Utilities\TaskRegistry.h"    

using namespace Microsoft::WRL;
using namespace std;
using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {

    ref class HLSControllerFactory;

    namespace Private {

      DEFINE_GUID(MF_EVENT_SOURCE_ACTUAL_START,
        0xa8cc55a9, 0x6b31, 0x419f, 0x84, 0x5d, 0xff, 0xb3, 0x51, 0xa2, 0x43, 0x4b);

      class CMFAudioStream;
      class CMFVideoStream;
      class VariableRate;

      enum MediaSourceState
      {
        MSS_OPENING, MSS_STARTING, MSS_STARTED, MSS_STOPPED, MSS_UNINITIALIZED, MSS_PAUSED, MSS_BUFFERING, MSS_ERROR, MSS_SEEKING
      };

      enum CommandType
      {
        BAD, START, STOP, PAUSE, SHUTDOWN
      };

      class AsyncCommand : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IUnknown>
      {
      private:

      public:
        CommandType type;
        shared_ptr<Timestamp> spTimestamp;
        MediaSourceState oldState;
        AsyncCommand(CommandType cType) :type(cType)
        {
        }
        AsyncCommand(CommandType cType, shared_ptr<Timestamp> ts) :type(cType), spTimestamp(ts)
        {
        }

        AsyncCommand(CommandType cType, MediaSourceState os) :type(cType), oldState(os)
        {
        }
        ~AsyncCommand()
        {

        }
      };


      //forward declare
      ref class HLSController;


      class CHLSMediaSource :
        public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>,
        IMFMediaSource,
        IMFGetService,
        IMFRateControl,
        IMFRateSupport,
        //IMFQualityAdvise,
        IMFAsyncCallback,
        FtmBase>
      {

        friend class Playlist;
        friend class StreamInfo;
        friend class MediaSegment;
        friend class CHLSPlaylist;
        friend class CHLSVariantStream;
        friend class CHLSAlternateRendition;
        friend class CHLSSubtitleRendition;
        friend class CMFAudioStream;
        friend class CMFVideoStream;
        friend class CMFStreamCommonImpl;
        friend class TaskRegistry<HRESULT>;
        friend class TaskRegistry<tuple<HRESULT, unsigned int>>;
        friend class TaskRegistry<std::vector<HRESULT>>;
      private:

        //serial work queue 
        DWORD SerialWorkQueueID, StreamWorkQueueID;
        
        //call back interface to inform the calling byte stream handler of initialization completion
        ComPtr<IMFAsyncResult> cpAsyncResultForOpen;
        //URI details for the playlist
        std::wstring baseUri, topPlaylistFilename, uri;
        //event queue for the media source
        ComPtr<IMFMediaEventQueue> sourceEventQueue;

        bool HandleInitialPauseForAutoPlay;

        //streams
        ComPtr<CMFAudioStream> cpAudioStream;
        ComPtr<CMFVideoStream> cpVideoStream;
        //state flag
        MediaSourceState currentSourceState;
        MediaSourceState preBufferingState;
        //critsec to guard event queue operations
        recursive_mutex LockSource, LockStateUpdate, LockEvent, LockBitrateLock;
        //current playback rate (0 or 1 only in this release)
        shared_ptr<VariableRate> curPlaybackRate;
        shared_ptr<VariableRate> prevPlaybackRate;
        //current playback direction
        MFRATE_DIRECTION curDirection;
        ////drop mode
        //MF_QUALITY_DROP_MODE curDropMode;
        ////quality level
        //MF_QUALITY_LEVEL curQualityLevel; 
        std::vector<shared_ptr<VariableRate>> SupportedRates;

       
        bool PlayerWindowVisible;
        bool BitrateLocked;
        std::vector<unsigned int> bufferinghistory;
        bool LivePlaylistPositioned;
        shared_ptr<StopWatch> swStopOrPause;
      public: 

        
        TaskRegistry<HRESULT> protectionRegistry;
        const unsigned int VIDEOSTREAMID, AUDIOSTREAMID;
        //PD
        ComPtr<IMFPresentationDescriptor> cpPresentationDescriptor;
        shared_ptr<ContentDownloadRegistry> spDownloadRegistry;
        shared_ptr<HeuristicsManager> spHeuristicsManager;
        //controller API
        HLSController^ cpController;
        HLSControllerFactory^ cpControllerFactory;
        //root playlist
        shared_ptr<Playlist> spRootPlaylist;

        CHLSMediaSource(std::wstring Uri);

        ~CHLSMediaSource();

        shared_ptr<VariableRate> GetCurrentPlaybackRate();

        MFRATE_DIRECTION GetCurrentDirection();

      
        shared_ptr<MediaSegment> LastPlayedVideoSegment, LastPlayedAudioSegment;

        size_t GetPlaybackRateDistanceFromNormal();

        MediaSourceState GetCurrentState() {

          return currentSourceState;
        }
        void SetCurrentState(MediaSourceState state) {

          currentSourceState = state;
        }

        void BlockPrematureRelease(task_completion_event<HRESULT> tce)
        {
          protectionRegistry.Register(task<HRESULT>(tce));
        }

        void UnblockReleaseBlock(task_completion_event<HRESULT> tce)
        {
          tce.set(S_OK);
         // protectionRegistry.CleanupCompletedOrCanceled();
        }

        void SetBitrateLock(bool val) {
          std::lock_guard<recursive_mutex> lock(LockBitrateLock);
          BitrateLocked = val;
          return;
        }

        bool IsBitrateLocked()
        {
          std::lock_guard<recursive_mutex> lock(LockBitrateLock);
          return BitrateLocked;
        }



        HRESULT StartAsync(shared_ptr<Timestamp> startFrom);
        HRESULT StartAsyncWithAltAudio(shared_ptr<Timestamp> startFrom);

        HRESULT StopAsync();

        HRESULT PauseAsync();

        HRESULT ShutdownAsync(MediaSourceState oldState);
        ///<summary>Associates the controller with the media source</summary>
        ///<param name='pcontroller'>The controller instance</param>
        void SetHLSController(HLSController ^pcontroller);

        ///<summary>Handles an incoming bitrate switch change suggestion from the bitrate monitor</summary>
        ///<param name='Bandwidth'>The suggested bitrate</param>
        ///<param name='Cancel'>Set to true if player cancels the bitrate switch - used to inform the bitrate monitor of the cancellation</param>
        void BitrateSwitchSuggested(unsigned int Bandwidth, unsigned int LastMeasured, bool& Cancel, bool IgnoreBuffer = false);

        ///<summary>Cancels any pending bitrate changes</summary>
        bool TryCancelPendingBitrateSwitch(bool Force = false);

        ///<summary>Cancels any pending rendition changes</summary>
        void CancelPendingRenditionChange();
        ///<summary>Handles a change request from the player to an alternate audio or video rendition (subtitle is handled at the player)</summary>
        ///<param name='RenditionType'>AUDIO or VIDEO</param>
        ///<param name='targetRendition'>The target rendition instance</param>
        HRESULT RenditionChangeRequest(std::wstring RenditionType, shared_ptr<Rendition> targetRendition, StreamInfo *ForVariant);



        ///<summary>Send presentation end</summary>
        HRESULT NotifyPresentationEnded();

        ///<summary>Called to complete the asynchronous BeginOpen()</summary>
        void EndOpen();

        ///<summary>Async call to initialize Media Source. Called by the the byte stream handler.</summary>
        ///<param name='pAsyncResult'>The IMFAsyncResult instance to call the byte stream handler back on, when initialization is completed.</param>
        ///<param name='cpControllerFactory'>The controller factory instance</param>
        HRESULT BeginOpen(IMFAsyncResult *pAsyncResult, Microsoft::HLSClient::HLSControllerFactory^ cpControllerFactory);
        HRESULT BeginOpen(IMFAsyncResult *pAsyncResult, Microsoft::HLSClient::HLSControllerFactory^ cpControllerFactory, wstring PlaylistData);
        ///<summary>Constructs the MF streams</summary>
        ///<param name='pPlaylist'>The playlist for the currently active variant (not the master playlist)</param>
        HRESULT ConstructStreams(Playlist *pPlaylist);
        HRESULT ConstructStreams(StreamInfo *pVariantStremInfo);

        ///<summary>Starts playing back a playlist</summary>
        ///<param name='pPlaylist'>Target playlist</param>
        ///<param name='curState'>The current state the media source is in</param>
        ///<param name='posTS'>The position to start playback from</param>
        //   HRESULT StartPlaylist(Playlist *pPlaylist,MediaSourceState curState = MediaSourceState::MSS_STARTING,std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartPlaylistInitial(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartVODPlaylistFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartLivePlaylistFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartVODPlaylistFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartLivePlaylistFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartVODPlaylistWithAltAudioFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartLivePlaylistWithAltAudioFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartVODPlaylistWithAltAudioFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        HRESULT StartLivePlaylistWithAltAudioFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS = nullptr);
        ///<summary>Puts the media source in a state of buffering</summary>
        HRESULT StartBuffering(bool Record = false,bool CanInitiateDownSwitch = false);
 

        ///<summary>Checks to see if the media source is buffering</summary>
        bool IsBuffering();

        ///<summary>Puts the media source out of buffering state</summary>
        HRESULT EndBuffering();

        ///<summary>Notify (MENewStream) that a new stream has been created</summary>
        ///<param name='pStream'>Stream instance</param>
        HRESULT NotifyNewStream(IMFMediaStream *pStream);

        ///<summary>Notify (MEUpdatedStream) that a stream has been updated</summary>
        ///<param name='pStream'>Stream instance</param>
        HRESULT NotifyStreamUpdated(IMFMediaStream *pStream);

        ///<summary>Notify (MESourceStarted) that a source has been started</summary>
        ///<param name='posTS'>Start position</param>
        ///<param name='hrStatus'>Indicates success or failure in starting the source</param>
        HRESULT NotifyStarted(std::shared_ptr<Timestamp> posTS = nullptr, HRESULT hr = S_OK);

        HRESULT NotifyActualStart(std::shared_ptr<Timestamp> posTS, HRESULT hr = S_OK);

        HRESULT NotifySeeked(std::shared_ptr<Timestamp> posTS = nullptr, HRESULT hr = S_OK);

        ///<summary>Notify (MESourcePaused) that a source has been paused</summary> 
        HRESULT NotifyPaused();

        ///<summary>Notify (MESourceStopped) that a source has been stopped</summary> 
        HRESULT NotifyStopped();

        ///<summary>Notify (MESourceRateChanged) that playback rate has changed</summary>
        ///<param name='newRate'>The new playback rate</param>
        HRESULT NotifyRateChanged(float newRate);

        ///<summary>Notify a fatal error</summary>
        ///<param name='hrErrorCode'>Error code</param>
        HRESULT NotifyError(HRESULT hrErrorCode);

#pragma region IMFMediaSource
        ///<summary>CreatePresentationDescriptor (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(CreatePresentationDescriptor)(IMFPresentationDescriptor **ppPresentationDescriptor);

        ///<summary>Start (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(Start)(IMFPresentationDescriptor *pPresentationDescriptor, const GUID *pGuidTimeFormat, const PROPVARIANT *pvarStartPosition);

        ///<summary>GetCharacteristics (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(GetCharacteristics)(DWORD *pdwCharacteristics);

        ///<summary>Pause (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(Pause)();

        ///<summary>Shutdown (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(Shutdown)();

        ///<summary>Stop (see IMFMediaSource on MSDN)</summary>
        IFACEMETHOD(Stop)();
#pragma endregion

#pragma region IMFGetService

        ///<summary>GetService (see IMFGetService on MSDN)</summary>
        IFACEMETHOD(GetService)(REFGUID guidService, REFIID riid, LPVOID *ppvObject);
#pragma endregion

#pragma region Rate Control

        ///<summary>GetRate (see IMFRateControl on MSDN)</summary>
        IFACEMETHOD(GetRate)(BOOL *pfThin, float *pflRate);

        ///<summary>SetRate (see IMFRateControl on MSDN)</summary>
        IFACEMETHOD(SetRate)(BOOL fThin, float flRate);

        ///<summary>GetFastestRate (see IMFRateSupport on MSDN)</summary>
        IFACEMETHOD(GetFastestRate)(MFRATE_DIRECTION direction, BOOL fThin, float *pflRate);

        ///<summary>GetSlowestRate (see IMFRateSupport on MSDN)</summary>
        IFACEMETHOD(GetSlowestRate)(MFRATE_DIRECTION direction, BOOL fThin, float *pflRate);

        ///<summary>IsRateSupported (see IMFRateSupport on MSDN)</summary>
        IFACEMETHOD(IsRateSupported)(BOOL fThin, float flRate, float *pflNearestSupportedRate);
#pragma endregion

        //#pragma region IMFQualityAdvise
        //
        //        IFACEMETHOD(DropTime)(LONGLONG hnsAmountToDrop);
        //        IFACEMETHOD(GetDropMode)(MF_QUALITY_DROP_MODE *pDropMode);
        //        IFACEMETHOD(GetQualityLevel)(MF_QUALITY_LEVEL *pQualityLevel);
        //        IFACEMETHOD(SetDropMode)(MF_QUALITY_DROP_MODE dropMode);
        //        IFACEMETHOD(SetQualityLevel)(MF_QUALITY_LEVEL qualityLevel);
        //
        //#pragma endregion

#pragma region IMFMediaEventGenerator

        ///<summary>BeginGetEvent (see IMFMediaEventGenerator on MSDN)</summary>
        IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback *pCallback, IUnknown *punkState);

        ///<summary>EndGetEvent (see IMFMediaEventGenerator on MSDN)</summary>
        IFACEMETHOD(EndGetEvent)(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
        ///<summary>GetEvent (see IMFMediaEventGenerator on MSDN)</summary>
        IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent **ppEvent);

        ///<summary>QueueEvent (see IMFMediaEventGenerator on MSDN)</summary>
        IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);
#pragma endregion

        IFACEMETHOD(QueueEvent)(IMFMediaEvent* evt);

#pragma region IMFAsyncCallback
        ///<summary>See IMFAsyncCallback in MSDN</summary>
        IFACEMETHOD(Invoke)(IMFAsyncResult *pResult);
        ///<summary>See IMFAsyncCallback in MSDN</summary>
        IFACEMETHOD(GetParameters)(DWORD *pdwFlags, DWORD *pdwQueue);
#pragma endregion
      };
    }
  }
} 