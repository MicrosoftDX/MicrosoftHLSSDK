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
#include "MFVideoStream.h"
#include "HLSMediaSource.h"
#include "TSConstants.h"
#include "HLSController.h" 
#include "HLSPlaylist.h"


using namespace Microsoft::HLSClient::Private;

void CMFVideoStream::SwitchBitrate()
{
  CMFStreamCommonImpl::SwitchBitrate(); 
 
}

///<summary>See IMFAsyncCallback in MSDN</summary>
IFACEMETHODIMP CMFVideoStream::Invoke(IMFAsyncResult *pResult)
{
  //complete an asynchronous sample request work item
  HRESULT hr = CompleteAsyncSampleRequest(pResult, ContentType::VIDEO);
  if (hr == E_FAIL || pResult->GetStatus() == E_FAIL)
    cpMediaSource->NotifyError(hr);
  pResult->Release();
  return S_OK;
}

///<summary>Constructor</summary>
///<param name='pSource'>The parent media source</param>
///<param name='pplaylist'>The current playlist</param>
CMFVideoStream::CMFVideoStream(CHLSMediaSource *pSource, Playlist *playlist, DWORD CommonWorkQueue) : CMFStreamCommonImpl(pSource, playlist, CommonWorkQueue)
{
}

///<summary>Switch completed event</summary>
void CMFVideoStream::RaiseBitrateSwitched(unsigned int From, unsigned int To)
{
  //raise the bitrate switch completion event on a background thread - do not block stream
  if (cpMediaSource != nullptr && cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
  {
    cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, this](){
      if (cpMediaSource != nullptr &&
        cpMediaSource->GetCurrentState() != MSS_STOPPED && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
        cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_PAUSED
        && cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
        cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(ContentType::VIDEO, From, To);
      return S_OK; }));
  }
}
///<summary>Switch the stream to an alternate rendition</summary>
void CMFVideoStream::SwitchRendition()
{
  //valid switch request ?
  if (pendingRenditionSwitch == nullptr)
    return;
  unsigned int currentbr = 0;
  unsigned int targetbr = 0;

  std::lock_guard<std::recursive_mutex> lock(LockStream);

  //switch playlist
  pPlaylist = pendingRenditionSwitch->targetPlaylist;
  //request request
  pendingRenditionSwitch.reset();


}

///<summary>Construct stream descriptor</summary>
HRESULT CMFVideoStream::ConstructStreamDescriptor(unsigned int id)
{
  HRESULT hr = S_OK;

  ComPtr<IMFMediaTypeHandler> cpMediaTypeHandler; 
  ComPtr<IMFAttributes> cpAttribs = nullptr;

  if (FAILED(hr = MFCreateMediaType(&cpMediaType))) return hr;
  cpMediaType.As(&cpAttribs);
  //major type
  if (FAILED(hr = cpMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video))) return hr;
  //sub type

  if (FAILED(hr = cpMediaType->SetGUID(MF_MT_SUBTYPE, pPlaylist->pParentStream != nullptr && IsEqualGUID(pPlaylist->pParentStream->VideoMediaType, GUID_NULL) == false ? pPlaylist->pParentStream->VideoMediaType : MFVideoFormat_H264))) return hr;
  //interlace mode
  if (FAILED(hr = cpMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlaceMode::MFVideoInterlace_Progressive))) return hr;
  //average bitrate
  if (pPlaylist->pParentStream != nullptr)
  {
    auto highestsupportedbitrate = pPlaylist->pParentStream->pParentPlaylist->Variants.rbegin()->first;
    if (FAILED(hr = cpMediaType->SetUINT32(MF_MT_AVG_BITRATE, highestsupportedbitrate))) return hr;
  }
  //resolution
  //


  if (FAILED(hr = MFSetAttributeSize(cpAttribs.Get(), MF_MT_FRAME_SIZE, 1920, 1080))) return hr;
 
  //create SD
  if (FAILED(hr = MFCreateStreamDescriptor(id, 1, cpMediaType.GetAddressOf(), &cpStreamDescriptor))) return hr;
  //set media type
  if (FAILED(hr = cpStreamDescriptor->GetMediaTypeHandler(&cpMediaTypeHandler))) return hr;
  return cpMediaTypeHandler->SetCurrentMediaType(cpMediaType.Get());
}
