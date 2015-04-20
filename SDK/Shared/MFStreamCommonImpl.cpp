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
#include "StopWatch.h"
#include "MFStreamCommonImpl.h"
#include "HLSMediaSource.h" 
#include "PlaylistOM.h" 
#include "Timestamp.h" 

using namespace Microsoft::HLSClient::Private;

ComPtr<IMFSample> CMFStreamCommonImpl::CreateEmptySample(long long ts)
{
  ComPtr<IMFSample> ret;
  HRESULT hr = MFCreateSample(&ret);
  if (FAILED(hr)) return nullptr;
  if (ts >= 0)
    ret->SetSampleTime(ts);
  return ret.Detach();
}

#pragma region IMFMediastream
///<summary>See IMFMediaStream in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::GetMediaSource(IMFMediaSource **ppMediaSource)
{
  return cpMediaSource->QueryInterface(IID_PPV_ARGS(ppMediaSource));
};
///<summary>See IMFMediaStream in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::GetStreamDescriptor(IMFStreamDescriptor **ppStreamDescriptor)
{
  return cpStreamDescriptor.Get()->QueryInterface(IID_PPV_ARGS(ppStreamDescriptor));
}

///<summary>Notify a fatal error</summary>
///<param name='hrErrorCode'>Error code</param>
HRESULT CMFStreamCommonImpl::NotifyError(HRESULT hrErrorCode)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MEError, GUID_NULL, hrErrorCode, &pvar);
  ::PropVariantClear(&pvar);

  return hr;
}
///<summary>See IMFMediaStream in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::RequestSample(IUnknown *pToken)
{
  //ensure we are in the right state

  if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_STOPPED /*|| cpMediaSource->GetCurrentState() == MediaSourceState::MSS_PAUSED*/)
  {
    LOG("RequestSample : Bad State - Stopped/Paused");
    return MF_E_MEDIA_SOURCE_WRONGSTATE;
  }
  if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_ERROR)
  {
    LOG("RequestSample : Bad State - Error");
    cpMediaSource->NotifyError(MF_E_MEDIA_SOURCE_WRONGSTATE);
    /*return E_FAIL;*/
    return S_OK;
  }
  if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED)
  {
    LOG("RequestSample : Bad State - Uninitialized");
    return MF_E_SHUTDOWN;
  }
  //store the token
  if (nullptr != pToken)
    try{
    sampletokenQueue.push(pToken);
  }
  catch (...)
  {
    LOG("RequestSample : Token Error");
    return E_FAIL;
  }

  HRESULT hr = S_OK;
  //create and schedule work item in the work queue
  ComPtr<IMFAsyncResult> pResult = nullptr;
  if (FAILED(hr = MFCreateAsyncResult(nullptr, this, nullptr, &pResult)))
  {
    LOG("RequestSample : Could not create async result");
    return hr;
  }
  if (FAILED(hr = MFPutWorkItemEx2(SerialWorkQueueID, 0, pResult.Detach())))
  {
    LOG("RequestSample : Could not create work item");
    return hr;
  }
  return hr;
}

#pragma endregion


///<summary>See IMFAsyncCallback in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::GetParameters(DWORD *pdwFlags, DWORD *pdwQueue)
{
  *pdwFlags = 0;
  //return our serial work queue
  *pdwQueue = SerialWorkQueueID;
  return S_OK;
}


#pragma region IMFMediaEventGenerator
///<summary>See IMFMediaEventGenerator in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::BeginGetEvent(IMFAsyncCallback *pCallback, IUnknown *punkState)
{
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);

  hr = cpEventQueue->BeginGetEvent(pCallback, punkState);
  return hr;
}
///<summary>See IMFMediaEventGenerator in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::EndGetEvent(IMFAsyncResult *pResult, IMFMediaEvent **ppEvent)
{
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = cpEventQueue->EndGetEvent(pResult, ppEvent);

  return hr;
}
///<summary>See IMFMediaEventGenerator in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::GetEvent(DWORD dwFlags, IMFMediaEvent **ppEvent)
{
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = cpEventQueue->GetEvent(dwFlags, ppEvent);

  return hr;
}
///<summary>See IMFMediaEventGenerator in MSDN</summary>
IFACEMETHODIMP CMFStreamCommonImpl::QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT *pvValue)
{
  HRESULT hr = S_OK;
  std::lock_guard<std::recursive_mutex> lock(LockEvent);
  hr = cpEventQueue->QueueEventParamVar(met, guidExtendedType, hrStatus, pvValue);
  return hr;
}
#pragma endregion

#pragma region Class specific 
///<summary>Called by derived stream types to complete async sample request work items</summary>
///<param name='pResult'>Callback result</param>
///<param name='forContentType'>The content type for which a sample is being requested</param>
HRESULT CMFStreamCommonImpl::CompleteAsyncSampleRequest(IMFAsyncResult *pResult, ContentType forContentType)
{

  HRESULT hr = S_OK;

  if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_STOPPED ||
    cpMediaSource->GetCurrentState() == MediaSourceState::MSS_ERROR/* ||
                                                                   cpMediaSource->GetCurrentState() == MediaSourceState::MSS_PAUSED*/)
  {
    LOG("Sample Request : Bad State");
    pResult->SetStatus(MF_E_MEDIA_SOURCE_WRONGSTATE);
    return S_OK;
  }
  if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED)
  {
    LOG("Sample Request : Uninitialized");
    pResult->SetStatus(MF_E_SHUTDOWN);
    return S_OK;
  }

  ComPtr<IMFSample> pSample = nullptr;
  try{
    //call the appropriate sample request method on the current playlist 
    //based on whether we are clamping to IDR pictures (key frames) or not
    hr = Playlist::RequestSample(pPlaylist, forContentType, &pSample);

  }
  catch (...)
  {

    LOG("Sample Request : Exception");
    pResult->SetStatus(E_FAIL);
    return S_OK;
  }

  //end of stream
  if (hr == MF_E_END_OF_STREAM)
  {
    NotifyStreamEnded();
    if (pPlaylist->IsEOS())
      cpMediaSource->NotifyPresentationEnded();
    pResult->SetStatus(S_OK);
    return S_OK;
  }

  if (FAILED(hr)) //other failures - we cannot continue playback
  {
    LOG("Sample Request : Cannot play stream");
    pResult->SetStatus(E_FAIL);
    return E_FAIL;
  }


  try{
    if (SUCCEEDED(hr))
    {
      ComPtr<IUnknown> nextToken = nullptr;
      //are there tokens ?
      if (sampletokenQueue.size() > 0)
      {
        //get the next token
        nextToken = sampletokenQueue.front();
        //associate with sample
        if (pSample != nullptr)
          pSample->SetUnknown(MFSampleExtension_Token, nextToken.Get());
        //pop
        sampletokenQueue.pop();
      }
      //notify sample available
      if (pSample != nullptr)
      { 
        NotifySample(pSample.Get());

      }
      else
        NotifySample(nullptr);

      //release token
      if (nextToken != nullptr)
        nextToken.Reset();
    }
  }
  catch (...)
  {
    if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_STOPPED ||
      cpMediaSource->GetCurrentState() == MediaSourceState::MSS_ERROR ||
      cpMediaSource->GetCurrentState() == MediaSourceState::MSS_PAUSED)
    {
      LOG("Sample Request : Exception : Bad State");
      pResult->SetStatus(MF_E_MEDIA_SOURCE_WRONGSTATE);
      return S_OK;
    }
    if (cpMediaSource->GetCurrentState() == MediaSourceState::MSS_UNINITIALIZED)
    {

      LOG("Sample Request : Exception : Uninitialized");
      pResult->SetStatus(MF_E_SHUTDOWN);
      return S_OK;
    }
    LOG("Sample Request : Exception : E_FAIL");
    pResult->SetStatus(E_FAIL);
    return S_OK;
  }
  pResult->SetStatus(S_OK);
  return hr;
}

///<summary>Switches the stream from one bitrate to another</summary>
void CMFStreamCommonImpl::SwitchBitrate()
{
  std::lock_guard<std::recursive_mutex> lock(LockSwitch);   //if no switch is pending return
  if (pendingBitrateSwitch == nullptr)
    return;

  if (pPlaylist->pParentRendition != nullptr) //no bitrate switch in alt rendition - main track bitrate switch will also handle this
    return;

  unsigned int currentbr = 0;
  unsigned int targetbr = 0;

  //get current bitrate
  currentbr = pPlaylist->pParentStream->Bandwidth;
  //get target bitrate
  targetbr = pendingBitrateSwitch->targetPlaylist->pParentStream->Bandwidth;
  //get target playlist
  pPlaylist = pendingBitrateSwitch->targetPlaylist;
  //reset the bitrate switch request
  pendingBitrateSwitch.reset();


  RaiseBitrateSwitched(currentbr, targetbr); //derived class will raise event
}



///<summary>Schedule a playloist switch (bitrate or rendition)</summary>
///<param name='pPlaylist'>Target playlist</param>
///<param name='type'>Switch type (Bitrate or Rendition)</param>
///<param name='atSegmentBoundary'>True if the switch should take place 
///when the stream changes to the next segment or false if the switch should be immediate</param>
void CMFStreamCommonImpl::ScheduleSwitch(Playlist *pplaylist, PlaylistSwitchRequest::SwitchType type, bool IgnoreBuffer)
{
  std::lock_guard<std::recursive_mutex> lock(LockSwitch);
  //if switching bitrate
  if (type == PlaylistSwitchRequest::BITRATE)
  {
    //if non null target playlist
    if (pplaylist != nullptr)
      //construct and store a switch request
      pendingBitrateSwitch = make_shared<PlaylistSwitchRequest>(pplaylist, this->pPlaylist, IgnoreBuffer);
    else
      //clear any pending switch request
      pendingBitrateSwitch.reset();
  }
  //if switching rendition
  else if (type == PlaylistSwitchRequest::RENDITION)
  {
    //if non null target playlist
    if (pplaylist != nullptr)
      //construct and store a switch request
      pendingRenditionSwitch = make_shared<PlaylistSwitchRequest>(pplaylist, this->pPlaylist);
    else
      //clear any pending switch request
      pendingRenditionSwitch.reset();
  }

}

///<summary>Constructor</summary>
///<param name='pSource'>The parent media source</param>
///<param name='pplaylist'>The current playlist</param>
CMFStreamCommonImpl::CMFStreamCommonImpl(CHLSMediaSource *pSource, Playlist *playlist, DWORD CommonWorkQueue) :
cpMediaSource(pSource), pPlaylist(playlist), IsSelected(false), pendingBitrateSwitch(nullptr), pendingRenditionSwitch(nullptr), SerialWorkQueueID(CommonWorkQueue),
StreamTickBase(nullptr), ApproximateFrameDistance(0)

{
  MFCreateEventQueue(&cpEventQueue);
}

///<summary>Send start notification</summary>
///<param name='startAt'>Position to start at</param>
HRESULT CMFStreamCommonImpl::NotifyStreamStarted(std::shared_ptr<Timestamp> startAt)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  if (startAt != nullptr)
  {
    pvar.vt = VT_I8;
    pvar.hVal.QuadPart = startAt->ValueInTicks;
  }
  else
    pvar.vt = VT_EMPTY;
  hr = QueueEvent(MEStreamStarted, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

HRESULT CMFStreamCommonImpl::NotifyStreamSeeked(std::shared_ptr<Timestamp> startAt)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  if (startAt != nullptr)
  {
    pvar.vt = VT_I8;
    pvar.hVal.QuadPart = startAt->ValueInTicks;
  }
  else
    pvar.vt = VT_EMPTY;
  hr = QueueEvent(MEStreamSeeked, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

HRESULT CMFStreamCommonImpl::NotifyStreamTick(unsigned long long ts)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);

  pvar.vt = VT_I8;
  pvar.hVal.QuadPart = ts;
  hr = QueueEvent(MEStreamTick, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Send stream updated notification(MEUpdatedStream)</summary>
HRESULT CMFStreamCommonImpl::NotifyStreamUpdated()
{

  HRESULT hr = S_OK;
  ComPtr<IUnknown> cpUnk;
  ComPtr<IMFMediaStream>(this).As(&cpUnk);
  hr = cpEventQueue->QueueEventParamUnk(MEUpdatedStream, GUID_NULL, S_OK, cpUnk.Get());
  return hr;
}

HRESULT CMFStreamCommonImpl::NotifyStreamThinning(bool Started)
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  pvar.vt = VT_BOOL;
  pvar.boolVal = Started ? VARIANT_TRUE : VARIANT_FALSE;
  hr = QueueEvent(MEStreamThinMode, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Send end notification(MEEndOfStream)</summary>
HRESULT CMFStreamCommonImpl::NotifyStreamEnded()
{
  PROPVARIANT pvar;
  HRESULT hr = S_OK;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MEEndOfStream, GUID_NULL, S_OK, &pvar);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Send sample available notification notification(MEMediaSample)</summary>
HRESULT CMFStreamCommonImpl::NotifySample(IMFSample *pSample)
{
  HRESULT hr = S_OK;
  hr = cpEventQueue->QueueEventParamUnk(MEMediaSample, GUID_NULL, S_OK, pSample);
  return hr;
}

///<summary>Send sample available notification notification(MEMediaSample)</summary>
HRESULT CMFStreamCommonImpl::NotifyFormatChanged(IMFMediaType *pMediaType)
{
  HRESULT hr = S_OK;
  if (pMediaType != nullptr)
    hr = cpEventQueue->QueueEventParamUnk(MEStreamFormatChanged, GUID_NULL, S_OK, pMediaType);
  else
  {
    ComPtr<IMFMediaType> cpTypeToApply;
    if (cpMediaType != nullptr && SUCCEEDED(MFCreateMediaType(&cpTypeToApply)) && SUCCEEDED(cpMediaType->CopyAllItems(cpTypeToApply.Get())))
    {
      hr = cpEventQueue->QueueEventParamUnk(MEStreamFormatChanged, GUID_NULL, S_OK, cpTypeToApply.Get());
    }
  }
  return hr;
}
///<summary>Send stream stop notification(MEStreamStopped)</summary>
HRESULT CMFStreamCommonImpl::NotifyStreamStopped()
{
  HRESULT hr = S_OK;
  PROPVARIANT pvar;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MEStreamStopped, GUID_NULL, S_OK, nullptr);
  ::PropVariantClear(&pvar);
  return hr;
}

///<summary>Send stream paused notification(MEStreamPaused)</summary>
HRESULT CMFStreamCommonImpl::NotifyStreamPaused()
{
  HRESULT hr = S_OK;
  PROPVARIANT pvar;
  ::PropVariantInit(&pvar);
  hr = QueueEvent(MEStreamPaused, GUID_NULL, S_OK, nullptr);
  ::PropVariantClear(&pvar);
  return hr;
}
#pragma endregion
