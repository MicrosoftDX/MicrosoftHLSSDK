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
#include "pch.h"
#include "TSConstants.h"
#include "Timestamp.h"
#include "VariableRate.h"
#include "PlaylistOM.h" 
#include "HLSController.h"
#include "HLSControllerFactory.h"
#include "HLSPlaylist.h"
#include "MFAudioStream.h"
#include "MFVideoStream.h"
#include "HLSMediaSource.h"

using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

#pragma region INITIALIZATION
CHLSMediaSource::CHLSMediaSource(std::wstring Uri) :
uri(Uri), BitrateLocked(false),
currentSourceState(MediaSourceState::MSS_UNINITIALIZED),
preBufferingState(MediaSourceState::MSS_UNINITIALIZED),
curPlaybackRate(nullptr), prevPlaybackRate(nullptr),
PlayerWindowVisible(true),
curDirection(MFRATE_DIRECTION::MFRATE_FORWARD),
spHeuristicsManager(nullptr), VIDEOSTREAMID(1),
AUDIOSTREAMID(0), HandleInitialPauseForAutoPlay(false),
LastPlayedVideoSegment(nullptr), LastPlayedAudioSegment(nullptr),
LivePlaylistPositioned(false)
{

  //  taskRegistry3.SetMediaSource(this);
  MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &SerialWorkQueueID);
  MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &StreamWorkQueueID);
  MFCreateEventQueue(&sourceEventQueue);

  SupportedRates.push_back(make_shared<VariableRate>(-12.0f, true, 2));
  SupportedRates.push_back(make_shared<VariableRate>(-8.0f, true, 1));
  SupportedRates.push_back(make_shared<VariableRate>(-4.0f, true, 0));
  SupportedRates.push_back(make_shared<VariableRate>(-2.0f, true, 0));
  SupportedRates.push_back(make_shared<VariableRate>(0.0f, false, 0));
  SupportedRates.push_back(make_shared<VariableRate>(0.5f, false, 0));
  curPlaybackRate = make_shared<VariableRate>(1.0f, false, 0);
  SupportedRates.push_back(curPlaybackRate);
  SupportedRates.push_back(make_shared<VariableRate>(2.0f, false, 0));
  SupportedRates.push_back(make_shared<VariableRate>(4.0f, true, 0));
  SupportedRates.push_back(make_shared<VariableRate>(8.0f, true, 1));
  SupportedRates.push_back(make_shared<VariableRate>(12.0f, true, 2));


}

CHLSMediaSource::~CHLSMediaSource()
{
  if (this->cpController != nullptr)
    this->cpController->Invalidate();


  if (StreamWorkQueueID != 0)
    MFUnlockWorkQueue(StreamWorkQueueID);
  if (SerialWorkQueueID != 0)
    MFUnlockWorkQueue(SerialWorkQueueID);

  if (sourceEventQueue != nullptr)
    sourceEventQueue->Shutdown();

  if (spHeuristicsManager != nullptr)
    spHeuristicsManager->StopNotifier();

  /*if (spRootPlaylist != nullptr)
  {

  spRootPlaylist->CancelDownloadsAndWaitForCompletion();

  protectionRegistry.CleanupCompletedOrCanceled();

  protectionRegistry.WhenAll().wait();

  }*/
  //LOG("MediaSource Destroyed");
}
/*
unsigned long long TimeElapsedSinceLastStopOrPause()
{
SYSTEMTIME systime;
ZeroMemory(&systime, sizeof(SYSTEMTIME));
::GetSystemTime(&systime);

FILETIME ftime;
ZeroMemory(&ftime, sizeof(FILETIME));
SystemTimeToFileTime(&systime, &ftime);
ULARGE_INTEGER uiftm;
uiftm.HighPart = ftime.dwHighDateTime;
uiftm.LowPart = ftime.dwLowDateTime;
auto curtime = uiftm.QuadPart;


return (curtime - LastStopOrPauseTime);


}

unsigned long long RecordStopOrPauseTime()
{



SYSTEMTIME systime;
ZeroMemory(&systime, sizeof(SYSTEMTIME));
::GetSystemTime(&systime);

FILETIME ftime;
ZeroMemory(&ftime, sizeof(FILETIME));
SystemTimeToFileTime(&systime, &ftime);
ULARGE_INTEGER uiftm;
uiftm.HighPart = ftime.dwHighDateTime;
uiftm.LowPart = ftime.dwLowDateTime;
LastStopOrPauseTime = uiftm.QuadPart;
}

*/

///<summary>Associates the controller with the media source</summary>
///<param name='pcontroller'>The controller instance</param>
void CHLSMediaSource::SetHLSController(HLSController^ pcontroller)
{
  cpController = pcontroller;
}

///<summary>Async call to initialize Media Source. Called by the the byte stream handler.</summary>
///<param name='pAsyncResult'>The IMFAsyncResult instance to call the byte stream handler back on, when initialization is completed.</param>
///<param name='cpControllerFactory'>The controller factory instance</param>
HRESULT CHLSMediaSource::BeginOpen(IMFAsyncResult *pAsyncResult, Microsoft::HLSClient::HLSControllerFactory^ cpFactory)
{
  //LOG("MediaSource BeginOpen()");


  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);

  cpControllerFactory = cpFactory;
  //reset the last suggested bandwidth - it will get recalculated later in the code
  spHeuristicsManager = make_shared<HeuristicsManager>(this);

  spDownloadRegistry = make_shared<ContentDownloadRegistry>();
  //set state flag
  SetCurrentState(MediaSourceState::MSS_OPENING);

  //bad URI
  if (uri.empty() || Helpers::SplitUri(uri, baseUri, topPlaylistFilename) == false)
  {
    UnblockReleaseBlock(tceProtectPlaylist);
    return E_FAIL;
  }


  //store the calllback interface
  cpAsyncResultForOpen = pAsyncResult;

  //download root playlist
  //Question : Why do we redownload the  playlist when the byte stream handler has already downloaded it and we can simply use the stream ?
  //Answer : Many HLS implementers send cookies down with the initial HTTP response on the master playlist download, that serve as an authn cookie 
  //and need to be resent with subsequent requests for children playlists. The byte stream handler does not afford us any way to access the HTTP response 
  //to extract a cookie. Hence the redownload. Alternative would be to implement a scheme handler, but it makes the scenario more complicated when the same page 
  //has both non HLS(say smooth streaming) and HLS playback. We would have re-delegate scheme requests based on file extensions to the appropriate handlers/media 
  //sources from here if they are not HLS(.m3u8)
  Playlist::DownloadPlaylistAsync(this, this->uri, spRootPlaylist)
    .then([this, tceProtectPlaylist](task<HRESULT> taskDownload)
  {
    HRESULT hr = S_OK;

    try
    {
      hr = taskDownload.get();
      if (FAILED(hr))
        throw hr;

      //attach the MS
      spRootPlaylist->AttachMediaSource(this);
      //parse the playlist
      spRootPlaylist->Parse();

      if (!spRootPlaylist->IsValid || (spRootPlaylist->IsVariant == false && spRootPlaylist->Segments.size() == 0))
        throw E_FAIL;

      //variant playlist
      if (spRootPlaylist->IsVariant)
      {
        //Set the bandwidth range on the network monitor
        spHeuristicsManager->SetBandwidthRange(spRootPlaylist->GetBitrateList());
      }
      //raise controller ready - opportunity for player to specifiy bitrate parameters
      if (nullptr != cpControllerFactory)
        cpControllerFactory->RaiseControllerReady(this->cpController);

      if (spRootPlaylist->PlaylistBatch.empty() == false)
      {
        for (auto itm : spRootPlaylist->PlaylistBatch)
          spRootPlaylist->AddToBatch(itm.first);
      }

      //variant playlist
      if (spRootPlaylist->IsVariant)
      {

        auto targetBitrate = spRootPlaylist->StartBitrate != 0 ?
          spHeuristicsManager->FindClosestBitrate(spRootPlaylist->StartBitrate) :
          spHeuristicsManager->GetLastSuggestedBandwidth();

        shared_ptr<StreamInfo> variantStreamInfo = spRootPlaylist->ActivateStream(targetBitrate, false, 0, true);

        if (nullptr == variantStreamInfo)
        {
          cpAsyncResultForOpen->SetStatus(E_FAIL);
          throw E_FAIL;
        }



        spHeuristicsManager->SetLastSuggestedBandwidth(variantStreamInfo->Bandwidth);

        if (nullptr != cpController)
        {
          cpController->RaiseInitialBitrateSelected();

          auto activealtaudio = spRootPlaylist->ActiveVariant->GetActiveAudioRendition();
          if (activealtaudio != nullptr)
          {
            //download the rendition playlist if needed
            if (activealtaudio->spPlaylist == nullptr && activealtaudio->PlaylistUri.empty() == false && FAILED(activealtaudio->DownloadRenditionPlaylistAsync().get()))
              spRootPlaylist->ActiveVariant->SetActiveAudioRendition(nullptr);
            else
            {
               
              unsigned int startseqnum = 0;
              unsigned int matchcount = 0;
              auto AltStartSegSeq = activealtaudio->spPlaylist->FindAltRenditionMatchingSegment(spRootPlaylist->ActiveVariant->spPlaylist.get(),
                spRootPlaylist->IsLive ? spRootPlaylist->ActiveVariant->spPlaylist->FindLiveStartSegmentSequenceNumber() : spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetSequenceNumber());

              auto altseg = activealtaudio->spPlaylist->GetSegment(AltStartSegSeq);
              if (altseg == nullptr || altseg->GetCurrentState() != INMEMORYCACHE)
              {
                auto ret = Playlist::StartStreamingAsync(activealtaudio->spPlaylist.get(), AltStartSegSeq, false, true, true).get();
                hr = std::get<0>(ret);
                if (FAILED(hr))
                  spRootPlaylist->ActiveVariant->SetActiveAudioRendition(nullptr);
                else
                  activealtaudio->spPlaylist->SetCurrentSegmentTracker(AUDIO, activealtaudio->spPlaylist->GetSegment(AltStartSegSeq));
              }
              else
              {
                activealtaudio->spPlaylist->SetCurrentSegmentTracker(AUDIO, activealtaudio->spPlaylist->GetSegment(AltStartSegSeq));
              }

              if (spRootPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(VIDEO) != nullptr && activealtaudio->spPlaylist->GetCurrentSegmentTracker(AUDIO) != nullptr)
              {
                auto seg = spRootPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(VIDEO);
                if (seg->HasMediaType(VIDEO))
                {
                  auto startvidsample = seg->PeekNextSample(seg->GetPIDForMediaType(VIDEO), MFRATE_FORWARD);
                  if (startvidsample != nullptr)
                    activealtaudio->spPlaylist->GetCurrentSegmentTracker(AUDIO)->SetCurrentPosition(startvidsample->SamplePTS->ValueInTicks, TimestampMatch::ClosestLesserOrEqual, MFRATE_FORWARD, true);
                }
              }
            }
          }
        }

        //start network monitor
        spHeuristicsManager->StartNotifier();

        if (variantStreamInfo->IsActive) //if this is the active variant
        {
          //construct the streams
          ConstructStreams(variantStreamInfo.get());
        }

        //for event playlists we also get the first segment and in turn establish the proper sliding window start
        if (/*spRootPlaylist->ActiveVariant->spPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT && */
          spRootPlaylist->IsLive &&
          spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(spRootPlaylist->ActiveVariant->spPlaylist.get(),
            spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetSequenceNumber(), false, true, true).get();
          hr = std::get<0>(ret);
          if (SUCCEEDED(hr))
          {
            spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->Timeline.front()->ValueInTicks);
            spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowEnd = std::make_shared<Timestamp>(spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowStart->ValueInTicks + spRootPlaylist->ActiveVariant->spPlaylist->Segments.back()->CumulativeDuration);
          }
          else
            throw E_FAIL;
        }
      }
      else //not a variant master playlist - just a good old single bitrate .m3u8
      {

        //construct streams
        ConstructStreams(spRootPlaylist.get());

        auto StartSeg = (spRootPlaylist->IsLive ? spRootPlaylist->GetSegment(spRootPlaylist->FindLiveStartSegmentSequenceNumber()) : spRootPlaylist->Segments.front());


        if (StartSeg->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(spRootPlaylist.get(), StartSeg->SequenceNumber, false).get();
          if (FAILED(std::get<0>(ret)))
            throw E_FAIL;
        }

        //for event playlists we also get the first segment and in turn establish the proper sliding window start
        if (/*spRootPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT &&*/
          spRootPlaylist->IsLive &&
          spRootPlaylist->Segments.front()->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(spRootPlaylist.get(),
            spRootPlaylist->Segments.front()->GetSequenceNumber(), false, true, true).get();
          hr = std::get<0>(ret);
          if (SUCCEEDED(hr))
          {
            spRootPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(spRootPlaylist->Segments.front()->Timeline.front()->ValueInTicks);
            spRootPlaylist->SlidingWindowEnd = std::make_shared<Timestamp>(spRootPlaylist->SlidingWindowStart->ValueInTicks + spRootPlaylist->Segments.back()->CumulativeDuration);

          }
          else
            throw E_FAIL;
        }
      }
    }
    catch (...)
    {
      cpAsyncResultForOpen->SetStatus(E_FAIL);
      if (spHeuristicsManager != nullptr)
        spHeuristicsManager->StopNotifier();
    }

    UnblockReleaseBlock(tceProtectPlaylist);
    EndOpen();
    return;

  }, task_options(task_continuation_context::use_arbitrary()));

  return S_OK;
}

HRESULT CHLSMediaSource::BeginOpen(IMFAsyncResult *pAsyncResult, Microsoft::HLSClient::HLSControllerFactory^ cpFactory, wstring PlaylistData)
{
  //LOG("MediaSource BeginOpen()");
  HRESULT hr = S_OK;
  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);


  try
  {
    cpControllerFactory = cpFactory;
    //reset the last suggested bandwidth - it will get recalculated later in the code
    spHeuristicsManager = make_shared<HeuristicsManager>(this);
    spDownloadRegistry = make_shared<ContentDownloadRegistry>();
    //set state flag
    SetCurrentState(MediaSourceState::MSS_OPENING);

    //bad URI
    if (uri.empty() || Helpers::SplitUri(uri, baseUri, topPlaylistFilename) == false)
      throw E_FAIL;

    //store the calllback interface
    cpAsyncResultForOpen = pAsyncResult;


    spRootPlaylist = make_shared<Playlist>(PlaylistData, baseUri, topPlaylistFilename);


    //attach the MS
    spRootPlaylist->AttachMediaSource(this);
    //parse the playlist
    spRootPlaylist->Parse();

    if (!spRootPlaylist->IsValid || (spRootPlaylist->IsVariant == false && spRootPlaylist->Segments.size() == 0))
      throw E_FAIL;
    //variant playlist
    if (spRootPlaylist->IsVariant)
    {
      //Set the bandwidth range on the network monitor
      spHeuristicsManager->SetBandwidthRange(spRootPlaylist->GetBitrateList());
    }
    //raise controller ready - opportunity for player to specifiy bitrate parameters
    if (nullptr != cpControllerFactory)
      cpControllerFactory->RaiseControllerReady(this->cpController);

    if (spRootPlaylist->PlaylistBatch.empty() == false)
    {
      for (auto itm : spRootPlaylist->PlaylistBatch)
        spRootPlaylist->AddToBatch(itm.first);
    }

    //variant playlist
    if (spRootPlaylist->IsVariant)
    {

      auto targetBitrate = spRootPlaylist->StartBitrate != 0 ?
        spHeuristicsManager->FindClosestBitrate(spRootPlaylist->StartBitrate) :
        spHeuristicsManager->GetLastSuggestedBandwidth();

      shared_ptr<StreamInfo> variantStreamInfo = spRootPlaylist->ActivateStream(targetBitrate, false, 0, true);

      if (nullptr == variantStreamInfo)
      {
        cpAsyncResultForOpen->SetStatus(hr);
        throw E_FAIL;
      }


      spHeuristicsManager->SetLastSuggestedBandwidth(variantStreamInfo->Bandwidth);

      if (nullptr != cpController)
      {
        cpController->RaiseInitialBitrateSelected();

        auto activealtaudio = spRootPlaylist->ActiveVariant->GetActiveAudioRendition();
        if (activealtaudio != nullptr)
        {
          //download the rendition playlist if needed
          if (activealtaudio->spPlaylist == nullptr && activealtaudio->PlaylistUri.empty() == false && FAILED(activealtaudio->DownloadRenditionPlaylistAsync().get()))
            spRootPlaylist->ActiveVariant->SetActiveAudioRendition(nullptr);
          else{
 
            unsigned int startseqnum = 0;
            unsigned int matchcount = 0;
            auto AltStartSegSeq = activealtaudio->spPlaylist->FindAltRenditionMatchingSegment(spRootPlaylist->ActiveVariant->spPlaylist.get(),
              spRootPlaylist->IsLive ? spRootPlaylist->ActiveVariant->spPlaylist->FindLiveStartSegmentSequenceNumber() :
              spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetSequenceNumber());

            auto altseg = activealtaudio->spPlaylist->GetSegment(AltStartSegSeq);
            if (altseg == nullptr || altseg->GetCurrentState() != INMEMORYCACHE)
            {
              auto ret = Playlist::StartStreamingAsync(activealtaudio->spPlaylist.get(), AltStartSegSeq, false, true, true).get();
              hr = std::get<0>(ret);
              if (FAILED(hr))
                spRootPlaylist->ActiveVariant->SetActiveAudioRendition(nullptr);
              else
                activealtaudio->spPlaylist->SetCurrentSegmentTracker(AUDIO, activealtaudio->spPlaylist->GetSegment(AltStartSegSeq));
            }
            else
            {
              activealtaudio->spPlaylist->SetCurrentSegmentTracker(AUDIO, activealtaudio->spPlaylist->GetSegment(AltStartSegSeq));
            }

            if (spRootPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(VIDEO) != nullptr && activealtaudio->spPlaylist->GetCurrentSegmentTracker(AUDIO) != nullptr)
            {
              auto seg = spRootPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(VIDEO);
              if (seg->HasMediaType(VIDEO))
              {
                auto startvidsample = seg->PeekNextSample(seg->GetPIDForMediaType(VIDEO), MFRATE_FORWARD);
                if (startvidsample != nullptr)
                  activealtaudio->spPlaylist->GetCurrentSegmentTracker(AUDIO)->SetCurrentPosition(startvidsample->SamplePTS->ValueInTicks, TimestampMatch::ClosestLesserOrEqual, MFRATE_FORWARD, true);
              }
            }
          }
        }
      }
      //start network monitor
      spHeuristicsManager->StartNotifier();

      if (variantStreamInfo->IsActive) //if this is the active variant
      {
        //construct the streams
        ConstructStreams(variantStreamInfo.get());

      }

      //for event playlists we also get the first segment and in turn establish the proper sliding window start
      if (/*spRootPlaylist->ActiveVariant->spPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT &&*/
        spRootPlaylist->IsLive &&
        spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetCurrentState() != INMEMORYCACHE)
      {
        auto ret = Playlist::StartStreamingAsync(spRootPlaylist->ActiveVariant->spPlaylist.get(),
          spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->GetSequenceNumber(), false, true, true).get();
        hr = std::get<0>(ret);
        if (SUCCEEDED(hr))
        {
          spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(spRootPlaylist->ActiveVariant->spPlaylist->Segments.front()->Timeline.front()->ValueInTicks);
          spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowEnd = std::make_shared<Timestamp>(spRootPlaylist->ActiveVariant->spPlaylist->SlidingWindowStart->ValueInTicks + spRootPlaylist->ActiveVariant->spPlaylist->Segments.back()->CumulativeDuration);

        }
        else
          throw E_FAIL;
      }
    }
    else //not a variant master playlist - just a good old single bitrate .m3u8
    {
      //construct streams
      ConstructStreams(spRootPlaylist.get());

      auto StartSeg = (spRootPlaylist->IsLive ? spRootPlaylist->GetSegment(spRootPlaylist->FindLiveStartSegmentSequenceNumber()) : spRootPlaylist->Segments.front());

      if (StartSeg->GetCurrentState() != INMEMORYCACHE)
      {
        auto ret = Playlist::StartStreamingAsync(spRootPlaylist.get(), StartSeg->SequenceNumber, false).get();
        if (FAILED(std::get<0>(ret)))
          throw E_FAIL;
      }

      //for event playlists we also get the first segment and in turn establish the proper sliding window start
      if (/*spRootPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT &&*/
        spRootPlaylist->IsLive &&
        spRootPlaylist->Segments.front()->GetCurrentState() != INMEMORYCACHE)
      {
        auto ret = Playlist::StartStreamingAsync(spRootPlaylist.get(),
          spRootPlaylist->Segments.front()->GetSequenceNumber(), false, true, true).get();
        hr = std::get<0>(ret);
        if (SUCCEEDED(hr))
        {
          spRootPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(spRootPlaylist->Segments.front()->Timeline.front()->ValueInTicks);
          spRootPlaylist->SlidingWindowEnd = std::make_shared<Timestamp>(spRootPlaylist->SlidingWindowStart->ValueInTicks + spRootPlaylist->Segments.back()->CumulativeDuration);

        }
        else
          throw E_FAIL;
      }
    }
  }
  catch (...)
  {
    cpAsyncResultForOpen->SetStatus(E_FAIL);
    if (spHeuristicsManager != nullptr)
      spHeuristicsManager->StopNotifier();
    hr = E_FAIL;
  }


  UnblockReleaseBlock(tceProtectPlaylist);

  //signal completion of initialization 
  EndOpen();
  return hr;
}

///<summary>Called to complete the asynchronous BeginOpen()</summary>
void CHLSMediaSource::EndOpen()
{

  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);

  if (SUCCEEDED(cpAsyncResultForOpen->GetStatus()) && spRootPlaylist != nullptr)
  {



    //set the state flag
    if (GetCurrentState() != MSS_ERROR)
    {

      SetCurrentState(MediaSourceState::MSS_STARTING);
      //monitor bandwidth changes if the root playlist is a variant master and config allows bitrate monitoring
      if (spRootPlaylist != nullptr && spRootPlaylist->IsVariant)
        //attach handler to bitrate monitor
        spHeuristicsManager->BitrateSwitchSuggested = [this](unsigned int Bandwidth, unsigned int LastMeasured, bool& Cancel) { BitrateSwitchSuggested(Bandwidth, LastMeasured, Cancel); };

    }
  }
  if (spRootPlaylist == nullptr || GetCurrentState() == MSS_ERROR)
    cpAsyncResultForOpen->SetStatus(E_FAIL);
  //LOG("MediaSource EndOpen()");
  //call the calling byte stream handler (HLSPlaylistHandler) back - HLS Media Source is now ready or failed
  MFInvokeCallback(cpAsyncResultForOpen.Get());

  UnblockReleaseBlock(tceProtectPlaylist);

  cpAsyncResultForOpen = nullptr;
}

///<summary>Constructs the MF streams</summary>
///<param name='pPlaylist'>The playlist for the currently active variant (not the master playlist)</param>
HRESULT CHLSMediaSource::ConstructStreams(Playlist *pPlaylist)
{

  HRESULT hr = S_OK;
  //start streaming the playlist and wait for a download
  //Playlist::StartStreamingAsync(pPlaylist).wait();//wait() or get() ?  
  //if the audio stream does not already exist
  if (cpAudioStream == nullptr)
  {
    //create the audio stream
    cpAudioStream = WRL::Make<CMFAudioStream>(this, pPlaylist, StreamWorkQueueID);
    //create the SD
    if (FAILED(hr = cpAudioStream->ConstructStreamDescriptor(AUDIOSTREAMID))) return hr;
  }
  //if the video stream does not already exist
  if (cpVideoStream == nullptr)
  {
    //create the video stream
    cpVideoStream = WRL::Make<CMFVideoStream>(this, pPlaylist, StreamWorkQueueID);
    //create the SD
    if (FAILED(hr = cpVideoStream->ConstructStreamDescriptor(VIDEOSTREAMID))) return hr;
  }

  return hr;
}

HRESULT CHLSMediaSource::ConstructStreams(StreamInfo *pVariantStreamInfo)
{

  HRESULT hr = S_OK;


  if (cpAudioStream == nullptr)
  {
    //create the audio stream
    cpAudioStream = WRL::Make<CMFAudioStream>(this,
      pVariantStreamInfo->GetActiveAudioRendition() != nullptr ? pVariantStreamInfo->GetActiveAudioRendition()->spPlaylist.get() : pVariantStreamInfo->spPlaylist.get(),
      StreamWorkQueueID);
    //create the SD
    if (FAILED(hr = cpAudioStream->ConstructStreamDescriptor(AUDIOSTREAMID))) return hr;
  }
  //if the video stream does not already exist
  if (cpVideoStream == nullptr)
  {
    //create the video stream
    cpVideoStream = WRL::Make<CMFVideoStream>(this,
      pVariantStreamInfo->GetActiveVideoRendition() != nullptr ? pVariantStreamInfo->GetActiveVideoRendition()->spPlaylist.get() : pVariantStreamInfo->spPlaylist.get(),
      StreamWorkQueueID);
    //create the SD
    if (FAILED(hr = cpVideoStream->ConstructStreamDescriptor(VIDEOSTREAMID))) return hr;
  }

  return hr;
}

#pragma endregion

#pragma region PLUMBING
///<summary>CreatePresentationDescriptor (see IMFMediaSource on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::CreatePresentationDescriptor(IMFPresentationDescriptor **ppPresentationDescriptor)
{
  HRESULT hr = S_OK;
  if (cpAudioStream == nullptr || cpVideoStream == nullptr)
    return E_FAIL;

  if (cpPresentationDescriptor != nullptr)
    return cpPresentationDescriptor->Clone(ppPresentationDescriptor);

  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);

  //we set up for audio only if
  // - the startseg has no video, and 
  // - the playlist is variant and we have no other variants with a non null video codec type, OR the playlist is not variant
  // OR tere is a ContentType filter for audio only
  //similar logic for video only
  try
  {
    auto StartSeg = spRootPlaylist->IsVariant == false ?
      (spRootPlaylist->IsLive ? spRootPlaylist->GetSegment(spRootPlaylist->FindLiveStartSegmentSequenceNumber()) : spRootPlaylist->Segments.front()) :
      (spRootPlaylist->ActiveVariant->spPlaylist->IsLive ?
      spRootPlaylist->ActiveVariant->spPlaylist->GetSegment(spRootPlaylist->ActiveVariant->spPlaylist->FindLiveStartSegmentSequenceNumber()) :
      spRootPlaylist->ActiveVariant->spPlaylist->Segments.front());

    if (StartSeg->GetCurrentState() != INMEMORYCACHE)
    {
      if (spRootPlaylist->IsVariant == false)
        Playlist::StartStreamingAsync(spRootPlaylist.get(), StartSeg->SequenceNumber, false).wait();
      else
        Playlist::StartStreamingAsync(spRootPlaylist->ActiveVariant->spPlaylist.get(), StartSeg->SequenceNumber, false).wait();
    }

    bool AudioOnly = (StartSeg->GetCurrentState() == INMEMORYCACHE && StartSeg->HasMediaType(ContentType::VIDEO) == false &&
      (spRootPlaylist->IsVariant == false ||
      (spRootPlaylist->IsVariant && spRootPlaylist->Variants.end() == std::find_if(spRootPlaylist->Variants.begin(), spRootPlaylist->Variants.end(), [this](std::pair<unsigned int, shared_ptr<StreamInfo>> pr)
    {
      return pr.second->VideoMediaType != GUID_NULL;
    })))) || Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO;

    bool VideoOnly = (StartSeg->GetCurrentState() == INMEMORYCACHE && StartSeg->HasMediaType(ContentType::AUDIO) == false &&
      (spRootPlaylist->IsVariant == false ||
      (spRootPlaylist->IsVariant && spRootPlaylist->Variants.end() == std::find_if(spRootPlaylist->Variants.begin(), spRootPlaylist->Variants.end(), [this](std::pair<unsigned int, shared_ptr<StreamInfo>> pr)
    {
      return pr.second->AudioMediaType != GUID_NULL;
    })))) || Configuration::GetCurrent()->ContentTypeFilter == ContentType::VIDEO;

    if (AudioOnly || VideoOnly)
    {
      IMFStreamDescriptor * streamDescriptors[1];

      if (AudioOnly)
      {
        if (FAILED(hr = cpAudioStream->GetStreamDescriptor(&streamDescriptors[0]))) throw hr;
        if (FAILED(hr = MFCreatePresentationDescriptor(1, streamDescriptors, &cpPresentationDescriptor))) throw hr;
        if (FAILED(hr = cpAudioStream->Select(cpPresentationDescriptor, 0))) throw hr;
      }
      else
      {
        if (FAILED(hr = cpVideoStream->GetStreamDescriptor(&streamDescriptors[0]))) throw hr;
        if (FAILED(hr = MFCreatePresentationDescriptor(1, streamDescriptors, &cpPresentationDescriptor))) throw hr;
        if (FAILED(hr = cpVideoStream->Select(cpPresentationDescriptor, 0))) throw hr;

      }
    }
    else
    {
      IMFStreamDescriptor * streamDescriptors[2];

      if (FAILED(hr = cpAudioStream->GetStreamDescriptor(&streamDescriptors[AUDIOSTREAMID]))) throw hr;
      if (FAILED(hr = cpVideoStream->GetStreamDescriptor(&streamDescriptors[VIDEOSTREAMID]))) throw hr;
      if (FAILED(hr = MFCreatePresentationDescriptor(2, streamDescriptors, &cpPresentationDescriptor))) throw hr;
      if (FAILED(hr = cpAudioStream->Select(cpPresentationDescriptor, AUDIOSTREAMID))) throw hr;
      if (FAILED(hr = cpVideoStream->Select(cpPresentationDescriptor, VIDEOSTREAMID))) throw hr;
    }



    if (!spRootPlaylist->IsLive)
    {
      if (FAILED(hr = cpPresentationDescriptor->SetUINT64(MF_PD_DURATION, spRootPlaylist->TotalDuration))) throw hr;
    }
    else
    {
      if (FAILED(hr = cpPresentationDescriptor->SetUINT64(MF_PD_DURATION, MAXLONGLONG))) throw hr;

    }
  }
  catch (HRESULT hr)
  {
    UnblockReleaseBlock(tceProtectPlaylist);
    return hr;
  }
  catch (...)
  {
    UnblockReleaseBlock(tceProtectPlaylist);
    return E_FAIL;
  }
  UnblockReleaseBlock(tceProtectPlaylist);
  return cpPresentationDescriptor->Clone(ppPresentationDescriptor);
}


///<summary>GetCharacteristics (see IMFMediaSource on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetCharacteristics(DWORD *pdwCharacteristics)
{

  *pdwCharacteristics = MFMEDIASOURCE_CAN_SEEK | MFMEDIASOURCE_CAN_PAUSE;// | MFMEDIASOURCE_HAS_SLOW_SEEK;
  if (spRootPlaylist != nullptr &&
    ((spRootPlaylist->IsVariant == false && spRootPlaylist->IsLive) ||
    (spRootPlaylist->ActiveVariant != nullptr && spRootPlaylist->ActiveVariant->spPlaylist != nullptr && spRootPlaylist->ActiveVariant->spPlaylist->IsLive)))
    *pdwCharacteristics |= MFMEDIASOURCE_IS_LIVE;
  return S_OK;
}

///<summary>GetService (see IMFGetService on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetService(REFGUID guidService, REFIID riid, LPVOID *ppvObject)
{
  if (MF_RATE_CONTROL_SERVICE == guidService)
  {
    if (riid == __uuidof(IMFRateControl) || riid == __uuidof(IMFRateSupport))
    {
      return this->QueryInterface(riid, ppvObject);
    }
    else
      return MF_E_UNSUPPORTED_SERVICE;
  }
  /*else if (MF_QUALITY_SERVICES == guidService)
  {
  if (riid == __uuidof(IMFQualityAdvise))
  {
  return this->QueryInterface(riid, ppvObject);
  }
  else
  return MF_E_UNSUPPORTED_SERVICE;
  }*/
  else
    return MF_E_UNSUPPORTED_SERVICE;
}

///<summary>See IMFAsyncCallback in MSDN</summary>
IFACEMETHODIMP CHLSMediaSource::GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
{

  *pdwFlags = 0;
  //return our serial work queue
  *pdwQueue = SerialWorkQueueID;
  return S_OK;
}

///<summary>BeginGetEvent (see IMFMediaEventGenerator on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED || sourceEventQueue == nullptr) return MF_E_SHUTDOWN;
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = sourceEventQueue->BeginGetEvent(pCallback, punkState);

  return hr;
}

///<summary>EndGetEvent (see IMFMediaEventGenerator on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED || sourceEventQueue == nullptr) return MF_E_SHUTDOWN;
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = sourceEventQueue->EndGetEvent(pResult, ppEvent);
  return hr;
}

///<summary>GetEvent (see IMFMediaEventGenerator on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent)
{
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED || sourceEventQueue == nullptr) return MF_E_SHUTDOWN;
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = sourceEventQueue->GetEvent(dwFlags, ppEvent);

  return hr;
}

///<summary>QueueEvent (see IMFMediaEventGenerator on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED || sourceEventQueue == nullptr) return MF_E_SHUTDOWN;
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = sourceEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
  return hr;
}

IFACEMETHODIMP CHLSMediaSource::QueueEvent(IMFMediaEvent* evt)
{
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED || sourceEventQueue == nullptr) return MF_E_SHUTDOWN;
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = sourceEventQueue->QueueEvent(evt);
  return hr;
}

///<summary>Notify (MENewStream) that a new stream has been created</summary>
///<param name='pStream'>Stream instance</param>
HRESULT CHLSMediaSource::NotifyNewStream(IMFMediaStream *pStream)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_UNKNOWN;
  pStream->QueryInterface(IID_PPV_ARGS(&(pvar.punkVal)));
  hr = QueueEvent(MENewStream, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Notify (MEUpdatedStream) that a stream has been updated</summary>
///<param name='pStream'>Stream instance</param>
HRESULT CHLSMediaSource::NotifyStreamUpdated(IMFMediaStream *pStream)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_UNKNOWN;
  pStream->QueryInterface(IID_PPV_ARGS(&(pvar.punkVal)));
  hr = QueueEvent(MEUpdatedStream, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Notify (MESourceStarted) that a source has been started</summary>
///<param name='posTS'>Start position</param>
///<param name='hrStatus'>Indicates success or failure in starting the source</param>
HRESULT CHLSMediaSource::NotifyStarted(std::shared_ptr<Timestamp> posTS, HRESULT hrStatus)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  if (posTS != nullptr)
  {
    pvar.vt = VT_I8;
    pvar.hVal.QuadPart = posTS->ValueInTicks;
  }
  else
    pvar.vt = VT_EMPTY;
  hr = QueueEvent(MESourceStarted, GUID_NULL, hrStatus, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

HRESULT CHLSMediaSource::NotifyActualStart(std::shared_ptr<Timestamp> posTS, HRESULT hrStatus)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_EMPTY;

  ComPtr<IMFMediaEvent> mediaEvt;
  if (FAILED(hr = MFCreateMediaEvent(MESourceStarted, GUID_NULL, hrStatus, &pvar, &mediaEvt))) return hr;
  ComPtr<IMFAttributes> mediaEvtAttribs;
  mediaEvt.As(&mediaEvtAttribs);
  mediaEvtAttribs->SetUINT64(MF_EVENT_SOURCE_ACTUAL_START, posTS->ValueInTicks);

  hr = QueueEvent(mediaEvt.Detach());

  ::PropVariantClear(&pvar);
  return hr;
}

HRESULT CHLSMediaSource::NotifySeeked(std::shared_ptr<Timestamp> posTS, HRESULT hrStatus)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  if (posTS != nullptr)
  {
    pvar.vt = VT_I8;
    pvar.hVal.QuadPart = posTS->ValueInTicks;
  }
  else
    pvar.vt = VT_EMPTY;
  hr = QueueEvent(MESourceSeeked, GUID_NULL, hrStatus, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}


///<summary>Notify (MESourcePaused) that a source has been paused</summary> 
HRESULT CHLSMediaSource::NotifyPaused()
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MESourcePaused, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Notify (MESourceStopped) that a source has been stopped</summary> 
HRESULT CHLSMediaSource::NotifyStopped()
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MESourceStopped, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}


///<summary>Send end notification</summary>
///<param name='startAt'>Position to start at</param>
HRESULT CHLSMediaSource::NotifyPresentationEnded()
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_EMPTY;
  hr = QueueEvent(MEEndOfPresentation, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}


///<summary>Notify (MESourceRateChanged) that playback rate has changed</summary>
///<param name='newRate'>The new playback rate</param>
HRESULT CHLSMediaSource::NotifyRateChanged(float newRate)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_R4;
  pvar.fltVal = newRate;
  hr = QueueEvent(MESourceRateChanged, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

#pragma endregion

#pragma region BUFFERING
///<summary>Puts the media source in a state of buffering</summary>
HRESULT CHLSMediaSource::StartBuffering(bool Record,bool CanInitiateDownSwitch)
{
  HRESULT hr = S_OK;
  auto oldState = GetCurrentState();
  //if we are in a valid state
  if (oldState == MediaSourceState::MSS_STARTED || oldState == MediaSourceState::MSS_PAUSED)
  {
   
    PROPVARIANT  pvt2;
    ::PropVariantInit(&pvt2);
    //queue the buffering started event
    if (GetCurrentPlaybackRate()->Rate == 1.0)
      QueueEvent(MEBufferingStarted, GUID_NULL, S_OK, &pvt2);
    //change state flag
    preBufferingState = GetCurrentState();
    SetCurrentState(MediaSourceState::MSS_BUFFERING);
    ::PropVariantClear(&pvt2);
    LOG("Source buffering started...");

    //toggle any downshift tolerance if applicable - we will resume tolerance at next upshift
    if (spHeuristicsManager != nullptr && spHeuristicsManager->IsBitrateChangeNotifierRunning() && Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift != 0.0)
      spHeuristicsManager->SetIgnoreDownshiftTolerance(true);

    //if we are buffering then we potentially need to downshift 
    if (CanInitiateDownSwitch && spHeuristicsManager != nullptr &&
      Configuration::GetCurrent()->EnableBitrateMonitor == true &&
      spHeuristicsManager->IsBitrateChangeNotifierRunning() &&
      oldState == MediaSourceState::MSS_STARTED &&
      spRootPlaylist->IsVariant && spRootPlaylist->ActiveVariant != nullptr)
    {
      spHeuristicsManager->NotifyBitrateChangeIfNeeded(spRootPlaylist->ActiveVariant->Bandwidth, spRootPlaylist->ActiveVariant->Bandwidth);
    }
  }
  return hr;
}

///<summary>Checks to see if the media source is buffering</summary>
bool CHLSMediaSource::IsBuffering()
{
  return (GetCurrentState() == MediaSourceState::MSS_BUFFERING);
}

///<summary>Puts the media source out of buffering state</summary>
HRESULT CHLSMediaSource::EndBuffering()
{
  HRESULT hr = S_OK;
  //if we are buffering
  if (GetCurrentState() == MediaSourceState::MSS_BUFFERING)
  {

    PROPVARIANT pvt2;
    ::PropVariantInit(&pvt2);
    //queue the buffering stopped event
    if (GetCurrentPlaybackRate()->Rate == 1.0)
      QueueEvent(MEBufferingStopped, GUID_NULL, S_OK, &pvt2);
    //reset state
    SetCurrentState(preBufferingState);
    preBufferingState = MediaSourceState::MSS_UNINITIALIZED;
    ::PropVariantClear(&pvt2);
    LOG("Source buffering ended...");
  }
  return hr;
}

#pragma endregion

#pragma region BITRATE/RENDITION CHANGES

///<summary>Cancels any pending bitrate changes</summary>
bool CHLSMediaSource::TryCancelPendingBitrateSwitch(bool Force)
{
  if (spRootPlaylist == nullptr || (spRootPlaylist != nullptr && !spRootPlaylist->IsVariant)) return false;

  std::lock(cpVideoStream->LockSwitch, cpAudioStream->LockSwitch);
  std::lock_guard<std::recursive_mutex> lockV(cpVideoStream->LockSwitch, adopt_lock);
  std::lock_guard<std::recursive_mutex> lockA(cpAudioStream->LockSwitch, adopt_lock);

  auto videoswitch = cpVideoStream->GetPendingBitrateSwitch();
  auto audioswitch = cpAudioStream->GetPendingBitrateSwitch();

  if (videoswitch == nullptr && audioswitch == nullptr)
    return true;

  if (audioswitch != nullptr && audioswitch->VideoSwitchedTo != nullptr && !Force) //switch half done
    return false;

  unsigned int targetBitrate = videoswitch != nullptr ? videoswitch->targetPlaylist->pParentStream->Bandwidth : (audioswitch != nullptr ? audioswitch->targetPlaylist->pParentStream->Bandwidth : 0);
  //if we have a selected video stream
  if (videoswitch != nullptr)
  {
    //stop any ongoing downloads
    videoswitch->targetPlaylist->CancelDownloads();
    //cancel the pending bitrate change on the video stream
    cpVideoStream->ScheduleSwitch(nullptr, PlaylistSwitchRequest::SwitchType::BITRATE);
  }
  //if we have a selected audio stream
  if (audioswitch != nullptr)
  {
    //stop any ongoing downloads
    audioswitch->targetPlaylist->CancelDownloads();
    //cancel the pending bitrate change on the audio stream
    cpAudioStream->ScheduleSwitch(nullptr, PlaylistSwitchRequest::SwitchType::BITRATE);
  }

  //revert the last suggested bandwidth 
  if ((videoswitch != nullptr || audioswitch != nullptr) && spHeuristicsManager->GetLastSuggestedBandwidth() != spRootPlaylist->ActiveVariant->Bandwidth)
    //set the monitor last suggested bandwidth accordingly
    spHeuristicsManager->SetLastSuggestedBandwidth(spRootPlaylist->ActiveVariant->Bandwidth);

  LOGIF(spRootPlaylist->ActiveVariant != nullptr, "Cancelled suggested bitrate change to " << targetBitrate << " from " << spRootPlaylist->ActiveVariant->Bandwidth);

  if ((videoswitch != nullptr || audioswitch != nullptr) && cpController != nullptr && cpController->GetPlaylist() != nullptr && (GetCurrentState() == MediaSourceState::MSS_STARTED || GetCurrentState() == MediaSourceState::MSS_BUFFERING || GetCurrentState() == MediaSourceState::MSS_SEEKING))
  {
    if (IsBitrateLocked())
      SetBitrateLock(false);
    auto currentBR = spRootPlaylist->ActiveVariant->Bandwidth;
    if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
    {
      protectionRegistry.Register(task<HRESULT>([currentBR, targetBitrate, this]()
      {
        //raise the change cancelled event
        if (GetCurrentState() != MSS_STOPPED && GetCurrentState() != MSS_UNINITIALIZED && GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_PAUSED
          && cpController != nullptr && cpController->GetPlaylist() != nullptr)
          cpController->GetPlaylist()->RaiseBitrateSwitchCancelled(currentBR, targetBitrate);
        return S_OK;
      }));
    }
  }
  return true;
}

///<summary>Cancels any pending bitrate changes</summary>
void CHLSMediaSource::CancelPendingRenditionChange()
{

  std::lock(cpVideoStream->LockSwitch, cpAudioStream->LockSwitch);
  std::lock_guard<std::recursive_mutex> lockV(cpVideoStream->LockSwitch, adopt_lock);
  std::lock_guard<std::recursive_mutex> lockA(cpAudioStream->LockSwitch, adopt_lock);

  auto videoswitch = cpVideoStream->GetPendingRenditionSwitch();
  auto audioswitch = cpAudioStream->GetPendingRenditionSwitch();
  //if we have a selected video stream
  if (videoswitch != nullptr)
  {

    //stop any ongoing downloads
    videoswitch->targetPlaylist->CancelDownloads();
    //cancel the pending rendition change on the video stream
    cpVideoStream->ScheduleSwitch(nullptr, PlaylistSwitchRequest::SwitchType::RENDITION);
  }
  //if we have a selected audio stream
  if (audioswitch != nullptr)
  {

    audioswitch->targetPlaylist->CancelDownloads();
    //cancel the pending rendition change on the audio stream
    cpAudioStream->ScheduleSwitch(nullptr, PlaylistSwitchRequest::SwitchType::RENDITION);
    LOG("Cancelling rendition switch");
  }
  return;
}

///<summary>Handles an incoming bitrate switch change suggestion from the bitrate monitor</summary>
///<param name='Bandwidth'>The suggested bitrate</param>
///<param name='Cancel'>Set to true if player cancels the bitrate switch - used to inform the bitrate monitor of the cancellation</param>
void CHLSMediaSource::BitrateSwitchSuggested(unsigned int Bandwidth, unsigned int LastMeasured, bool& Cancel, bool IgnoreBuffer)
{

  /* std::lock(cpVideoStream->LockSwitch, cpAudioStream->LockSwitch);
   std::lock_guard<std::recursive_mutex> lockV(cpVideoStream->LockSwitch, adopt_lock);
   std::lock_guard<std::recursive_mutex> lockA(cpAudioStream->LockSwitch, adopt_lock);*/


  auto curPlaylist = spRootPlaylist->ActiveVariant->spPlaylist;
  auto curBitrate = spRootPlaylist->ActiveVariant->Bandwidth;



  //only do if the current state is started
  if (GetCurrentState() != MediaSourceState::MSS_STARTED && GetCurrentState() != MSS_BUFFERING)
  {
    if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
      //set the monitor last suggested bandwidth accordingly
      spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);
    return;
  }



  //check to see if the target bandwidth falls outside allowed range  and make sure we are playing at normal playback rate  
  if ((spRootPlaylist->MinAllowedBitrate != 0 && Bandwidth < spRootPlaylist->MinAllowedBitrate) ||
    (spRootPlaylist->MaxAllowedBitrate != UINT32_MAX && Bandwidth > spRootPlaylist->MaxAllowedBitrate) ||
    GetCurrentPlaybackRate()->Rate != 1.0)
  {
    //ignore the change
    if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
      //set the monitor last suggested bandwidth accordingly
      spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);
    return;
  }

  //get the target variant
  auto targetVariant = spRootPlaylist->Variants[Bandwidth];

  std::shared_ptr<PlaylistSwitchRequest> pendingVideoSwitch = nullptr;
  std::shared_ptr<PlaylistSwitchRequest> pendingAudioSwitch = nullptr;

  if (std::try_lock(cpVideoStream->LockSwitch, cpAudioStream->LockSwitch) < 0)
  {
    std::lock_guard<std::recursive_mutex> lockv(cpVideoStream->LockSwitch, adopt_lock);
    std::lock_guard<std::recursive_mutex> locka(cpAudioStream->LockSwitch, adopt_lock);

    pendingVideoSwitch = cpVideoStream->GetPendingBitrateSwitch();
    pendingAudioSwitch = cpAudioStream->GetPendingBitrateSwitch();


    if (pendingVideoSwitch != nullptr || pendingAudioSwitch != nullptr)//switch pending
    {

      unsigned int pendingShiftBandwidth = 0;
      if (pendingVideoSwitch != nullptr)
        pendingShiftBandwidth = pendingVideoSwitch->targetPlaylist->pParentStream->Bandwidth;
      else if (pendingAudioSwitch != nullptr)
        pendingShiftBandwidth = pendingAudioSwitch->targetPlaylist->pParentStream->Bandwidth;

      //if we have a pending upshift and the new suggestion is the same as or lower the current bitrate
      if (!IsBitrateLocked() && Bandwidth <= curBitrate &&
        ((pendingVideoSwitch != nullptr && pendingVideoSwitch->VideoSwitchedTo == nullptr) ||
        (pendingVideoSwitch == nullptr && pendingAudioSwitch != nullptr)) && pendingShiftBandwidth > curBitrate)
      {
        //cancel the pending shift
        curPlaylist->PauseBufferBuilding = false;
        TryCancelPendingBitrateSwitch(true);
        if(Bandwidth == curBitrate) //stop processing only if the new suggestion is the same as current
          return;
      }

      if (IsBitrateLocked())
      {
        if (spHeuristicsManager->GetLastSuggestedBandwidth() != pendingShiftBandwidth)  //reset the bandwidth
          //set the monitor last suggested bandwidth accordingly
          spHeuristicsManager->SetLastSuggestedBandwidth(pendingShiftBandwidth);
        return;
      }
      //if pending switch is upshift and suggested switch is upshift and to a higher bitrate - ignore current - let runtime do it in steps when it comes around next time
      //i.e. we have 300 -> 500 pending and new suggestion is 700
      //OR if pending switch is downshift and suggested switch is downshift but to a higher bitrate - ignore current - let runtime do it in steps when it comes around next time
      //i.e. we have 700 -> 300 and the new suggestion is 500 
      else if ((pendingShiftBandwidth > curBitrate && Bandwidth > pendingShiftBandwidth) ||
        (pendingShiftBandwidth < curBitrate && Bandwidth > pendingShiftBandwidth))
      {
        //ignore the change
        if (spHeuristicsManager->GetLastSuggestedBandwidth() != pendingShiftBandwidth)  //reset the bandwidth
          //set the monitor last suggested bandwidth accordingly
          spHeuristicsManager->SetLastSuggestedBandwidth(pendingShiftBandwidth);
        return;
      }
    }
    else
    {
      if (IsBitrateLocked())
      {
        if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
          //set the monitor last suggested bandwidth accordingly
          spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);
        return;
      }
    }
    //}

    if (targetVariant->spPlaylist == nullptr)
    {
      targetVariant = spRootPlaylist->DownloadVariantStreamPlaylist(targetVariant->Bandwidth,
        //if we are trying to go up and the target is the very next bitrate and we cannot get that playlist - fail fast - no where else to go - else search
        targetVariant->Bandwidth > curBitrate && spHeuristicsManager->FindNextHigherBitrate(curBitrate) == targetVariant->Bandwidth ? true : false,
        -1);
    }

    auto cursegment = curPlaylist->MaxCurrentSegment();
    if (curPlaylist->IsLive == false &&
      cursegment != nullptr &&
      curPlaylist->GetCurrentLABLength(cursegment->GetSequenceNumber(), false, 2 * curPlaylist->DerivedTargetDuration) >= 2 * curPlaylist->DerivedTargetDuration
      && !(targetVariant == nullptr || targetVariant->spPlaylist == nullptr || targetVariant->Bandwidth == spRootPlaylist->ActiveVariant->Bandwidth))
    {
      //if this is VOD playlist and we have at least twice the target duration buffered - stop buffering to reduce parallel downloads
      curPlaylist->PauseBufferBuilding = true;
    }

    if (targetVariant == nullptr || targetVariant->spPlaylist == nullptr || targetVariant->Bandwidth == spRootPlaylist->ActiveVariant->Bandwidth)
    {
      //ignore the change
      if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
        //set the monitor last suggested bandwidth accordingly
        spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);
      curPlaylist->PauseBufferBuilding = false;
      return;
    }



    LOG("Suggesting Bitrate Switch from " << curBitrate << " to " << Bandwidth << " with last measured bandwidth at " << LastMeasured);
    //if we have a valid controller
    if (cpController != nullptr && cpController->GetPlaylist() != nullptr &&
      GetCurrentState() != MSS_STOPPED && GetCurrentState() != MSS_UNINITIALIZED &&
      GetCurrentState() != MSS_ERROR  && GetCurrentState() != MSS_PAUSED && GetCurrentState() != MSS_SEEKING)
    {
      //raise the change suggested event
      cpController->GetPlaylist()->RaiseBitrateSwitchSuggested(curBitrate, Bandwidth, LastMeasured, Cancel);
      //if player wants to cancel
      if (Cancel)
      {
        //ignore the change
        if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
          //set the monitor last suggested bandwidth accordingly
          spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);
        curPlaylist->PauseBufferBuilding = false;
        return;
      }
    }

    //prepare the playlist for bitrate transfer to the target
    if (curPlaylist->PrepareBitrateTransfer(targetVariant->spPlaylist.get(), IgnoreBuffer))
    {
      //do these together
      //if there is a selected video stream
      if (cpVideoStream != nullptr && cpVideoStream->Selected() && spRootPlaylist->ActiveVariant->GetActiveVideoRendition() == nullptr)
        //schedule a switch on the video stream
        cpVideoStream->ScheduleSwitch(targetVariant->spPlaylist.get(), PlaylistSwitchRequest::SwitchType::BITRATE);
      //if there is a selected audio stream
      if (cpAudioStream != nullptr && cpAudioStream->Selected() && spRootPlaylist->ActiveVariant->GetActiveAudioRendition() == nullptr)
        //schedule a switch the audio stream
        cpAudioStream->ScheduleSwitch(targetVariant->spPlaylist.get(), PlaylistSwitchRequest::SwitchType::BITRATE);

      //ignore the change
      if (spHeuristicsManager->GetLastSuggestedBandwidth() != targetVariant->Bandwidth)  //reset the bandwidth
        //set the monitor last suggested bandwidth accordingly
        spHeuristicsManager->SetLastSuggestedBandwidth(targetVariant->Bandwidth);
    }
    else
    {
      //ignore the change
      if (spHeuristicsManager->GetLastSuggestedBandwidth() != curBitrate)  //reset the bandwidth
        //set the monitor last suggested bandwidth accordingly
        spHeuristicsManager->SetLastSuggestedBandwidth(curBitrate);

      curPlaylist->PauseBufferBuilding = false;

      if (cpController != nullptr && cpController->GetPlaylist() != nullptr && GetCurrentState() == MediaSourceState::MSS_STARTED)
      {

        auto currentBR = spRootPlaylist->ActiveVariant->Bandwidth;
        auto targetBR = targetVariant->Bandwidth;
        if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
          protectionRegistry.Register(task<HRESULT>([currentBR, targetBR, this]()
        {
          //raise the change cancelled event
          if (GetCurrentState() != MSS_STOPPED && GetCurrentState() != MSS_UNINITIALIZED && GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_PAUSED
            && cpController != nullptr && cpController->GetPlaylist() != nullptr)
            cpController->GetPlaylist()->RaiseBitrateSwitchCancelled(currentBR, targetBR);

          return S_OK;
        }));

      }
    }
  }

}

///<summary>Handles a change request from the player to an alternate audio or video rendition (subtitle is handled at the player)</summary>
///<param name='RenditionType'>AUDIO or VIDEO</param>
///<param name='targetRendition'>The target rendition instance</param>
HRESULT CHLSMediaSource::RenditionChangeRequest(std::wstring RenditionType, shared_ptr<Rendition> targetRendition, StreamInfo *ForVariant)
{
  if ((targetRendition->Default && targetRendition->PlaylistUri.empty()) && spRootPlaylist->ActiveVariant != nullptr &&
    spRootPlaylist->ActiveVariant->GetActiveAudioRendition() == nullptr)//we are trying to switch to default - but we are already playing default
    return S_OK;
  else if (spRootPlaylist->ActiveVariant->GetActiveAudioRendition() == targetRendition.get()) //again no need to switch
    return S_OK;
  LOG("Requesting Rendition Switch...");


  //only do if the current state is started
  if (GetCurrentState() != MediaSourceState::MSS_STARTED) return S_OK;
  //root playlist needs to be variant
  if (!spRootPlaylist->IsVariant)
    return E_FAIL;
  //valid rendition ? 
  if (targetRendition != nullptr && RenditionType != targetRendition->Type)
    return E_FAIL;

  //if rendition is not null
  if (targetRendition != nullptr)
  {
    //Group ID needs to match
    if ((RenditionType == Rendition::TYPEAUDIO && ForVariant->AudioRenditionGroupID != targetRendition->GroupID) ||
      (RenditionType == Rendition::TYPEVIDEO && ForVariant->VideoRenditionGroupID != targetRendition->GroupID))
      return E_FAIL;

    //dowmload the rendition playlist if needed
    if (targetRendition->spPlaylist == nullptr && targetRendition->PlaylistUri.empty() == false && FAILED(targetRendition->DownloadRenditionPlaylistAsync().get()))
      return E_FAIL;
  }

  LOG("Preparing Rendition Switch...")
    shared_ptr<Playlist> targetPlaylist = nullptr;
  //we prepare the active variant for the switch. If we get back a valida target playlist, then ...
  if ((targetPlaylist = ForVariant->PrepareRenditionSwitch(RenditionType, targetRendition == nullptr ? nullptr : targetRendition.get())) != nullptr)
  {
    //if this an alternate audio switch
    if (RenditionType == Rendition::TYPEAUDIO && cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      LOG("Scheduling Rendition Switch...")
        //schedule the switch
        cpAudioStream->ScheduleSwitch(targetPlaylist.get(), PlaylistSwitchRequest::SwitchType::RENDITION);
    }
    //else if switching to alternate video 
    else if (RenditionType == Rendition::TYPEVIDEO && cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //schedule switch
      cpVideoStream->ScheduleSwitch(targetPlaylist.get(), PlaylistSwitchRequest::SwitchType::RENDITION);
    }
  }

  return S_OK;

}

#pragma endregion


#pragma region ERROR NOTIFICATION
///<summary>Notify a fatal error</summary>
///<param name='hrErrorCode'>Error code</param>
HRESULT CHLSMediaSource::NotifyError(HRESULT hrErrorCode)
{
  HRESULT hr = S_OK;



  std::lock_guard<std::recursive_mutex> lock(LockSource);

  auto oldstate = GetCurrentState();

  if (oldstate == MSS_STARTING) //we got an error while trying to start - we will let events unfold and this taken care of once Start is called and playback attempt is made
    return S_OK;

  SetCurrentState(MediaSourceState::MSS_ERROR);
  if (oldstate != MediaSourceState::MSS_ERROR && oldstate != MediaSourceState::MSS_UNINITIALIZED)
  {
    if (oldstate == MediaSourceState::MSS_STARTED || oldstate == MediaSourceState::MSS_BUFFERING || oldstate == MSS_SEEKING)
    {


      LOG("NotifyError : Closing Streams...");
      if (cpVideoStream != nullptr && cpVideoStream->Selected())
        //cpVideoStream->NotifyError(hrErrorCode);
        cpVideoStream->NotifyStreamStopped();
      if (cpAudioStream != nullptr && cpAudioStream->Selected())
        cpAudioStream->NotifyStreamStopped();

    }


    if (spHeuristicsManager != nullptr)
      spHeuristicsManager->StopNotifier();


    if (spRootPlaylist != nullptr)
    {
      if (spRootPlaylist->IsLive && spRootPlaylist->ActiveVariant != nullptr && spRootPlaylist->ActiveVariant->spPlaylist != nullptr && spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh != nullptr)
      {
        spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh->StopTicking();
        spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh.reset();

      }

      //cancel all downloads     
      spRootPlaylist->CancelDownloads();

      if (oldstate == MediaSourceState::MSS_STARTED || oldstate == MediaSourceState::MSS_BUFFERING)
      {
        LOG("NotifyError : Closing Media Source...")
          PROPVARIANT pvar;

        ::PropVariantInit(&pvar);
        QueueEvent(MEError, GUID_NULL, E_FAIL, &pvar);
        ::PropVariantClear(&pvar);
      }
    }
  }
  return hr;
}



#pragma endregion


#pragma region PLAYBACK 


IFACEMETHODIMP CHLSMediaSource::Invoke(IMFAsyncResult *pResult)
{
  if (pResult == nullptr)
    return E_FAIL;
  ComPtr<AsyncCommand> cpCommand = reinterpret_cast<AsyncCommand *>(pResult->GetStateNoAddRef());
  if (cpCommand == nullptr)
    return E_POINTER;
  HRESULT hr = S_OK;
  if (cpCommand->type == CommandType::START)
  {
    hr = (spRootPlaylist->IsVariant && spRootPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr) ? StartAsyncWithAltAudio(cpCommand->spTimestamp) : StartAsync(cpCommand->spTimestamp);
    pResult->SetStatus(hr);
  }
  else if (cpCommand->type == CommandType::STOP)
  {
    hr = StopAsync();
    pResult->SetStatus(hr);
  }
  else if (cpCommand->type == CommandType::PAUSE)
  {
    hr = PauseAsync();
    pResult->SetStatus(hr);
  }
  /*else if (cpCommand->type == CommandType::SHUTDOWN)
  {
  hr = ShutdownAsync(cpCommand->oldState);
  pResult->SetStatus(hr);
  }*/


  return S_OK;
}

///<summary>Start (see IMFMediaSource on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::Start(IMFPresentationDescriptor *pPresentationDescriptor, const GUID *pGuidTimeFormat, const PROPVARIANT *pvarStartPosition)
{
  LOG("MediaSource Start()");
  if (nullptr == pPresentationDescriptor)
    return MF_E_INVALIDREQUEST;
  //we only measure time in ticks (100 ns)
  if (nullptr != pGuidTimeFormat && GUID_NULL != *pGuidTimeFormat)
    return MF_E_UNSUPPORTED_TIME_FORMAT;
  if (pvarStartPosition->vt != VT_I8 && pvarStartPosition->vt != VT_EMPTY)
    return MF_E_INVALIDREQUEST;
  if (GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED)
    return MF_E_SHUTDOWN;
  if (GetCurrentState() == MediaSourceState::MSS_ERROR)
    return MF_E_MEDIA_SOURCE_WRONGSTATE;

  HRESULT hr = S_OK;
  std::shared_ptr<Timestamp> startFrom = nullptr;
  //are we starting from somewhere other than the beginning ?
  if (pvarStartPosition->vt == VT_I8)
    startFrom = make_shared<Timestamp>(pvarStartPosition->hVal.QuadPart < 0 ? 0 : pvarStartPosition->hVal.QuadPart);




  this->AddRef();//lock from shutdown initiated release
  ComPtr<AsyncCommand> startCmd = WRL::Make<AsyncCommand>(CommandType::START, startFrom);
  return MFPutWorkItem2(SerialWorkQueueID, 0, this, startCmd.Detach());


}

HRESULT CHLSMediaSource::StartAsync(shared_ptr<Timestamp> startFrom)
{
  //Per current Media Foundation implementation, XAML MEdiaElement.Stop() is actually a composition of Pause() + Seek(0). <Video> tag does not have a Stop() equivalent.
  //When ME.Stop() is invoked, the pipeline calls in the following order:
  // - Source.Stop()
  // - Source.Start() with position = 0 (effectively Seek(0))
  // - Source.Pause()
  //So the only way Start() can be called on the source while the source is in a STOPPED state is 
  //as an interim step in the ME.Stop() workflow.
  HRESULT hr = S_OK;

  if (GetCurrentState() == MSS_ERROR || GetCurrentState() == MSS_UNINITIALIZED)
  {
    this->Release();
    return MF_E_MEDIA_SOURCE_WRONGSTATE;
  }

  task_completion_event<HRESULT> tceProtectPlaylist;
  //stop premature release of the root playlist

  BlockPrematureRelease(tceProtectPlaylist);

  LOG("MediaSource StartAsync()");
  try
  {


    if (spRootPlaylist->IsLive)
    {
      auto pl = spRootPlaylist->IsVariant ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      bool stale = ((swStopOrPause != nullptr ? (unsigned long long)swStopOrPause->Stop() : 0UL) > pl->DerivedTargetDuration);
      //we reset Live non event playlists and redownload to catch it at its current live position
      if (!HandleInitialPauseForAutoPlay && !LivePlaylistPositioned && stale)
      {
        LOG("Live Refresh Stale - Nulling segment trackers")
        pl->SetCurrentSegmentTracker(AUDIO, nullptr);
        pl->SetCurrentSegmentTracker(VIDEO, nullptr);
        pl->SlidingWindowEnd.reset();
        pl->SlidingWindowStart.reset();
      }

      LivePlaylistPositioned = false;

      if (GetCurrentState() == MSS_PAUSED && (stale || startFrom != nullptr))
      {
        if (spRootPlaylist->IsVariant)
        {
          auto si = spRootPlaylist->DownloadVariantStreamPlaylist(spRootPlaylist->ActiveVariant->Bandwidth);
          if (si == nullptr)
            throw hr;
          if (si->Bandwidth == spRootPlaylist->ActiveVariant->Bandwidth)//still go the same bitrate - no errors
          {
            pl->MergeLivePlaylistChild();
          }
          else
          {
            si->MakeActive();
            pl = si->spPlaylist;
            spHeuristicsManager->SetLastSuggestedBandwidth(si->Bandwidth);
          }
        }
        else
        {
          if (FAILED(Playlist::DownloadPlaylistAsync(this, Helpers::JoinUri(spRootPlaylist->BaseUri, spRootPlaylist->FileName), spRootPlaylist).get()))
            throw E_FAIL;

          spRootPlaylist->MergeLivePlaylistMaster();
        }
      }

      if (startFrom != nullptr)
      {
        if (pl->Segments.front()->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(pl.get(), pl->Segments.front()->GetSequenceNumber(), false, true, true).get();
          hr = std::get<0>(ret);
          if (FAILED(hr))
            throw E_FAIL;
        }

        pl->SlidingWindowStart = std::make_shared<Timestamp>(pl->Segments.front()->Timeline.front()->ValueInTicks);
        pl->SlidingWindowEnd = std::make_shared<Timestamp>(pl->SlidingWindowStart->ValueInTicks + pl->Segments.back()->CumulativeDuration);
      }

      //start notifier
      if (!spHeuristicsManager->IsBitrateChangeNotifierRunning())
        spHeuristicsManager->StartNotifier();
    }



    auto target = (spRootPlaylist->IsVariant) ? spRootPlaylist->ActiveVariant->spPlaylist.get() : spRootPlaylist.get();

    switch (GetCurrentState())
    {
    case MSS_STARTING:
      hr = StartPlaylistInitial(target, startFrom);
      break;
    case MSS_STOPPED:
      hr = spRootPlaylist->IsLive ? StartLivePlaylistFromStoppedState(target, startFrom) : StartVODPlaylistFromStoppedState(target, startFrom);
      break;
    case MSS_PAUSED:
      hr = spRootPlaylist->IsLive ? StartLivePlaylistFromPausedState(target, startFrom) : StartVODPlaylistFromPausedState(target, startFrom);
      break;
    default:
      break;
    }

    SetCurrentState(MSS_STARTED);


    if (SUCCEEDED(hr))
    {
      if (spRootPlaylist->IsLive && HandleInitialPauseForAutoPlay == false)
      {
        if (spRootPlaylist->IsVariant && (spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh == nullptr || spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh->IsTicking == false)) //this is the active playlist
        {
          spRootPlaylist->ActiveVariant->spPlaylist->SetStopwatchForNextPlaylistRefresh(spRootPlaylist->ActiveVariant->spPlaylist->DerivedTargetDuration / 2);
        }
        else if (spRootPlaylist->spswPlaylistRefresh == nullptr || spRootPlaylist->spswPlaylistRefresh->IsTicking == false)
        {
          spRootPlaylist->SetStopwatchForNextPlaylistRefresh(spRootPlaylist->DerivedTargetDuration / 2);
        }
      }

      auto targetPlaylist = spRootPlaylist->IsVariant ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      if (targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) != nullptr &&
        (LastPlayedVideoSegment == nullptr ||
        LastPlayedVideoSegment->GetSequenceNumber() != targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO)->GetSequenceNumber()))
      {
        auto pPlaylist = targetPlaylist.get();
        auto segseq = targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO)->SequenceNumber;
        //raise the segment changed event 
        if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
          protectionRegistry.Register(task<HRESULT>([pPlaylist, segseq, this]()
        {
          if (GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_UNINITIALIZED && cpController != nullptr && cpController->GetPlaylist() != nullptr)
            cpController->GetPlaylist()->RaiseSegmentSwitched(nullptr, pPlaylist, 0, segseq);
          return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
      }
      else if (targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) != nullptr &&
        (LastPlayedAudioSegment == nullptr ||
        LastPlayedAudioSegment->GetSequenceNumber() != targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO)->GetSequenceNumber()))
      {
        auto pPlaylist = targetPlaylist.get();
        auto segseq = targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO)->SequenceNumber;
        //raise the segment changed event 
        if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
          protectionRegistry.Register(task<HRESULT>([pPlaylist, segseq, this]()
        {
          if (GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_UNINITIALIZED && cpController != nullptr && cpController->GetPlaylist() != nullptr)
            cpController->GetPlaylist()->RaiseSegmentSwitched(nullptr, pPlaylist, 0, segseq);

          return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
      }
    }

  }
  catch (...)
  {

    SetCurrentState(MediaSourceState::MSS_STOPPED);
    hr = E_FAIL;
  }

  this->Release();

  if (FAILED(hr))
  {
    if (spHeuristicsManager->IsBitrateChangeNotifierRunning())
      spHeuristicsManager->StopNotifier();

    UnblockReleaseBlock(tceProtectPlaylist);
    return MF_E_INVALIDREQUEST;
  }
  else
  {
    //start notifier
    if (!spHeuristicsManager->IsBitrateChangeNotifierRunning())
      spHeuristicsManager->StartNotifier();
    UnblockReleaseBlock(tceProtectPlaylist);
    return  S_OK;
  }


}

HRESULT CHLSMediaSource::StartAsyncWithAltAudio(shared_ptr<Timestamp> startFrom)
{
  //Per current Media Foundation implementation, XAML MEdiaElement.Stop() is actually a composition of Pause() + Seek(0). <Video> tag does not have a Stop() equivalent.
  //When ME.Stop() is invoked, the pipeline calls in the following order:
  // - Source.Stop()
  // - Source.Start() with position = 0 (effectively Seek(0))
  // - Source.Pause()
  //So the only way Start() can be called on the source while the source is in a STOPPED state is 
  //as an interim step in the ME.Stop() workflow.
  HRESULT hr = S_OK;

  if (GetCurrentState() == MSS_ERROR || GetCurrentState() == MSS_UNINITIALIZED)
  {
    this->Release();
    return MF_E_MEDIA_SOURCE_WRONGSTATE;
  }

  task_completion_event<HRESULT> tceProtectPlaylist;
  //stop premature release of the root playlist

  BlockPrematureRelease(tceProtectPlaylist);

  LOG("MediaSource StartAsync()");
  try
  {


    if (spRootPlaylist->IsLive)
    {
      auto pl = spRootPlaylist->IsVariant ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      bool stale = ((swStopOrPause != nullptr ? (unsigned long long)swStopOrPause->Stop() : 0UL) > pl->DerivedTargetDuration);
      //we reset Live non event playlists and redownload to catch it at its current live position 


      //we reset Live non event playlists and redownload to catch it at its current live position
      if (!HandleInitialPauseForAutoPlay && !LivePlaylistPositioned && stale)
      {

        LOG("Merge Stale - Nulling segment trackers")
          pl->SetCurrentSegmentTracker(AUDIO, nullptr);
        pl->SetCurrentSegmentTracker(VIDEO, nullptr);
        auto altaudioren = pl->pParentStream->GetActiveAudioRendition();
        if (altaudioren != nullptr && altaudioren->spPlaylist != nullptr)
        {
          altaudioren->spPlaylist->SetCurrentSegmentTracker(AUDIO, nullptr);
        }
        pl->SlidingWindowEnd.reset();
        pl->SlidingWindowStart.reset();
      }

      LivePlaylistPositioned = false;

      if (GetCurrentState() == MSS_PAUSED && (stale || startFrom != nullptr))
      {
        if (spRootPlaylist->IsVariant)
        {
          auto si = spRootPlaylist->DownloadVariantStreamPlaylist(spRootPlaylist->ActiveVariant->Bandwidth);
          if (si == nullptr)
            throw hr;
          if (si->Bandwidth == spRootPlaylist->ActiveVariant->Bandwidth)//still go the same bitrate - no errors
          {
            pl->MergeLivePlaylistChild();
          }
          else
          {
            si->MakeActive();
            pl = si->spPlaylist;
            spHeuristicsManager->SetLastSuggestedBandwidth(si->Bandwidth);
          }
        }
        else
        {
          if (FAILED(Playlist::DownloadPlaylistAsync(this, Helpers::JoinUri(spRootPlaylist->BaseUri, spRootPlaylist->FileName), spRootPlaylist).get()))
            throw E_FAIL;

          spRootPlaylist->MergeLivePlaylistMaster();
        }
      }

      if (startFrom != nullptr)
      {
        if (pl->Segments.front()->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(pl.get(), pl->Segments.front()->GetSequenceNumber(), false, true, true).get();
          hr = std::get<0>(ret);
          if (FAILED(hr))
            throw E_FAIL;
        }

        pl->SlidingWindowStart = std::make_shared<Timestamp>(pl->Segments.front()->Timeline.front()->ValueInTicks);
        pl->SlidingWindowEnd = std::make_shared<Timestamp>(pl->SlidingWindowStart->ValueInTicks + pl->Segments.back()->CumulativeDuration);
      }

      //start notifier
      if (!spHeuristicsManager->IsBitrateChangeNotifierRunning())
        spHeuristicsManager->StartNotifier();
    }



    auto target = (spRootPlaylist->IsVariant) ? spRootPlaylist->ActiveVariant->spPlaylist.get() : spRootPlaylist.get();

    switch (GetCurrentState())
    {
    case MSS_STARTING:
      hr = StartPlaylistInitial(target, startFrom);
      break;
    case MSS_STOPPED:
      hr = spRootPlaylist->IsLive ? StartLivePlaylistWithAltAudioFromStoppedState(target, startFrom) : StartVODPlaylistWithAltAudioFromStoppedState(target, startFrom);
      break;
    case MSS_PAUSED:
      hr = spRootPlaylist->IsLive ? StartLivePlaylistWithAltAudioFromPausedState(target, startFrom) : StartVODPlaylistWithAltAudioFromPausedState(target, startFrom);
      break;
    default:
      break;
    }

    SetCurrentState(MSS_STARTED);



    if (SUCCEEDED(hr))
    {
      if (spRootPlaylist->IsLive && HandleInitialPauseForAutoPlay == false)
      {
        if (spRootPlaylist->IsVariant && (spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh == nullptr || spRootPlaylist->ActiveVariant->spPlaylist->spswPlaylistRefresh->IsTicking == false)) //this is the active playlist
        {
          spRootPlaylist->ActiveVariant->spPlaylist->SetStopwatchForNextPlaylistRefresh(spRootPlaylist->ActiveVariant->spPlaylist->DerivedTargetDuration / 2);
        }
        else if (spRootPlaylist->spswPlaylistRefresh == nullptr || spRootPlaylist->spswPlaylistRefresh->IsTicking == false)
        {
          spRootPlaylist->SetStopwatchForNextPlaylistRefresh(spRootPlaylist->DerivedTargetDuration / 2);
        }
      }

      auto targetPlaylist = spRootPlaylist->IsVariant ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      if (targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) != nullptr &&
        (LastPlayedVideoSegment == nullptr ||
        LastPlayedVideoSegment->GetSequenceNumber() != targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO)->GetSequenceNumber()))
      {
        auto pPlaylist = targetPlaylist.get();
        auto segseq = targetPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO)->SequenceNumber;
        //raise the segment changed event 
        if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
          protectionRegistry.Register(task<HRESULT>([pPlaylist, segseq, this]()
        {
          if (GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_UNINITIALIZED && cpController != nullptr && cpController->GetPlaylist() != nullptr)
            cpController->GetPlaylist()->RaiseSegmentSwitched(nullptr, pPlaylist, 0, segseq);
          return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
      }
      else if (targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) != nullptr &&
        (LastPlayedAudioSegment == nullptr ||
        LastPlayedAudioSegment->GetSequenceNumber() != targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO)->GetSequenceNumber()))
      {
        auto pPlaylist = targetPlaylist.get();
        auto segseq = targetPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO)->SequenceNumber;
        //raise the segment changed event 
        if (cpController != nullptr && cpController->GetPlaylist() != nullptr)
          protectionRegistry.Register(task<HRESULT>([pPlaylist, segseq, this]()
        {
          if (GetCurrentState() != MSS_ERROR && GetCurrentState() != MSS_UNINITIALIZED && cpController != nullptr && cpController->GetPlaylist() != nullptr)
            cpController->GetPlaylist()->RaiseSegmentSwitched(nullptr, pPlaylist, 0, segseq);

          return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
      }
    }

  }
  catch (...)
  {

    SetCurrentState(MediaSourceState::MSS_STOPPED);
    hr = E_FAIL;
  }

  this->Release();

  if (FAILED(hr))
  {
    if (spHeuristicsManager->IsBitrateChangeNotifierRunning())
      spHeuristicsManager->StopNotifier();

    UnblockReleaseBlock(tceProtectPlaylist);
    return MF_E_INVALIDREQUEST;
  }
  else
  {
    //start notifier
    if (!spHeuristicsManager->IsBitrateChangeNotifierRunning())
      spHeuristicsManager->StartNotifier();
    UnblockReleaseBlock(tceProtectPlaylist);
    return  S_OK;
  }


}

HRESULT CHLSMediaSource::StartPlaylistInitial(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{
  auto livestartseqnum = pPlaylist->FindLiveStartSegmentSequenceNumber();
  auto StartSeg = pPlaylist->IsLive ? pPlaylist->GetSegment(livestartseqnum) : pPlaylist->Segments.front();

  auto prefetchcount = pPlaylist->NeedsPreFetch();
  if (prefetchcount == 0)
  {
    auto result = Playlist::StartStreamingAsync(pPlaylist, StartSeg->SequenceNumber).get();//wait() or get() ?
    if (FAILED(std::get<0>(result)))
      return std::get<0>(result);
  }
  else
  {
    if (FAILED(pPlaylist->BulkFetch(StartSeg, prefetchcount)))
      return E_FAIL;
  }

  if (StartSeg->GetCurrentState() != INMEMORYCACHE)
  {
    auto result = Playlist::StartStreamingAsync(pPlaylist, StartSeg->SequenceNumber, false).get();
    if (FAILED(std::get<0>(result)))
      return std::get<0>(result);
  }

  //notify that the source has started
  if (spRootPlaylist->IsLive)
  {
    if (StartSeg->HasMediaType(VIDEO) == false || cpVideoStream->Selected() == false)
      NotifyStarted(StartSeg->PeekNextSample(StartSeg->GetPIDForMediaType(AUDIO), MFRATE_DIRECTION::MFRATE_FORWARD)->SamplePTS);
    else
      NotifyStarted(StartSeg->PeekNextSample(StartSeg->GetPIDForMediaType(VIDEO), MFRATE_DIRECTION::MFRATE_FORWARD)->SamplePTS);
  }
  else
  {
    NotifyStarted();
  }

  //if video stream exists and has been selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    //notify that there is a new video stream
    NotifyNewStream(cpVideoStream.Get());
    if (spRootPlaylist->IsLive && StartSeg->HasMediaType(VIDEO))
    {
      auto vidpid = StartSeg->GetPIDForMediaType(VIDEO);
      auto firstsample = StartSeg->PeekNextSample(vidpid, MFRATE_FORWARD);
      cpVideoStream->NotifyStreamStarted(StartSeg->Discontinous && firstsample->DiscontinousTS != nullptr ? firstsample->DiscontinousTS : firstsample->SamplePTS);
    }
    else
    {
      //notify that the stream has started
      cpVideoStream->NotifyStreamStarted(posTS);
    }

  }
  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //notify new audio stream
    NotifyNewStream(cpAudioStream.Get());
    if (spRootPlaylist->IsLive && StartSeg->HasMediaType(AUDIO))
    {
      auto audpid = StartSeg->GetPIDForMediaType(AUDIO);
      auto firstsample = StartSeg->PeekNextSample(audpid, MFRATE_FORWARD);
      cpAudioStream->NotifyStreamStarted(StartSeg->Discontinous && firstsample->DiscontinousTS != nullptr ? firstsample->DiscontinousTS : firstsample->SamplePTS);
    }
    else
    {
      //notify that the stream has started
      cpAudioStream->NotifyStreamStarted(posTS);
    }
  }

  if (StartSeg->GetCurrentState() == INMEMORYCACHE)
  {
    if (StartSeg->HasMediaType(ContentType::VIDEO) == false || Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO)
      pPlaylist->RaiseStreamSelectionChanged(TrackType::BOTH, TrackType::AUDIO);
    else if (StartSeg->HasMediaType(ContentType::AUDIO) == false || Configuration::GetCurrent()->ContentTypeFilter == ContentType::VIDEO)
      pPlaylist->RaiseStreamSelectionChanged(TrackType::BOTH, TrackType::VIDEO);
  }
  //change state flag

  return S_OK;
}

HRESULT CHLSMediaSource::StartVODPlaylistFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{
  Playlist *ptrpl = pPlaylist;


  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    auto curvidseg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);
    //we were cloaking and not scrubbing i.e. we were either going FF or REW and thinned
    if (curvidseg != nullptr && curvidseg->HasMediaType(ContentType::VIDEO) && GetCurrentPlaybackRate()->Rate != 0.0 && curvidseg->GetCloaking() != nullptr && pPlaylist->pParentStream != nullptr)
    {

      //we do this so that a normal bitrate switch is used later to switch back to the non-cloaked bitrate
      pPlaylist->pParentStream->MakeActive(false);
      ptrpl = curvidseg->GetCloaking()->pParentPlaylist;

      auto si = spRootPlaylist->ActivateStream(ptrpl->pParentStream->Bandwidth);
      if (si == nullptr)
        return E_FAIL;
      ptrpl = si->spPlaylist.get();
      spHeuristicsManager->SetLastSuggestedBandwidth(ptrpl->pParentStream->Bandwidth);

      if (ptrpl->StartPTSOriginal == nullptr && ptrpl->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr)
      {
        if (curvidseg->SequenceNumber != ptrpl->Segments[0]->SequenceNumber)
          ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
      }

    }

    cpVideoStream->SetPlaylist(ptrpl);
  }

  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //set audio stream to play main audio
    cpAudioStream->SetPlaylist(ptrpl);
  }



  if (posTS == nullptr || posTS->ValueInTicks == 0)
  {
    //rewind playlist
    spRootPlaylist->Rewind();
    //initial wait for data 
    if (FAILED(std::get<0>(Playlist::StartStreamingAsync(ptrpl, ptrpl->Segments.front()->SequenceNumber, false, true, true).get())))
      return E_FAIL;//wait() or get() ?
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);
  }
  else //position specified
  {

    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionVOD(ptrpl, posTS->ValueInTicks);
    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    HRESULT hr = std::get<0>(ret);

    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }

    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      //notify audio stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      else //start with audio position for audio only bitrate
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));

    }
  }
  return S_OK;
}

HRESULT CHLSMediaSource::StartLivePlaylistFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{
  Playlist *ptrpl = pPlaylist;

  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    //play main video track
    cpVideoStream->SetPlaylist(ptrpl);
  }

  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {

    //set audio stream to play main audio
    cpAudioStream->SetPlaylist(ptrpl);
  }


  if (posTS == nullptr || posTS->ValueInTicks == 0)
  {
    //rewind playlist
    spRootPlaylist->Rewind();
    //initial wait for data 
    if (FAILED(std::get<0>(Playlist::StartStreamingAsync(ptrpl, ptrpl->Segments.front()->SequenceNumber, false, true, true).get())))
      return E_FAIL;//wait() or get() ?
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
    {
      cpVideoStream->NotifyStreamStarted(posTS);
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      cpAudioStream->NotifyStreamStarted(posTS);
    }
  }
  else //position specified
  {

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      cpVideoStream->NotifyStreamUpdated();
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      cpAudioStream->NotifyStreamUpdated();
    }


    unsigned long long max = posTS->ValueInTicks;
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionLive(ptrpl, posTS->ValueInTicks);
    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    HRESULT hr = std::get<0>(ret);
    if (FAILED(hr))
      return E_FAIL;



    //find the maximum position across all streams

    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }

    //notify source started using the max position value
    NotifySeeked(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      //notify audio stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      else //start with audio position for audio only bitrate
        cpVideoStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
      {
        cpAudioStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
      }

    }


    //we came here because a live DVR seek is in progress
    LivePlaylistPositioned = true;

  }

  return S_OK;
}

HRESULT CHLSMediaSource::StartVODPlaylistFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{


  Playlist *ptrpl = pPlaylist;
  shared_ptr<Timestamp> posVideo = nullptr;
  shared_ptr<Timestamp> posAudio = nullptr;
  //if video stream exists and is selected
  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    auto curvidseg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);
    //we were cloaking and not scrubbing i.e. we were either going FF or REW and thinned
    if (curvidseg != nullptr && curvidseg->HasMediaType(ContentType::VIDEO) && GetCurrentPlaybackRate()->Rate != 0.0 && curvidseg->GetCloaking() != nullptr  && prevPlaybackRate != nullptr && prevPlaybackRate->Rate == 0.0 && pPlaylist->pParentStream != nullptr)
    {

      //we do this so that a normal bitrate switch is used later to switch back to the non-cloaked bitrate
      pPlaylist->pParentStream->MakeActive(false);
      ptrpl = curvidseg->GetCloaking()->pParentPlaylist;

      auto si = spRootPlaylist->ActivateStream(ptrpl->pParentStream->Bandwidth);
      if (si == nullptr)
        return E_FAIL;

      ptrpl = si->spPlaylist.get();

      spHeuristicsManager->SetLastSuggestedBandwidth(ptrpl->pParentStream->Bandwidth);

      if (posTS == nullptr) //we need to find out where to start - this only applies if we were scrubbing (and scrubbing only works if there is video)
      {
        auto nextsd = curvidseg->PeekNextSample(curvidseg->GetPIDForMediaType(ContentType::VIDEO), curDirection);
        posTS = (nextsd != nullptr ? nextsd->SamplePTS : make_shared<Timestamp>(curvidseg->CumulativeDuration));
      }

    }

    //play main video track
    cpVideoStream->SetPlaylist(ptrpl);
  }
  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {

    //set audio stream to play main audio
    cpAudioStream->SetPlaylist(ptrpl);
  }

  if (ptrpl->StartPTSOriginal == nullptr)
  {
    ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
  }

  //if supplied position is null - we start from where we are currently paused at
  if (posTS == nullptr)
  {

    auto seg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr ?
      pPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) : pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);

    if (seg != nullptr)
      Playlist::StartStreamingAsync(ptrpl, seg->GetSequenceNumber());
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started
    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);

  }
  else //position specified
  {
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionVOD(ptrpl, posTS->ValueInTicks);
    HRESULT hr = std::get<0>(ret);
    //if position setting failed
    if (FAILED(hr))
    {
      //notify and return
      NotifyStarted(posTS, hr);
      return S_OK;
    }

    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }
    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      //notify audio stream started
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }
  }

  return S_OK;
}

HRESULT CHLSMediaSource::StartLivePlaylistFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{

  Playlist *ptrpl = pPlaylist;
  shared_ptr<Timestamp> posVideo = nullptr;
  shared_ptr<Timestamp> posAudio = nullptr;
  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    //play main video track
    cpVideoStream->SetPlaylist(ptrpl);
  }
  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {

    //set audio stream to play main audio
    cpAudioStream->SetPlaylist(ptrpl);
  }

  if (ptrpl->StartPTSOriginal == nullptr)
  {
    ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
  }

  if (posTS == nullptr)
  {
    shared_ptr<MediaSegment> seg = nullptr;


    seg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr ?
      pPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) : pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);

    if (seg == nullptr){
      if (Configuration::GetCurrent()->ResumeLiveFromPausedOrEarliest)
      {
        if (cpVideoStream->Selected() && cpAudioStream->Selected() && LastPlayedVideoSegment != nullptr && LastPlayedAudioSegment != nullptr)
        {

          if (pPlaylist->Segments.front()->SequenceNumber > __min(LastPlayedVideoSegment->SequenceNumber, LastPlayedAudioSegment->SequenceNumber)) //we no longer have those segments
          {
            seg = pPlaylist->GetSegment(pPlaylist->Segments.front()->SequenceNumber);
            pPlaylist->SetCurrentSegmentTracker(VIDEO, seg);
            pPlaylist->SetCurrentSegmentTracker(AUDIO, seg);
          }
          else
          {
            pPlaylist->SetCurrentSegmentTracker(VIDEO, LastPlayedVideoSegment);
            pPlaylist->SetCurrentSegmentTracker(AUDIO, LastPlayedAudioSegment);
            seg = LastPlayedVideoSegment->SequenceNumber > LastPlayedAudioSegment->SequenceNumber ? LastPlayedVideoSegment : LastPlayedAudioSegment;

          }

        }
        else if (cpAudioStream->Selected() && LastPlayedAudioSegment != nullptr)
        {
          if (pPlaylist->Segments.front()->SequenceNumber > LastPlayedAudioSegment->SequenceNumber) //we no longer have those segments
          {
            seg = pPlaylist->GetSegment(pPlaylist->Segments.front()->SequenceNumber);
            pPlaylist->SetCurrentSegmentTracker(AUDIO, seg);
          }
          else
          {
            pPlaylist->SetCurrentSegmentTracker(AUDIO, LastPlayedAudioSegment);
            seg = LastPlayedAudioSegment;
          }
        }

      }
      else
      {
        seg = pPlaylist->GetSegment(pPlaylist->FindLiveStartSegmentSequenceNumber());
        pPlaylist->SetCurrentSegmentTracker(VIDEO, seg);
        pPlaylist->SetCurrentSegmentTracker(AUDIO, seg);
      }

    }

    if (seg == nullptr)
      return E_FAIL;

    if (seg->GetCurrentState() != INMEMORYCACHE)
    {
      auto result = Playlist::StartStreamingAsync(pPlaylist, seg->SequenceNumber).get();//wait() or get() ?
      if (FAILED(std::get<0>(result)))
        return E_FAIL;
    }



    if (this->HandleInitialPauseForAutoPlay)
    {
      this->HandleInitialPauseForAutoPlay = false;
    }
    //notify source started
    NotifyStarted(posTS, S_OK);
    ////notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);


  }
  else //position specified
  {
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionLive(ptrpl, posTS->ValueInTicks);
    HRESULT hr = std::get<0>(ret);

    //if position setting failed
    if (FAILED(hr))
    {
      //notify and return
      NotifyStarted(posTS, hr);
      return S_OK;
    }

    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }
    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
      {
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      }
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      //notify audio stream started
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
      {
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
      }
    }

  }
  return S_OK;
}


HRESULT CHLSMediaSource::StartVODPlaylistWithAltAudioFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{
  Playlist *ptrpl = pPlaylist;


  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    auto curvidseg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);
    //we were cloaking and not scrubbing i.e. we were either going FF or REW and thinned
    if (curvidseg != nullptr && curvidseg->HasMediaType(ContentType::VIDEO) && GetCurrentPlaybackRate()->Rate != 0.0 && curvidseg->GetCloaking() != nullptr && pPlaylist->pParentStream != nullptr)
    {

      //we do this so that a normal bitrate switch is used later to switch back to the non-cloaked bitrate
      pPlaylist->pParentStream->MakeActive(false);
      ptrpl = curvidseg->GetCloaking()->pParentPlaylist;

      auto si = spRootPlaylist->ActivateStream(ptrpl->pParentStream->Bandwidth);
      if (si == nullptr)
        return E_FAIL;
      ptrpl = si->spPlaylist.get();
      spHeuristicsManager->SetLastSuggestedBandwidth(ptrpl->pParentStream->Bandwidth);

      if (ptrpl->StartPTSOriginal == nullptr && ptrpl->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr)
      {
        if (curvidseg->SequenceNumber != ptrpl->Segments[0]->SequenceNumber)
          ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
      }

    }
    //if we have an alternate video rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveVideoRendition() != nullptr)
    {
      //download alternate video playlist if needed
      if (ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveVideoRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;//wait() or get() ?
      //set video stream to play with alternate video
      cpVideoStream->SetPlaylist(ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist.get());
    }
    else
    {
      //play main video track
      cpVideoStream->SetPlaylist(ptrpl);
    }
  }

  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //if we have an alternate audio rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr)
    {
      //download alternate audio playlist if needed
      if (ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveAudioRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;
      //set audio stream to play alternate audio
      cpAudioStream->SetPlaylist(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get());
    }
    else
      //set audio stream to play main audio
      cpAudioStream->SetPlaylist(ptrpl);
  }



  if (posTS == nullptr || posTS->ValueInTicks == 0)
  {
    //rewind playlist
    spRootPlaylist->Rewind();
    //initial wait for data 
    if (FAILED(std::get<0>(Playlist::StartStreamingAsync(ptrpl, ptrpl->Segments.front()->SequenceNumber, false, true, true).get())))
      return E_FAIL;//wait() or get() ?
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);
  }
  else //position specified
  {

    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionVOD(ptrpl, posTS->ValueInTicks);
    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    HRESULT hr = std::get<0>(ret);

    //set alternate rendition playlist positions as needed
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr && SUCCEEDED(hr))
    {
      auto altaudioposret = Playlist::SetCurrentPositionVOD(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), posTS->ValueInTicks);
      hr = std::get<0>(altaudioposret);
      auto altaudiopos = std::get<1>(altaudioposret);
      if (SUCCEEDED(hr) && altaudiopos.find(ContentType::AUDIO) != altaudiopos.end())
      {
        if (actualPos.find(ContentType::AUDIO) != actualPos.end())
          actualPos[ContentType::AUDIO] = altaudiopos[ContentType::AUDIO];
        else
          actualPos.emplace(pair<ContentType, unsigned long long>(ContentType::AUDIO, altaudiopos[ContentType::AUDIO]));
      }
    }
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveVideoRendition() != nullptr && SUCCEEDED(hr))
    {
      auto altvideoposret = Playlist::SetCurrentPositionVOD(ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist.get(), posTS->ValueInTicks);
      hr = std::get<0>(altvideoposret);
      auto altvideopos = std::get<1>(altvideoposret);
      if (SUCCEEDED(hr) && altvideopos.find(ContentType::VIDEO) != altvideopos.end())
      {
        if (actualPos.find(ContentType::VIDEO) != actualPos.end())
          actualPos[ContentType::VIDEO] = altvideopos[ContentType::VIDEO];
        else
          actualPos.emplace(pair<ContentType, unsigned long long>(ContentType::VIDEO, altvideopos[ContentType::VIDEO]));
      }
    }



    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }



    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      //notify audio stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      else //start with audio position for audio only bitrate
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));

    }
  }



  return S_OK;
}

HRESULT CHLSMediaSource::StartLivePlaylistWithAltAudioFromStoppedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{
  Playlist *ptrpl = pPlaylist;

  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    auto curvidseg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);

    //if we have an alternate video rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveVideoRendition() != nullptr)
    {
      //download alternate video playlist if needed
      if (ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveVideoRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;//wait() or get() ?
      //set video stream to play with alternate video
      cpVideoStream->SetPlaylist(ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist.get());
    }
    else
    {
      //play main video track
      cpVideoStream->SetPlaylist(ptrpl);
    }
  }

  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //if we have an alternate audio rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr)
    {
      //download alternate audio playlist if needed
      if (ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveAudioRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;
      //set audio stream to play alternate audio
      cpAudioStream->SetPlaylist(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get());
    }
    else
      //set audio stream to play main audio
      cpAudioStream->SetPlaylist(ptrpl);
  }


  if (posTS == nullptr || posTS->ValueInTicks == 0)
  {
    //rewind playlist
    spRootPlaylist->Rewind();
    //initial wait for data 
    if (FAILED(std::get<0>(Playlist::StartStreamingAsync(ptrpl, ptrpl->Segments.front()->SequenceNumber, false, true, true).get())))
      return E_FAIL;//wait() or get() ?
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
    {
      cpVideoStream->NotifyStreamStarted(posTS);
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      cpAudioStream->NotifyStreamStarted(posTS);
    }
  }
  else //position specified
  {

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      cpVideoStream->NotifyStreamUpdated();
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      cpAudioStream->NotifyStreamUpdated();
    }


    unsigned long long max = posTS->ValueInTicks;
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionLive(ptrpl, posTS->ValueInTicks);
    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    HRESULT hr = std::get<0>(ret);
    if (FAILED(hr))
      return E_FAIL;
    //set alternate rendition playlist positions as needed
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr && SUCCEEDED(hr))
    {
      auto altaudioposret = Playlist::SetCurrentPositionLiveAlternateAudio(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), posTS->ValueInTicks, ptrpl->GetCurrentSegmentTracker(VIDEO)->SequenceNumber);
      hr = std::get<0>(altaudioposret);
      auto altaudiopos = std::get<1>(altaudioposret);
      if (SUCCEEDED(hr) && altaudiopos.find(ContentType::AUDIO) != altaudiopos.end())
      {
        if (actualPos.find(ContentType::AUDIO) != actualPos.end())
          actualPos[ContentType::AUDIO] = altaudiopos[ContentType::AUDIO];
        else
          actualPos.emplace(pair<ContentType, unsigned long long>(ContentType::AUDIO, altaudiopos[ContentType::AUDIO]));
      }
      if (FAILED(hr))
        return E_FAIL;
    }


    //find the maximum position across all streams

    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }

    //notify source started using the max position value
    NotifySeeked(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      //notify audio stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      else //start with audio position for audio only bitrate
        cpVideoStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
      {
        cpAudioStream->NotifyStreamSeeked(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
      }

    }


    //we came here because a live DVR seek is in progress
    LivePlaylistPositioned = true;

  }

  return S_OK;
}

HRESULT CHLSMediaSource::StartVODPlaylistWithAltAudioFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{


  Playlist *ptrpl = pPlaylist;
  shared_ptr<Timestamp> posVideo = nullptr;
  shared_ptr<Timestamp> posAudio = nullptr;
  //if video stream exists and is selected
  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    auto curvidseg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);
    //we were cloaking and not scrubbing i.e. we were either going FF or REW and thinned
    if (curvidseg != nullptr && curvidseg->HasMediaType(ContentType::VIDEO) && GetCurrentPlaybackRate()->Rate != 0.0 && curvidseg->GetCloaking() != nullptr  && prevPlaybackRate != nullptr && prevPlaybackRate->Rate == 0.0 && pPlaylist->pParentStream != nullptr)
    {

      //we do this so that a normal bitrate switch is used later to switch back to the non-cloaked bitrate
      pPlaylist->pParentStream->MakeActive(false);
      ptrpl = curvidseg->GetCloaking()->pParentPlaylist;

      auto si = spRootPlaylist->ActivateStream(ptrpl->pParentStream->Bandwidth);
      if (si == nullptr)
        return E_FAIL;

      ptrpl = si->spPlaylist.get();

      spHeuristicsManager->SetLastSuggestedBandwidth(ptrpl->pParentStream->Bandwidth);

      if (posTS == nullptr) //we need to find out where to start - this only applies if we were scrubbing (and scrubbing only works if there is video)
      {
        auto nextsd = curvidseg->PeekNextSample(curvidseg->GetPIDForMediaType(ContentType::VIDEO), curDirection);
        posTS = (nextsd != nullptr ? nextsd->SamplePTS : make_shared<Timestamp>(curvidseg->CumulativeDuration));
      }

    }
    //if we have an alternate video rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveVideoRendition() != nullptr)
    {
      //download alternate video playlist if needed
      if (ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveVideoRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;//wait() or get() ?
      //set video stream to play with alternate video
      cpVideoStream->SetPlaylist(ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist.get());
    }
    else
    {
      //play main video track
      cpVideoStream->SetPlaylist(ptrpl);
    }
  }
  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //if we have an alternate audio rendition set to play
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr)
    {
      //download alternate audio playlist if needed
      if (ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist == nullptr &&
        FAILED(ptrpl->pParentStream->GetActiveAudioRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;
      //set audio stream to play alternate audio
      cpAudioStream->SetPlaylist(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get());
    }
    else
      //set audio stream to play main audio
      cpAudioStream->SetPlaylist(ptrpl);
  }

  if (ptrpl->StartPTSOriginal == nullptr)
  {
    ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
  }

  //if supplied position is null - we start from where we are currently paused at
  if (posTS == nullptr)
  {

    auto seg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr ?
      pPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) : pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);

    if (seg != nullptr)
      Playlist::StartStreamingAsync(ptrpl, seg->GetSequenceNumber());
    //notify source started
    NotifyStarted(posTS, S_OK);
    //notify streams started
    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);

  }
  else //position specified
  {
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionVOD(ptrpl, posTS->ValueInTicks);
    HRESULT hr = std::get<0>(ret);
    //set alternate rendition playlist positions as needed
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr && SUCCEEDED(hr))
    {
      hr = std::get<0>(Playlist::SetCurrentPositionVOD(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), posTS->ValueInTicks));
    }
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveVideoRendition() != nullptr && SUCCEEDED(hr))
    {
      hr = std::get<0>(Playlist::SetCurrentPositionVOD(ptrpl->pParentStream->GetActiveVideoRendition()->spPlaylist.get(), posTS->ValueInTicks));
    }

    //if position setting failed
    if (FAILED(hr))
    {
      //notify and return
      NotifyStarted(posTS, hr);
      return S_OK;
    }

    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }
    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
    }
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      //notify audio stream started
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
    }
  }

  return S_OK;
}

HRESULT CHLSMediaSource::StartLivePlaylistWithAltAudioFromPausedState(Playlist *pPlaylist, std::shared_ptr<Timestamp> posTS)
{

  Playlist *ptrpl = pPlaylist;
  Playlist *altpl = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist.get();
  shared_ptr<Timestamp> posVideo = nullptr;
  shared_ptr<Timestamp> posAudio = nullptr;
  //if video stream exists and is selected
  if (cpVideoStream != nullptr && cpVideoStream->Selected())
  {
    //play main video track
    cpVideoStream->SetPlaylist(ptrpl);
  }
  //if audio stream exists and is selected
  if (cpAudioStream != nullptr && cpAudioStream->Selected())
  {
    //download alternate audio playlist if needed
    if (ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist == nullptr &&
      FAILED(ptrpl->pParentStream->GetActiveAudioRendition()->DownloadRenditionPlaylistAsync().get())) return E_FAIL;
    //set audio stream to play alternate audio
    cpAudioStream->SetPlaylist(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get());

  }

  if (ptrpl->StartPTSOriginal == nullptr)
  {
    ptrpl->StartPTSOriginal = ptrpl->GetPlaylistStartTimestamp();
  }

  if (posTS == nullptr)
  {
    shared_ptr<MediaSegment> seg = nullptr;
    shared_ptr<MediaSegment> altseg = nullptr;
    shared_ptr<SampleData> nextvidsample = nullptr;

    seg = pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO) == nullptr ?
      pPlaylist->GetCurrentSegmentTracker(ContentType::AUDIO) : pPlaylist->GetCurrentSegmentTracker(ContentType::VIDEO);

    if (seg == nullptr){
      if (Configuration::GetCurrent()->ResumeLiveFromPausedOrEarliest)
      {
        if (cpVideoStream->Selected() && LastPlayedVideoSegment != nullptr)
        {

          if (pPlaylist->Segments.front()->SequenceNumber > LastPlayedVideoSegment->SequenceNumber) //we no longer have those segments
          {
            seg = pPlaylist->GetSegment(pPlaylist->Segments.front()->SequenceNumber);
            pPlaylist->SetCurrentSegmentTracker(VIDEO, seg);
            pPlaylist->SetCurrentSegmentTracker(AUDIO, seg);
          }
          else
          {
            pPlaylist->SetCurrentSegmentTracker(VIDEO, LastPlayedVideoSegment);
            pPlaylist->SetCurrentSegmentTracker(AUDIO, LastPlayedVideoSegment);
            seg = LastPlayedVideoSegment;

          }

        }
      }
      else
      {
        seg = pPlaylist->GetSegment(pPlaylist->FindLiveStartSegmentSequenceNumber());
        pPlaylist->SetCurrentSegmentTracker(VIDEO, seg);
        pPlaylist->SetCurrentSegmentTracker(AUDIO, seg);
      }

    }

    if (seg == nullptr)
      return E_FAIL;

    if (seg->GetCurrentState() != INMEMORYCACHE)
    {
      auto result = Playlist::StartStreamingAsync(pPlaylist, seg->SequenceNumber).get();//wait() or get() ?
      if (FAILED(std::get<0>(result)))
        return E_FAIL;
    }


    //position the alt audio
    if (seg->HasMediaType(VIDEO))
    {
      nextvidsample = seg->PeekNextSample(seg->GetPIDForMediaType(VIDEO), MFRATE_FORWARD);
    }

    if (altpl->GetCurrentSegmentTracker(AUDIO) == nullptr)
    {
      if (Configuration::GetCurrent()->ResumeLiveFromPausedOrEarliest)
      {
        if (cpAudioStream->Selected() &&
          LastPlayedAudioSegment != nullptr &&
          LastPlayedAudioSegment->pParentPlaylist->pParentRendition != nullptr &&
          LastPlayedAudioSegment->pParentPlaylist->pParentRendition->IsMatching(altpl->pParentRendition))
        {
          if (altpl->Segments.front()->SequenceNumber > LastPlayedAudioSegment->SequenceNumber) //we no longer have those segments
          {
            altseg = altpl->GetSegment(altpl->FindAltRenditionMatchingSegment(ptrpl, seg->SequenceNumber));

            if (altseg == nullptr)
              return E_FAIL;

            if (altseg->GetCurrentState() != INMEMORYCACHE)
            {
              auto ret = Playlist::StartStreamingAsync(altpl, altseg->SequenceNumber, false, true, true).get();
              auto hr = std::get<0>(ret);
              if (FAILED(hr))
                return E_FAIL;

            }

            auto firstaltsample = altseg->PeekNextSample(altseg->GetPIDForMediaType(AUDIO), MFRATE_FORWARD);
            altpl->SetCurrentSegmentTracker(AUDIO, seg);
            altseg->SetCurrentPosition(nextvidsample->SamplePTS->ValueInTicks, TimestampMatch::ClosestLesserOrEqual, MFRATE_FORWARD, true);
          }
          else
          {
            altseg = LastPlayedAudioSegment;

            if (altseg == nullptr)
              return E_FAIL;

            if (altseg->GetCurrentState() != INMEMORYCACHE)
            {
              auto ret = Playlist::StartStreamingAsync(altpl, altseg->SequenceNumber, false, true, true).get();
              auto hr = std::get<0>(ret);
              if (FAILED(hr))
                return E_FAIL;

            }

            altpl->SetCurrentSegmentTracker(AUDIO, LastPlayedAudioSegment);
          }
        }
      }
      else
      {
        altseg = altpl->GetSegment(altpl->FindAltRenditionMatchingSegment(ptrpl, seg->SequenceNumber));

        if (altseg == nullptr)
          return E_FAIL;

        if (altseg->GetCurrentState() != INMEMORYCACHE)
        {
          auto ret = Playlist::StartStreamingAsync(altpl, altseg->SequenceNumber, false, true, true).get();
          auto hr = std::get<0>(ret);
          if (FAILED(hr))
            return E_FAIL;

        }

        auto firstaltsample = altseg->PeekNextSample(altseg->GetPIDForMediaType(AUDIO), MFRATE_FORWARD);

        altpl->SetCurrentSegmentTracker(AUDIO, altseg);
        altseg->SetCurrentPosition(nextvidsample->SamplePTS->ValueInTicks, TimestampMatch::ClosestLesserOrEqual, MFRATE_FORWARD, true);
      }

    }

    // SeekToStart = false;

    if (this->HandleInitialPauseForAutoPlay)
    {
      this->HandleInitialPauseForAutoPlay = false;
    }
    //notify source started
    NotifyStarted(posTS, S_OK);
    ////notify streams started

    if (cpVideoStream != nullptr &&cpVideoStream->Selected())
      cpVideoStream->NotifyStreamStarted(posTS);
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamStarted(posTS);


  }
  else //position specified
  {
    SetCurrentState(MSS_SEEKING);
    //set the main playlist position
    auto ret = Playlist::SetCurrentPositionLive(ptrpl, posTS->ValueInTicks);
    HRESULT hr = std::get<0>(ret);
    if (FAILED(hr)) return E_FAIL;
    //set alternate rendition playlist positions as needed
    if (ptrpl->pParentStream != nullptr && ptrpl->pParentStream->GetActiveAudioRendition() != nullptr && SUCCEEDED(hr))
    {
      hr = std::get<0>(Playlist::SetCurrentPositionLiveAlternateAudio(ptrpl->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), posTS->ValueInTicks, ptrpl->GetCurrentSegmentTracker(VIDEO)->SequenceNumber));
    }


    //if position setting failed
    if (FAILED(hr))
    {
      //notify and return
      NotifyStarted(posTS, hr);
      return S_OK;
    }

    //get the actual positions that the playlist streams got set to
    std::map<ContentType, unsigned long long> actualPos = std::get<1>(ret);
    //find the maximum position across all streams
    unsigned long long max = 0;
    for (auto itr = actualPos.begin(); itr != actualPos.end(); itr++)
    {
      if (max == 0)
        max = itr->second;
      if (max > itr->second)
        max = itr->second;
    }
    //notify source started using the max position value
    NotifyStarted(make_shared<Timestamp>(max), S_OK);

    if (cpVideoStream != nullptr && cpVideoStream->Selected())
    {
      //notify video stream started
      if (actualPos.find(ContentType::VIDEO) != actualPos.end())
      {
        cpVideoStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::VIDEO]));
      }
    }

    if (cpAudioStream != nullptr && cpAudioStream->Selected())
    {
      //notify audio stream started
      if (actualPos.find(ContentType::AUDIO) != actualPos.end())
      {
        cpAudioStream->NotifyStreamStarted(make_shared<Timestamp>(actualPos[ContentType::AUDIO]));
      }
    }

  }

  return S_OK;
}

///<summary>Pause (see IMFMediaSource on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::Pause()
{
  LOG("MediaSource Pause()");
  HRESULT hr = S_OK;


  ComPtr<AsyncCommand> pauseCmd = WRL::Make<AsyncCommand>(CommandType::PAUSE);
  this->AddRef();
  return MFPutWorkItem2(SerialWorkQueueID, 0, this, pauseCmd.Detach());
}
///<summary>Pause (see IMFMediaSource on MSDN)</summary>
HRESULT CHLSMediaSource::PauseAsync()
{

  HRESULT hr = S_OK;
  LOG("MediaSource PauseAsync()");

  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);



  auto oldstate = GetCurrentState();

  //if source buffering - end buffering
  if (IsBuffering()) EndBuffering();


  SetCurrentState(MediaSourceState::MSS_PAUSED);

  try
  {
    if (oldstate != MSS_PAUSED)
    {
      if (FAILED(hr = NotifyPaused()))
      {
        throw hr;
      }
      if (cpVideoStream != nullptr && cpVideoStream->Selected())
      {
        if (FAILED(hr = cpVideoStream->NotifyStreamPaused()))
          throw hr;
      }
      if (cpAudioStream != nullptr && cpAudioStream->Selected())
      {
        if (FAILED(hr = cpAudioStream->NotifyStreamPaused()))
          throw hr;
      }
    }

    spHeuristicsManager->StopNotifier();
    //cancel any pending bandwidth changes 
    TryCancelPendingBitrateSwitch(true);
    CancelPendingRenditionChange();

    if (spRootPlaylist != nullptr && spRootPlaylist->IsLive)
    {
      spRootPlaylist->LiveVideoPlaybackCumulativeDuration = 0;
      spRootPlaylist->LiveAudioPlaybackCumulativeDuration = 0;

      auto pl = spRootPlaylist->IsVariant && spRootPlaylist->ActiveVariant != nullptr ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      if (pl->GetCurrentSegmentTracker(VIDEO) != nullptr && pl->GetCurrentSegmentTracker(VIDEO)->GetCurrentState() == INMEMORYCACHE)
      {
        LastPlayedVideoSegment = pl->GetCurrentSegmentTracker(VIDEO);
      }
      if (pl->IsVariant == false && pl->pParentStream != nullptr && pl->pParentStream->GetActiveAudioRendition() != nullptr)
      {

        LastPlayedAudioSegment = pl->pParentStream->GetActiveAudioRendition()->spPlaylist->GetCurrentSegmentTracker(AUDIO);
      }
      else if (pl->GetCurrentSegmentTracker(AUDIO) != nullptr && pl->GetCurrentSegmentTracker(AUDIO)->GetCurrentState() == INMEMORYCACHE)
      {

        LastPlayedAudioSegment = pl->GetCurrentSegmentTracker(AUDIO);
      }

      if (pl->spswPlaylistRefresh != nullptr)
      {
        pl->spswPlaylistRefresh->StopTicking();
        pl->spswPlaylistRefresh.reset();
      }

      swStopOrPause = make_shared<StopWatch>();
      swStopOrPause->Start();

    }



  }
  catch (...)
  {
  }



  this->Release();
  UnblockReleaseBlock(tceProtectPlaylist);
  return S_OK;

}


///<summary>Stop (see IMFMediaSource on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::Stop()
{
  LOG("MediaSource Stop()");
  HRESULT hr = S_OK;

  //if (spRootPlaylist->IsLive && GetCurrentState() != MediaSourceState::MSS_PAUSED) //for live streams we do not stop unless it is a player issued stop command in which case this will be the sequence
  //  return MF_E_MEDIA_SOURCE_WRONGSTATE;
  ComPtr<AsyncCommand> stopCmd = WRL::Make<AsyncCommand>(CommandType::STOP);
  this->AddRef();
  return MFPutWorkItem2(SerialWorkQueueID, 0, this, stopCmd.Detach());

}

///<summary>Stop (see IMFMediaSource on MSDN)</summary>
HRESULT CHLSMediaSource::StopAsync()
{

  HRESULT hr = S_OK;
  LOG("MediaSource StopAsync()");

  task_completion_event<HRESULT> tceProtectPlaylist;
  BlockPrematureRelease(tceProtectPlaylist);
  //if source buffering - end buffering

  auto oldstate = GetCurrentState();

  if (IsBuffering()) EndBuffering();

  SetCurrentState(MediaSourceState::MSS_STOPPED);
  LOG("StopAsync");
  try
  {

    if (!(oldstate == MSS_STOPPED)) //if we were not already stopped
    {
      if (cpVideoStream != nullptr && cpVideoStream->Selected() && FAILED(hr = cpVideoStream->NotifyStreamStopped()))
        throw E_FAIL;
      if (cpAudioStream != nullptr && cpAudioStream->Selected() && FAILED(hr = cpAudioStream->NotifyStreamStopped()))
        throw E_FAIL;
      if (FAILED(hr = NotifyStopped()))
        throw hr;
    }

    //cancel any pending bandwidth changes 
    TryCancelPendingBitrateSwitch(true);
    CancelPendingRenditionChange();
    spRootPlaylist->CancelDownloads();


    if (spRootPlaylist->IsLive)
    {
      spRootPlaylist->LiveVideoPlaybackCumulativeDuration = 0;
      spRootPlaylist->LiveAudioPlaybackCumulativeDuration = 0;

      auto pl = spRootPlaylist->IsVariant && spRootPlaylist->ActiveVariant != nullptr ? spRootPlaylist->ActiveVariant->spPlaylist : spRootPlaylist;
      if (pl->GetCurrentSegmentTracker(VIDEO) != nullptr && pl->GetCurrentSegmentTracker(VIDEO)->GetCurrentState() == INMEMORYCACHE)
      {
        LastPlayedVideoSegment = pl->GetCurrentSegmentTracker(VIDEO);
      }
      if (pl->IsVariant == false && pl->pParentStream != nullptr && pl->pParentStream->GetActiveAudioRendition() != nullptr)
      {

        LastPlayedAudioSegment = pl->GetCurrentSegmentTracker(AUDIO);
      }
      else if (pl->GetCurrentSegmentTracker(AUDIO) != nullptr && pl->GetCurrentSegmentTracker(AUDIO)->GetCurrentState() == INMEMORYCACHE)
      {

        LastPlayedAudioSegment = pl->GetCurrentSegmentTracker(AUDIO);
      }

      if (pl->spswPlaylistRefresh != nullptr)
      {
        pl->spswPlaylistRefresh->StopTicking();
        pl->spswPlaylistRefresh.reset();
      }

      swStopOrPause = make_shared<StopWatch>();
      swStopOrPause->Start();
    }

  }
  catch (task_canceled)
  {

  }
  catch (...)
  {

  }
  this->Release();
  UnblockReleaseBlock(tceProtectPlaylist);
  return S_OK;
}

IFACEMETHODIMP CHLSMediaSource::Shutdown()
{
  LOG("Shutdown()");
  HRESULT hr = S_OK;




  if (cpController != nullptr)
    cpController->Invalidate();

  auto oldstate = GetCurrentState();

  if (oldstate == MSS_STARTED)
  {
    if (cpVideoStream != nullptr && cpVideoStream->Selected())
      cpVideoStream->NotifyStreamEnded();
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamEnded();
    NotifyPresentationEnded();
  }

  SetCurrentState(MediaSourceState::MSS_UNINITIALIZED);


  {
    std::lock_guard<std::recursive_mutex> lock(LockEvent); //ensure that no ongoing event op gets disrupted midstream.

    MFUnlockWorkQueue(StreamWorkQueueID);
    StreamWorkQueueID = 0;
    MFUnlockWorkQueue(SerialWorkQueueID);
    SerialWorkQueueID = 0;
    if (sourceEventQueue != nullptr)
    {
      sourceEventQueue->Shutdown();
      sourceEventQueue = nullptr;
    }
  }


  /* MFLockPlatform();*/
  if (oldstate != MSS_UNINITIALIZED && oldstate != MSS_ERROR)
  {
    try
    {
      if (spHeuristicsManager != nullptr)
        spHeuristicsManager->StopNotifier();

      //cancel all downloads     
      if (spRootPlaylist != nullptr)
      {
        this->AddRef();

        task<void>([this]()
        {
          try
          {
            spRootPlaylist->CancelDownloadsAndWaitForCompletion();
          }
          catch (...)
          {
          }
          try
          {
            //LOG("Waiting for protection registry...");
            protectionRegistry.CleanupCompletedOrCanceled();
            protectionRegistry.WhenAll().wait();
            spRootPlaylist.reset();
            this->Release();
          }
          catch (...)
          {
            spRootPlaylist.reset();
            this->Release();
          }

        }, Concurrency::task_options(Concurrency::task_continuation_context::use_arbitrary()));


      }
    }
    catch (...)
    {

    }
  }

  //MFUnlockPlatform();
  return S_OK;

}

///<summary>Shutdown (see IMFMediaSource on MSDN)</summary>
//IFACEMETHODIMP CHLSMediaSource::Shutdown()
//{
//  LOG("Shutdown()");
//  HRESULT hr = S_OK;
//
//
//  if (cpController != nullptr)
//    cpController->Invalidate();
//
//  auto oldstate = GetCurrentState();
//  SetCurrentState(MediaSourceState::MSS_UNINITIALIZED);
//
//  MFUnlockWorkQueue(StreamWorkQueueID);
//  StreamWorkQueueID = 0;
//
//  if (sourceEventQueue != nullptr){
//    sourceEventQueue->Shutdown();
//    sourceEventQueue = nullptr;
//  }
//    
//  ComPtr<AsyncCommand> shutdownCmd = WRL::Make<AsyncCommand>(CommandType::SHUTDOWN, oldstate);
//  this->AddRef();
//  return MFPutWorkItem2(SerialWorkQueueID, 0, this, shutdownCmd.Detach());
//
//
//}



//HRESULT CHLSMediaSource::ShutdownAsync(MediaSourceState oldstate)
//{
//  LOG("ShutdownAsync()"); 
//  MFLockPlatform();
//  if (oldstate != MSS_UNINITIALIZED && oldstate != MSS_ERROR)
//  {
//    try
//    {
//      if (spHeuristicsManager != nullptr)
//        spHeuristicsManager->StopNotifier();
//
//      //cancel all downloads     
//      if (spRootPlaylist != nullptr)
//      {
//        spRootPlaylist->CancelDownloadsAndWaitForCompletion().then([this]() {
//
//          protectionRegistry.CleanupCompletedOrCanceled();
//
//          protectionRegistry.WhenAll().then([this]() {
//            spRootPlaylist.reset();
//           
//          });
//        });
//      }
//    }
//    catch (...)
//    {
//     
//    }
//  }
//
// 
//  MFUnlockWorkQueue(SerialWorkQueueID); 
//  SerialWorkQueueID = 0;
//
//  this->Release();
//  MFUnlockPlatform();
// 
//  return S_OK;
//
//
//}

#pragma endregion

#pragma region RATE MANAGEMENT

size_t CHLSMediaSource::GetPlaybackRateDistanceFromNormal()
{
  if (curPlaybackRate->Rate < 0.0)
  {
    auto curitr = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> t) { return t->Rate == curPlaybackRate->Rate; });
    auto zeroitr = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> t) { return t->Rate == 0.0; });
    return std::distance(zeroitr, curitr);
  }
  else if (curPlaybackRate->Rate > 1.0)
  {
    auto curitr = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> t) { return t->Rate == curPlaybackRate->Rate; });
    auto oneitr = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> t) { return t->Rate == 1.0; });
    return  std::distance(oneitr, curitr);
  }
  else
    return 0;
}

///<summary>GetRate (see IMFRateControl on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetRate(BOOL *pfThin, float *pflRate)
{


  *pfThin = (curPlaybackRate->Thinned ? TRUE : FALSE);
  *pflRate = curPlaybackRate->Rate;
  //LOG("GetRate() : Rate = " << curPlaybackRate->Rate << ", Thinning = " << (curPlaybackRate->Thinned ? L"True" : L"False"));
  return S_OK;
}

///<summary>SetRate (see IMFRateControl on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::SetRate(BOOL fThin, float flRate)
{


  LOG("SetRate() : Rate = " << flRate << ", Thinning = " << (fThin == TRUE ? L"True" : L"False"));



  auto thinval = (fThin == TRUE ? true : false);
  auto found = std::find_if(SupportedRates.begin(), SupportedRates.end(), [flRate, thinval, this](shared_ptr<VariableRate>  val)
  {
    return val->Rate == flRate && val->Thinned == thinval;
  });
  if (found == SupportedRates.end())
    return MF_E_UNSUPPORTED_RATE;

  if (curPlaybackRate != nullptr)
    prevPlaybackRate = curPlaybackRate;
  curPlaybackRate = *found;
  /*if((curDirection == MFRATE_DIRECTION::MFRATE_FORWARD && flRate < 0 || curDirection == MFRATE_DIRECTION::MFRATE_REVERSE && flRate >= 0))
  {
  spRootPlaylist->SwitchDirection();
  }*/

  curDirection = (flRate >= 0 ? MFRATE_DIRECTION::MFRATE_FORWARD : MFRATE_DIRECTION::MFRATE_REVERSE);

  if (curPlaybackRate->Thinned == true && fThin == false)
  {
    if (cpVideoStream != nullptr && cpVideoStream->Selected())
      cpVideoStream->NotifyStreamThinning(false);
    if (cpAudioStream != nullptr && cpAudioStream->Selected())
      cpAudioStream->NotifyStreamThinning(false);
  }

  if (curPlaybackRate->Rate != 1)
  {
    //cancel any pending bandwidth changes 
    TryCancelPendingBitrateSwitch(true);
    CancelPendingRenditionChange();
    //stop bitrate changes
    if (GetCurrentState() == MSS_STARTED && spHeuristicsManager->IsBitrateChangeNotifierRunning())
      spHeuristicsManager->StopNotifier();
  }
  else
  {
    if (GetCurrentState() == MSS_STARTED && spHeuristicsManager->IsBitrateChangeNotifierRunning() == false && Configuration::GetCurrent()->EnableBitrateMonitor == true)
      spHeuristicsManager->StartNotifier();
  }

  if (GetCurrentState() == MSS_STARTING && flRate == 0.0)
    this->HandleInitialPauseForAutoPlay = true;
  else
    this->HandleInitialPauseForAutoPlay = false;

  if (cpVideoStream != nullptr && cpVideoStream->Selected() && GetCurrentPlaybackRate()->Thinned)
    cpVideoStream->NotifyStreamThinning(true);
  if (cpAudioStream != nullptr && cpAudioStream->Selected() && GetCurrentPlaybackRate()->Thinned)
    cpAudioStream->NotifyStreamThinning(true);
  return NotifyRateChanged(curPlaybackRate->Rate);
}

///<summary>GetFastestRate (see IMFRateSupport on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetFastestRate(MFRATE_DIRECTION direction, BOOL fThin, float *pflRate)
{


  if (direction == MFRATE_DIRECTION::MFRATE_REVERSE)
  {
    if (SupportedRates.front()->Rate >= 0.0)
      return MF_E_REVERSE_UNSUPPORTED;
    else if (fThin)
    {
      auto match = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate < 0 && val->Thinned == true;
      });
      if (match == SupportedRates.end())
        return MF_E_THINNING_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
    else
    {
      auto match = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate>  val)
      {
        return val->Rate < 0 && val->Thinned == false;
      });
      if (match == SupportedRates.end())
        return MF_E_REVERSE_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
  }
  else
  {
    if (fThin)
    {
      auto match = std::find_if(SupportedRates.rbegin(), SupportedRates.rend(), [this](shared_ptr<VariableRate>  val)
      {
        return val->Rate >= 0 && val->Thinned == true;
      });
      if (match == SupportedRates.rend())
        return MF_E_THINNING_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
    else
    {
      auto match = std::find_if(SupportedRates.rbegin(), SupportedRates.rend(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate >= 0 && val->Thinned == false;
      });
      if (match == SupportedRates.rend())
        *pflRate = 1.0;
      else
        *pflRate = (*match)->Rate;
    }
  }

  //LOG("GetFastestRate() : Thinning = " << (fThin == TRUE ? L"True" : L"False") << ",Reverse = " << (direction == MFRATE_DIRECTION::MFRATE_REVERSE ? L"True" : L"False") << ", Rate = " << *pflRate);
  return S_OK;
}

///<summary>GetSlowestRate (see IMFRateSupport on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::GetSlowestRate(MFRATE_DIRECTION direction, BOOL fThin, float *pflRate)
{

  if (direction == MFRATE_DIRECTION::MFRATE_REVERSE)
  {
    if (SupportedRates.front()->Rate >= 0.0)
      return MF_E_REVERSE_UNSUPPORTED;
    else if (fThin)
    {
      auto match = std::find_if(SupportedRates.rbegin(), SupportedRates.rend(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate < 0 && val->Thinned == true;
      });
      if (match == SupportedRates.rend())
        return MF_E_THINNING_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
    else
    {
      auto match = std::find_if(SupportedRates.rbegin(), SupportedRates.rend(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate < 0 && val->Thinned == false;
      });
      if (match == SupportedRates.rend())
        return MF_E_REVERSE_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
  }
  else
  {
    if (fThin)
    {
      auto match = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate >= 0 && val->Thinned == true;
      });
      if (match == SupportedRates.end())
        return MF_E_THINNING_UNSUPPORTED;
      else
        *pflRate = (*match)->Rate;
    }
    else
    {
      auto match = std::find_if(SupportedRates.begin(), SupportedRates.end(), [this](shared_ptr<VariableRate> val)
      {
        return val->Rate >= 0 && val->Thinned == false;
      });
      if (match == SupportedRates.end())
        *pflRate = 0.0;
      else
        *pflRate = (*match)->Rate;
    }
  }

  //LOG("GetSlowestRate() : Thinning = " << (fThin == TRUE ? L"True" : L"False") << ",Reverse = " << (direction == MFRATE_DIRECTION::MFRATE_REVERSE ? L"True" : L"False") << ", Rate = " << *pflRate);
  return S_OK;
}

///<summary>IsRateSupported (see IMFRateSupport on MSDN)</summary>
IFACEMETHODIMP CHLSMediaSource::IsRateSupported(BOOL fThin, float flRate, float *pflNearestSupportedRate)
{

  auto RateEntry = std::find_if(SupportedRates.begin(), SupportedRates.end(), [flRate, this](shared_ptr<VariableRate> entry)
  {
    return entry->Rate == flRate;
  });
  if (RateEntry == SupportedRates.end()) return MF_E_UNSUPPORTED_RATE;
  if (fThin && (*RateEntry)->Thinned == false)  return MF_E_THINNING_UNSUPPORTED;
  if (pflNearestSupportedRate != nullptr)
    *pflNearestSupportedRate = flRate;
  //LOG("IsRateSupported() : Thinning = " << (fThin == TRUE ? L"True" : L"False") << ", Rate = " << flRate);
  return S_OK;


}

shared_ptr<VariableRate> CHLSMediaSource::GetCurrentPlaybackRate()
{
  return curPlaybackRate;
}

MFRATE_DIRECTION CHLSMediaSource::GetCurrentDirection()
{
  return curDirection;
}

#pragma endregion


//IFACEMETHODIMP CHLSMediaSource::DropTime(LONGLONG hnsAmountToDrop)
//{
//  return MF_E_DROPTIME_NOT_SUPPORTED;
//}
//IFACEMETHODIMP CHLSMediaSource::GetDropMode(MF_QUALITY_DROP_MODE *pDropMode)
//{
//  *pDropMode = this->curDropMode;
//  return S_OK;
//}
//IFACEMETHODIMP CHLSMediaSource::GetQualityLevel(MF_QUALITY_LEVEL *pQualityLevel)
//{
//  *pQualityLevel = this->curQualityLevel;
//  return S_OK;
//}
//IFACEMETHODIMP CHLSMediaSource::SetDropMode(MF_QUALITY_DROP_MODE dropMode)
//{
//  return MF_E_NO_MORE_DROP_MODES;
//}
//IFACEMETHODIMP CHLSMediaSource::SetQualityLevel(MF_QUALITY_LEVEL qualityLevel)
//{
//  return MF_E_NO_MORE_QUALITY_LEVELS;
//}
