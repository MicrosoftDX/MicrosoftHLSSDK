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
#include "HLSMediaSource.h"
#include "MFAudioStream.h"
#include "ABI\HLSController.h"
#include "ABI\HLSPlaylist.h"
#include "TransportStream\TSConstants.h"
#include "mp3HeaderParser\MP3HeaderParser.h"

using namespace Microsoft::HLSClient::Private;


void CMFAudioStream::SwitchBitrate()
{
  CMFStreamCommonImpl::SwitchBitrate();
 
}
///<summary>See IMFAsyncCallback in MSDN</summary>
IFACEMETHODIMP CMFAudioStream::Invoke(IMFAsyncResult *pResult)
{
  //complete an asynchronous sample request work item
  HRESULT hr = CompleteAsyncSampleRequest(pResult, ContentType::AUDIO);
  if (hr == E_FAIL || pResult->GetStatus() == E_FAIL)
    cpMediaSource->NotifyError(hr);
  pResult->Release();
  return S_OK;
}

///<summary>Constructor</summary>
///<param name='pSource'>The parent media source</param>
///<param name='pplaylist'>The current playlist</param>
CMFAudioStream::CMFAudioStream(CHLSMediaSource *pSource, Playlist *playlist, DWORD CommonWorkQueue) : CMFStreamCommonImpl(pSource, playlist, CommonWorkQueue)
{
}

///<summary>Switch completed event</summary>
void CMFAudioStream::RaiseBitrateSwitched(unsigned int From, unsigned int To)
{
  //raise the bitrate switch completion event on a background thread - do not block stream
  if (cpMediaSource != nullptr && cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
  {
    cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, this](){
      if (cpMediaSource != nullptr &&
        cpMediaSource->GetCurrentState() != MSS_STOPPED &&
        cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
        cpMediaSource->GetCurrentState() != MSS_ERROR
        && cpMediaSource->GetCurrentState() != MSS_PAUSED &&
        cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
        cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(ContentType::AUDIO, From, To); 
      return S_OK; }));
  }
}
///<summary>Switch the stream to an alternate rendition</summary>
void CMFAudioStream::SwitchRendition()
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
HRESULT CMFAudioStream::ConstructStreamDescriptor(unsigned int id)
{
  HRESULT hr = S_OK;

  ComPtr<IMFMediaTypeHandler> cpMediaTypeHandler; 
  if (FAILED(hr = MFCreateMediaType(&cpMediaType))) return hr;
  //major type
  if (FAILED(hr = cpMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio))) return hr;

  if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->AudioMediaType == MFAudioFormat_MP3)
  {
    //sub type from playlist  
    if (FAILED(hr = cpMediaType->SetGUID(MF_MT_SUBTYPE, pPlaylist->pParentStream->AudioMediaType))) return hr;
    
    MPEGLAYER3WAVEFORMAT wfInfo;

    auto audioseg = pPlaylist->GetCurrentSegmentTracker(AUDIO);
    if (nullptr == audioseg) return E_FAIL;
    auto sample = audioseg->PeekNextSample(audioseg->GetPIDForMediaType(AUDIO), MFRATE_FORWARD);
    std::vector<BYTE> audiodata;
    audiodata.resize(sample->TotalLen);
    unsigned int ctr = 0;
    for (auto elemdata : sample->elemData)
    {
      memcpy_s(&(*(audiodata.begin() + ctr)), std::get<1>(elemdata), std::get<0>(elemdata), std::get<1>(elemdata));
      ctr += std::get<1>(elemdata);
    };

    if (!MP3HeaderParser::Parse(&(*(audiodata.begin())), sample->TotalLen, &wfInfo))
      return E_FAIL;

 
    
    if (FAILED(hr = ::MFInitMediaTypeFromWaveFormatEx(cpMediaType.Get(), (WAVEFORMATEX*) &wfInfo, sizeof(MPEGLAYER3WAVEFORMAT))))
      return hr;
     

  //  if (FAILED(hr = cpMediaType->SetBlob(MF_MT_USER_DATA, reinterpret_cast<const UINT8*>(&(wfInfo.wID)), MPEGLAYER3_WFX_EXTRA_BYTES))) return hr;
  }
  else
  {
    //sub type from playlist or default of AAC
    if (FAILED(hr = cpMediaType->SetGUID(MF_MT_SUBTYPE, pPlaylist->pParentStream != nullptr && IsEqualGUID(pPlaylist->pParentStream->AudioMediaType, GUID_NULL) == false ? pPlaylist->pParentStream->AudioMediaType : MFAudioFormat_AAC))) return hr;

    //user data
    HEAACWAVEINFO wfInfo;

    wfInfo.wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;
    wfInfo.wfx.wBitsPerSample = 0;
    wfInfo.wfx.nChannels = 0;
    wfInfo.wfx.nSamplesPerSec = 0;
    wfInfo.wfx.nAvgBytesPerSec = 0;
    wfInfo.wfx.nBlockAlign = 1;
    wfInfo.wfx.cbSize = 12;
    wfInfo.wPayloadType = 1;
    wfInfo.wAudioProfileLevelIndication = 0;
    wfInfo.wStructType = 0;
    wfInfo.wReserved1 = 0;
    wfInfo.dwReserved2 = 0;

    /*
    wfInfo.wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;
    wfInfo.wfx.wBitsPerSample = 16;
    wfInfo.wfx.nChannels = 2;
    wfInfo.wfx.nSamplesPerSec = 0;
    wfInfo.wfx.nAvgBytesPerSec = 0;
    wfInfo.wfx.nBlockAlign = 44100;
    wfInfo.wfx.cbSize = 12;
    wfInfo.wPayloadType = 1;
    wfInfo.wAudioProfileLevelIndication = 0;
    wfInfo.wStructType = 0;
    wfInfo.wReserved1 = 0;
    wfInfo.dwReserved2 = 0;
    */

    if (FAILED(hr = cpMediaType->SetBlob(MF_MT_USER_DATA, reinterpret_cast<const UINT8*>(&(wfInfo.wPayloadType)), 12))) return hr;
   
  }
  //create stream descriptor - set stream index as 0
  if (FAILED(hr = MFCreateStreamDescriptor(id, 1, cpMediaType.GetAddressOf(), &cpStreamDescriptor))) return hr;
  //set current media type
  if (FAILED(hr = cpStreamDescriptor->GetMediaTypeHandler(&cpMediaTypeHandler))) return hr;
  return cpMediaTypeHandler->SetCurrentMediaType(cpMediaType.Get());
}

