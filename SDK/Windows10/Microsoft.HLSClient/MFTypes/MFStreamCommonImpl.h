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

#define DEFAULT_VIDEO_STREAMTICK_OFFSET 333334
#define DEFAULT_AUDIO_STREAMTICK_OFFSET 10000000

#include "pch.h" 
#include <wrl.h>
#include <queue>
#include "Utilities\StopWatch.h"
#include "TransportStream\TSConstants.h"
#include "Playlist\MediaSegment.h" 

using namespace Microsoft::WRL;



namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      //forward declare
      class CHLSMediaSource;
      class Playlist;
      class Timestamp;
      ///<summary>Represents a Playlist switch request (either a result of a bitrate switch or an alternate rendition switch)</summary>
      class PlaylistSwitchRequest
      {
      public:
        enum SwitchType { BITRATE, RENDITION };
        Playlist *targetPlaylist;
        Playlist *fromPlaylist;
        shared_ptr<MediaSegment> VideoSwitchedTo;
        short KeyFrameMatchTryCount;
        unsigned int SegmentTryCount;
        bool BufferBuildStarted;
        bool IgnoreBuffer;
        PlaylistSwitchRequest(Playlist *target, Playlist* from, bool ignoreBuffer = false) :
          targetPlaylist(target), fromPlaylist(from), VideoSwitchedTo(nullptr), KeyFrameMatchTryCount(0), SegmentTryCount(0), BufferBuildStarted(false), IgnoreBuffer(false)
        {
        }
      };

      ///<summary>Base implementation for an MF stream</summary>
      class CMFStreamCommonImpl : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IMFMediaStream, IMFAsyncCallback, FtmBase>
      {


      protected:
        //sample token queue

        std::shared_ptr<PlaylistSwitchRequest> pendingBitrateSwitch, pendingRenditionSwitch;
        DWORD SerialWorkQueueID;
        //the event queue for this object
        ComPtr<IMFMediaEventQueue> cpEventQueue;
        //the owning media source
        CHLSMediaSource* cpMediaSource;
        //the stream descriptor
        ComPtr<IMFStreamDescriptor> cpStreamDescriptor;
        std::queue<ComPtr<IUnknown>> sampletokenQueue;
        //the current playlist
        Playlist *pPlaylist;

        //serial work queue for handling sample requests

        //selection state flag
        bool IsSelected;

        ///<summary>Called by derived stream types to complete async sample request work items</summary>
        ///<param name='pResult'>Callback result</param>
        ///<param name='forContentType'>The content type for which a sample is being requested</param>
        HRESULT CompleteAsyncSampleRequest(IMFAsyncResult *pResult, ContentType forContentType);
      public:
        //critsecs for queue operations
        recursive_mutex LockStream, LockEvent, LockSwitch;
        shared_ptr<Timestamp> StreamTickBase;
        unsigned long long ApproximateFrameDistance;
        ComPtr<IMFMediaType> cpMediaType;
#pragma region IMFMediastream
        ///<summary>See IMFMediaStream in MSDN</summary>
        IFACEMETHOD(GetMediaSource)(IMFMediaSource **ppMediaSource);
        ///<summary>See IMFMediaStream in MSDN</summary>
        IFACEMETHOD(GetStreamDescriptor)(IMFStreamDescriptor **ppStreamDescriptor);
        ///<summary>See IMFMediaStream in MSDN</summary>
        IFACEMETHOD(RequestSample)(IUnknown *pToken);

#pragma endregion

#pragma region IMFMediaEventGenerator  
        ///<summary>See IMFMediaEventGenerator in MSDN</summary>
        IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback *pCallback, IUnknown *punkState);
        ///<summary>See IMFMediaEventGenerator in MSDN</summary>
        IFACEMETHOD(EndGetEvent)(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent);
        ///<summary>See IMFMediaEventGenerator in MSDN</summary>
        IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent **ppEvent);
        ///<summary>See IMFMediaEventGenerator in MSDN</summary>
        IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue);

#pragma endregion

#pragma region IMFAsyncCallback
        ///<summary>See IMFAsyncCallback in MSDN</summary>
        IFACEMETHOD(Invoke)(IMFAsyncResult *pResult) = 0;
        ///<summary>See IMFAsyncCallback in MSDN</summary>
        IFACEMETHOD(GetParameters)(DWORD *pdwFlags, DWORD *pdwQueue);
#pragma endregion
#pragma region Class specific



        ComPtr<IMFSample> CreateEmptySample(long long ts = -1);
        ///<summary>Constructor</summary>
        ///<param name='pSource'>The parent media source</param>
        ///<param name='pplaylist'>The current playlist</param>
        CMFStreamCommonImpl(CHLSMediaSource *pSource, Playlist *pplaylist, DWORD CommonWorkQueue);

        ///<summary>Construct stream descriptor - overridden by derived stream type</summary>
        virtual HRESULT ConstructStreamDescriptor(unsigned int Id) = 0;

        ///<summary>Switches the stream from one bitrate to another</summary>
        virtual void SwitchBitrate();
        virtual void RaiseBitrateSwitched(unsigned int From, unsigned int To) = 0;
        ///<summary>Switches the stream to an alternate rendition - overridden by derived stream types</summary>
        virtual void SwitchRendition() = 0;

        ///<summary>Schedule a playloist switch (bitrate or rendition)</summary>
        ///<param name='pPlaylist'>Target playlist (pass nullptr to clear a pending switch request)</param>
        ///<param name='type'>Switch type (Bitrate or Rendition)</param>
        ///<param name='atSegmentBoundary'>True if the switch should take place 
        ///when the stream changes to the next segment or false if the switch should be immediate</param>
        void ScheduleSwitch(Playlist *pPlaylist,
          PlaylistSwitchRequest::SwitchType type, bool IgnoreBuffer = false);


        HRESULT NotifyFormatChanged(IMFMediaType *pMediaType);

        ///<summary>Send start notification(MEStreamStrated)</summary>
        ///<param name='startAt'>Position to start at</param>
        HRESULT NotifyStreamStarted(std::shared_ptr<Timestamp> startAt);

        HRESULT NotifyStreamSeeked(std::shared_ptr<Timestamp> startAt);

        ///<summary>Send end notification(MEEndOfStream)</summary>
        HRESULT NotifyStreamEnded();


        HRESULT NotifyStreamThinning(bool Started);

        ///<summary>Send stream updated notification(MEUpdatedStream)</summary>
        HRESULT NotifyStreamUpdated();

        ///<summary>Send stream stop notification(MEStreamStopped)</summary>
        HRESULT NotifyStreamStopped();

        ///<summary>Send stream paused notification(MEStreamPaused)</summary>
        HRESULT NotifyStreamPaused();

        ///<summary>Send sample available notification notification(MEMediaSample)</summary>
        HRESULT NotifySample(IMFSample *pSample);

        ///<summary>Notify a fatal error</summary>
        ///<param name='hrErrorCode'>Error code</param>
        HRESULT NotifyError(HRESULT hrErrorCode);

        ///<summary>Notify a fatal error</summary>
        ///<param name='hrErrorCode'>Error code</param>
        HRESULT NotifyStreamTick(unsigned long long ts);

        ///<summary>Destructor</summary>
        ~CMFStreamCommonImpl()
        {
          if (cpEventQueue != nullptr)
            cpEventQueue->Shutdown();

        }

        ///<summary>Returns the selection state flag</summary>
        bool Selected()
        {
          return IsSelected;
        };

        ///<summary>Selects a stream</summary>
        ///<param name='cppd'>Presentation Descriptor</param>
        ///<param name='idx'>Stream index</param>
        HRESULT Select(ComPtr<IMFPresentationDescriptor> cppd, DWORD idx)
        {
          if (!IsSelected){
            IsSelected = true;
            return cppd->SelectStream(idx);
          }
          else
            return S_OK;
        }

        ///<summary>Deselects a stream</summary>
        ///<param name='cppd'>Presentation Descriptor</param>
        ///<param name='idx'>Stream index</param>
        HRESULT Deselect(ComPtr<IMFPresentationDescriptor> cppd, DWORD idx)
        {
          if (IsSelected){
            IsSelected = false;
            return cppd->DeselectStream(idx);
          }
          else
            return S_OK;
        }

        ///<summary>Checks to see if a switch is scheduled</summary>
        ///<param name='type'>Switch type (Rendition or Bitrate) to check for</param>
        bool IsSwitchScheduled(PlaylistSwitchRequest::SwitchType type)
        {
          return (type == PlaylistSwitchRequest::SwitchType::BITRATE) ? pendingBitrateSwitch != nullptr :
            (type == PlaylistSwitchRequest::SwitchType::RENDITION ? pendingRenditionSwitch != nullptr : false);
        }

        ///<summary>Sets the current playlist</summary>
        void SetPlaylist(Playlist *ptr)
        {
          pPlaylist = ptr;
        }

        shared_ptr<PlaylistSwitchRequest> GetPendingBitrateSwitch()
        {
          std::lock_guard<std::recursive_mutex> lock(LockSwitch);
          return pendingBitrateSwitch;
        }
        shared_ptr<PlaylistSwitchRequest> GetPendingRenditionSwitch()
        {
          std::lock_guard<std::recursive_mutex> lock(LockSwitch);
          return pendingRenditionSwitch;
        }

#pragma endregion
      };


    }
  }
} 