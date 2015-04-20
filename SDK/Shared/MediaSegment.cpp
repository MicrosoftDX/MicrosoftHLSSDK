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
#include <numeric>
#include <wrl.h>
#include <string>
#include <algorithm>
#include <ppltasks.h>

#include "Timestamp.h"
#include "AESCrypto.h"
#include "VariableRate.h"
#include "HLSMediaSource.h"
#include "HLSController.h"
#include "HLSPlaylist.h"
#include "Playlist.h"
#include "Cookie.h"
#include "MediaSegment.h" 
#include "ContentDownloader.h"
#include "ContentDownloadRegistry.h"
#include "AVCParser.h"
#include "Cookie.h"
#include "FileLogger.h" 
#include "EncryptionKey.h"

using namespace Microsoft::WRL;
using namespace Windows::Security::Cryptography::Core;
using namespace Microsoft::HLSClient::Private;




unsigned long long MediaSegment::TSDiscontinousRelativeToAbsolute(unsigned long long tsrel, shared_ptr<MediaSegment> prevSegment)
{
  if (pParentPlaylist->Segments.front()->GetSequenceNumber() == SequenceNumber) //first segment
    return tsrel + Timeline.front()->ValueInTicks;
  else
    return tsrel - ((prevSegment == nullptr ? pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE) : prevSegment)->CumulativeDuration) + Timeline.front()->ValueInTicks;
}

unsigned long long MediaSegment::TSAbsoluteToDiscontinousAbsolute(unsigned long long tsabs)
{
  return TSRelativeToAbsolute(TSAbsoluteToDiscontinousRelative(tsabs, pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE)))->ValueInTicks;

}

unsigned long long MediaSegment::TSDiscontinousAbsoluteToAbsolute(unsigned long long tsabs)
{
  return TSDiscontinousRelativeToAbsolute(TSAbsoluteToRelative(tsabs)->ValueInTicks, pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE));
}

unsigned long long MediaSegment::TSAbsoluteToDiscontinousRelative(unsigned long long tsabs, shared_ptr<MediaSegment> prevSegment)
{

  if (pParentPlaylist->Segments.front()->GetSequenceNumber() == SequenceNumber) //first segment
    return (tsabs - Timeline.front()->ValueInTicks);
  else
    return (tsabs - Timeline.front()->ValueInTicks) + (prevSegment == nullptr ? pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE) : prevSegment)->CumulativeDuration;
}

MediaSegment::~MediaSegment()
{
  CCSamples.clear();
  UnreadQueues.clear();
  ReadQueues.clear();
  MediaTypePIDMap.clear();
  Timeline.clear();
  if (buffer != nullptr)
    buffer.reset(nullptr);
  spDownloadRegistry->CancelAll();
}

void MediaSegment::CancelDownloads(bool WaitForRunningTasks)
{
  spDownloadRegistry->CancelAll(WaitForRunningTasks);
}
void MediaSegment::Scavenge(bool Force)
{
  if (!Force && !CanScavenge()) return;

  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  ResetFailedCloaking();
  if (GetCurrentState() == INMEMORYCACHE)
  {
    LOG("Scavenging segment " << SequenceNumber << "," << MediaUri);
    CCSamples.clear();
    UnreadQueues.clear();
    ReadQueues.clear();
    //keep the timeline around for sliding window playlists so that we can update the sliding window - the memory will be reclaimed when the segment gets dropped 
    if ((pParentPlaylist->IsLive && pParentPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT) || pParentPlaylist->IsLive == false)
      Timeline.clear();
    else
    {
      if (Timeline.size() > 2)
      {
        auto tsstart = Timeline.front();
        auto tsend = Timeline.back();
        Timeline.clear();
        Timeline.push_back(tsstart);
        Timeline.push_back(tsend);
      }
     
    }
    MetadataStreams.clear();
    backbuffer.clear();
    if (buffer != nullptr)
      buffer.reset(nullptr);
    SetCloaking(nullptr);
    SetCurrentState(LENGTHONLY);
  }
}

unsigned int MediaSegment::SampleReadCount()
{
  if (GetCurrentState() != INMEMORYCACHE)
    return 0;
  else
  {
    auto ret = (unsigned int) std::accumulate(ReadQueues.begin(), ReadQueues.end(), (size_t) 0, [](size_t ret, std::pair<unsigned int, std::deque<std::shared_ptr<SampleData>>> q)
    {
      return ret + q.second.size();
    });
    return ret;
  }
}

///<summary>MediaSegment constructor</summary>
///<param name='tagWithAttributes'>The EXTINF tag string with attributes</param>
///<param name='mediauri'>The URL to the segment resource</param>
///<param name='parentplaylist'>The parent playlist (variant - not master)</param>
///<param name='byterangeinfo'>The applicable EXT-X-BYTERANGE tag data in case the segment is a byte range</param>
MediaSegment::MediaSegment(std::wstring& tagWithAttributes, Playlist *parentplaylist)
  : Duration(0)
  , pParentPlaylist(parentplaylist)
  , LengthInBytes(0)
  , IsHttpByteRange(false)
  , ByteRangeOffset(0)
  , State(MediaSegmentState::UNAVAILABLE),
  EncKey(nullptr),
  StartPTSNormalized(nullptr),
  EndPTSNormalized(nullptr),
  chainAssociationCount(0),
  CumulativeDuration(0),
  spCloaking(nullptr),
  Discontinous(false),
  StartsDiscontinuity(false),
  IsTransportStream(true),
  ProgramDateTime(nullptr),
  spPIDFilter(nullptr),
  buffer(nullptr)
{
  //set up the crit sec for later


  spDownloadRegistry = make_shared<ContentDownloadRegistry>();
  //if supplied segment URI is not absolute, merge with base URI on the parent playlist to make absolute


  auto attriblist = Helpers::ReadAttributeList(tagWithAttributes);
  double durTemp = 0;
  //get the segment duration
  Helpers::ReadAttributeValueFromPosition(attriblist, 0, durTemp);
  //convert to ticks (100 ns)
  Duration = (unsigned long long)std::ceill(durTemp * 10000000);


}

void MediaSegment::SetUri(std::wstring& mediauri)
{
  //if supplied segment URI is not absolute, merge with base URI on the parent playlist to make absolute
  if (Helpers::IsAbsoluteUri(mediauri))
    MediaUri = mediauri;
  else
    MediaUri = Helpers::JoinUri(this->pParentPlaylist->BaseUri, mediauri);
}
void MediaSegment::SetByteRangeInfo(const std::wstring& byterangeinfo)
{
  //if this is a byte range
  IsHttpByteRange = (byterangeinfo.empty() == false);
  if (IsHttpByteRange)
  {
    //get the length of the segment in bytes
    Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(byterangeinfo), 0, LengthInBytes, '@');
    //if we got the segment length, set state to indicate so
    if (LengthInBytes > 0)
      this->State = MediaSegmentState::LENGTHONLY;
    //if it has a byte range offset, extract it
    bool HasOffset = Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(byterangeinfo), 1, ByteRangeOffset, '@');
    //if there is no offset provided
    if (HasOffset == false && this->pParentPlaylist->Segments.size() > 0)
      //set offset to be the sum of the byte length and offset from the previous segment
      ByteRangeOffset = this->pParentPlaylist->Segments.back()->ByteRangeOffset + this->pParentPlaylist->Segments.back()->LengthInBytes;
  }
}
///<summary>Sets the current state on a media segment</summary>
///<param name='state'>The new state</param>
void MediaSegment::SetCurrentState(MediaSegmentState state)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  State = state;
}

unsigned long long MediaSegment::GetApproximateFrameDistance(ContentType type, unsigned short tgtPID)
{
  //check for some extreme conditions
  auto totsamples = UnreadQueues[tgtPID].size() + (ReadQueues.find(tgtPID) != ReadQueues.end() ? ReadQueues[tgtPID].size() : 0);
  if (totsamples < 3) //too small - try to use previous
  {
    if (type == VIDEO && pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance != 0)
      return pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance;
    else if (type == AUDIO && pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0)
      return pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance;
  }
  return (unsigned long long) (Duration / (totsamples));
}

HRESULT MediaSegment::AttemptBitrateShiftOnStreamThinning(
  CHLSMediaSource *ms,
  DefaultContentDownloader^ downloader,
  task_completion_event<HRESULT> tceSegmentDownloadCompleted)
{

  //Is this a variant scenario ? If not - this is moot
  if (ms->spRootPlaylist->IsVariant == false) return E_FAIL;
  if (ms->spRootPlaylist->ActiveVariant->Bandwidth == (*(ms->spRootPlaylist->Variants.begin())).first)
    return E_FAIL;//already at lowest
  shared_ptr<MediaSegment> targetseg = nullptr;

  std::lock_guard<std::recursive_mutex> lock(LockSegment);

  auto lastsuggestedbr = ms->spHeuristicsManager->GetLastSuggestedBandwidth();
  if (lastsuggestedbr == 0) return E_FAIL;


  //logistics:
  //- The higher the playback rate, the lower we need the bitrate to be - since we need our download performance to keep up with playback
  // - But our current bitrate is not necessarily a tru representative of available bandwidth - so we need to weight this with bandwidth usage proportion i.e. the lower the current usage, the more room we have left in the pipe
  //formula = (current bitrate / playback rate factor) times actual bandwidth usage proportion(current avail. bandwidth / current used bandwidth)
  auto targetbr = ms->GetCurrentPlaybackRate()->Rate == 0.0 ? ms->spRootPlaylist->Variants.begin()->first :
    ms->spHeuristicsManager->FindClosestBitrate(
    (unsigned int) (ms->spRootPlaylist->ActiveVariant->Bandwidth / abs(ms->GetCurrentPlaybackRate()->Rate)) *
    (lastsuggestedbr / ms->spRootPlaylist->ActiveVariant->Bandwidth), true);

  if (targetbr >= ms->spRootPlaylist->ActiveVariant->Bandwidth) //if result is greater than current
    targetbr = (*(ms->spRootPlaylist->Variants.find(ms->spRootPlaylist->ActiveVariant->Bandwidth)--)).first; //we go down at least one anyways

  auto curitr = ms->spRootPlaylist->Variants.find(targetbr);
  if (IsEqualGUID(curitr->second->VideoMediaType, GUID_NULL))  //skip audio only bitrate
  {
    if (curitr == ms->spRootPlaylist->Variants.end()) //no video bitrates
      return E_FAIL;
    else
      ++curitr;
  }
  if (curitr->second->spPlaylist == nullptr)
    curitr->second->DownloadPlaylistAsync().wait();

  if (curitr->second->spPlaylist == nullptr)
    return E_FAIL;
  targetseg = curitr->second->spPlaylist->GetSegment(this->SequenceNumber);

  //restart download from redirected URL
  std::vector<shared_ptr<Cookie>> cookies;
  std::map<std::wstring, std::wstring> headers;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
  bool MeasureDownload = (pParentPlaylist->pParentStream != nullptr /*&& pParentPlaylist->pParentStream->IsActive*/) || (pParentPlaylist->pParentRendition != nullptr && pParentPlaylist->pParentRendition->Type == Rendition::TYPEVIDEO);
  wstring url = targetseg->MediaUri;
  if (targetseg->IsHttpByteRange)
  {
    wostringstream byterange;
    byterange << "bytes=" << targetseg->ByteRangeOffset << "-" << targetseg->ByteRangeOffset + targetseg->LengthInBytes - 1;
    //set HTTP Range header
    headers.insert(std::pair<wstring, wstring>(L"Range", byterange.str()));
  }


  ms->cpController->RaisePrepareResourceRequest(ResourceType::SEGMENT, url, cookies, headers, &external);
  downloader->Initialize(ref new Platform::String(url.data()));


  if (external == nullptr)
    downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr ? pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr,
    L"GET",
    cookies,
    headers,
    pParentPlaylist->AllowCache, L"", L"", pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->IsActive : false);
  else
    downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr && pParentPlaylist->pParentStream != nullptr && pParentPlaylist->pParentStream->IsActive ?
    pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr, external);


  pParentPlaylist->spDownloadRegistry->Register(downloader);


  this->SetCloaking(targetseg);
  //targetseg->SetCurrentState(DOWNLOADING);
  this->SetCurrentState(DOWNLOADING);
  LOG("Attempting cloaking(for Thinning) " << pParentPlaylist->pParentStream->Bandwidth << "(" << SequenceNumber << ") with " << targetseg->pParentPlaylist->pParentStream->Bandwidth << "(" << targetseg->GetSequenceNumber() << ")");


  downloader->DownloadAsync();
  return S_OK;
}

HRESULT MediaSegment::AttemptBitrateShiftOnDownloadFailure(
  CHLSMediaSource *ms,
  DefaultContentDownloader^ downloader,
  task_completion_event<HRESULT> tceSegmentDownloadCompleted)
{
  //Is this a variant scenario ? If not - this is moot
  if (ms->spRootPlaylist->IsVariant == false) return E_FAIL;
  shared_ptr<MediaSegment> targetseg = nullptr;


  std::lock_guard<std::recursive_mutex> lock(LockSegment);

  //let's look for a matching segment in playlists that are at bitrates lower than the current one - and that have not FAILED already
  for (auto itr = ms->spRootPlaylist->Variants.rbegin(); itr != ms->spRootPlaylist->Variants.rend(); ++itr)
  {
    if (itr->first >= this->pParentPlaylist->pParentStream->Bandwidth) continue;
    if (IsEqualGUID(itr->second->VideoMediaType, GUID_NULL)) continue; //audio only - we skip
    if (ms->spRootPlaylist->IsLive || itr->second->spPlaylist == nullptr)
    {
      if (FAILED(itr->second->DownloadPlaylistAsync().get()))
        continue;
      if (ms->spRootPlaylist->IsLive)
        itr->second->spPlaylist->MergeLivePlaylistChild();
    }

    targetseg = itr->second->spPlaylist->GetSegment(this->SequenceNumber);
    if (targetseg == nullptr || this->HasCloakingFailedAt(targetseg->pParentPlaylist->pParentStream->Bandwidth))
      continue;
    else
      break;
  }

  if (targetseg == nullptr) //did not find any appropriate segment at lower bitrates - try shifting up one bitrate
  {
    auto nextHighest = std::find_if(ms->spRootPlaylist->Variants.begin(),
      ms->spRootPlaylist->Variants.end(),
      [this](pair<unsigned int, shared_ptr<StreamInfo>> vs) { return vs.first > this->pParentPlaylist->pParentStream->Bandwidth; });

    if (nextHighest != ms->spRootPlaylist->Variants.end())
    {
      HRESULT hr = S_OK;
      if (ms->spRootPlaylist->IsLive || (nextHighest)->second->spPlaylist == nullptr)
      {
        hr = (nextHighest)->second->DownloadPlaylistAsync().get();
        if (ms->spRootPlaylist->IsLive && SUCCEEDED(hr))
          (nextHighest)->second->spPlaylist->MergeLivePlaylistChild();
      }
      if (SUCCEEDED(hr))
        targetseg = (nextHighest)->second->spPlaylist->GetSegment(this->SequenceNumber);
    }
  }

  if (targetseg == nullptr || this->HasCloakingFailedAt(targetseg->pParentPlaylist->pParentStream->Bandwidth))
  {

    SetCloaking(nullptr);
    return E_FAIL;
  }

  spDownloadRegistry->CancelAll();
  // pParentPlaylist->spDownloadRegistry->Unregister(downloader);


  //restart download from redirected URL
  std::vector<shared_ptr<Cookie>> cookies;
  std::map<std::wstring, std::wstring> headers;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
  bool MeasureDownload = (pParentPlaylist->pParentStream != nullptr /*&& pParentPlaylist->pParentStream->IsActive*/) ||
    (pParentPlaylist->pParentRendition != nullptr && pParentPlaylist->pParentRendition->Type == Rendition::TYPEVIDEO);
  wstring url = targetseg->MediaUri;
  if (targetseg->IsHttpByteRange)
  {
    wostringstream byterange;
    byterange << "bytes=" << targetseg->ByteRangeOffset << "-" << targetseg->ByteRangeOffset + targetseg->LengthInBytes - 1;
    //set HTTP Range header
    headers.insert(std::pair<wstring, wstring>(L"Range", byterange.str()));
  }


  ms->cpController->RaisePrepareResourceRequest(ResourceType::SEGMENT, url, cookies, headers, &external);
  downloader->Initialize(ref new Platform::String(url.data()));


  if (external == nullptr)
    downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr ? pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr,
    L"GET",
    cookies,
    headers,
    pParentPlaylist->AllowCache, L"", L"", pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->IsActive : false);
  else
    downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr && pParentPlaylist->pParentStream != nullptr  && pParentPlaylist->pParentStream->IsActive ?
    pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr, external);


  pParentPlaylist->spDownloadRegistry->Register(downloader);

  this->SetCloaking(targetseg);
  this->SetCurrentState(DOWNLOADING);
  LOG("Attempting cloaking " << pParentPlaylist->pParentStream->Bandwidth << "(" << SequenceNumber << ") with " << targetseg->pParentPlaylist->pParentStream->Bandwidth << "(" << targetseg->GetSequenceNumber() << ")");

  downloader->DownloadAsync();
  return S_OK;
}



///<summary>Downloads and parses transport stream segment</summary>
task<HRESULT> MediaSegment::DownloadSegmentDataAsync(task_completion_event<HRESULT> tceSegmentDownloadCompleted)
{
  CHLSMediaSource * ms = pParentPlaylist->cpMediaSource;
  if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tceSegmentDownloadCompleted.set(E_FAIL);
    return task<HRESULT>(tceSegmentDownloadCompleted);
  }



  if (buffer != nullptr)
  {
    std::lock_guard<std::recursive_mutex> lock(LockSegment);
    UnreadQueues.clear();
    ReadQueues.clear();
    Timeline.clear();
    MediaTypePIDMap.clear();
    buffer.reset(nullptr);
    LengthInBytes = 0;
  }
  //set the state
  SetCurrentState(MediaSegmentState::DOWNLOADING);

  this->ResetFailedCloaking();

  DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();

  downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
    [this, tceSegmentDownloadCompleted, ms](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);


    if ((ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED))
    {
      tceSegmentDownloadCompleted.set(E_FAIL);
    }
    else
    {
      if (args->Content != nullptr && args->IsSuccessStatusCode)
      {
        shared_ptr<SegmentTSData> tsdata = make_shared<SegmentTSData>();
        backbuffer.emplace(static_cast<DefaultContentDownloader^>(downloader)->DownloaderID, tsdata);

      /*  BYTE * buff = nullptr;
        LengthInBytes = DefaultContentDownloader::BufferToBlob(args->Content, &buff);
        tsdata->buffer.reset(buff);*/

        LengthInBytes = DefaultContentDownloader::BufferToBlob(args->Content, tsdata->buffer);


        if (LengthInBytes == 0)
          tceSegmentDownloadCompleted.set(E_FAIL);
        else
          this->OnSegmentDownloadCompleted(ms, downloader, args, tceSegmentDownloadCompleted);
      }
      else
        tceSegmentDownloadCompleted.set(E_FAIL);
    }

    //   pParentPlaylist->spDownloadRegistry->Unregister(downloader);
  });

  downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
    [this, ms, tceSegmentDownloadCompleted](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
  {

    spDownloadRegistry->CancelAll();

    if (args->StatusCode == Windows::Web::Http::HttpStatusCode::Ok)//just a cancellation
    {
      this->SetCurrentState(MediaSegmentState::UNAVAILABLE);
      tceSegmentDownloadCompleted.set(E_FAIL);
      return;
    }


    if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
    {
      tceSegmentDownloadCompleted.set(E_FAIL);
      return;
    }


    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);

    if ((ms->GetCurrentState() != MSS_ERROR && ms->GetCurrentState() != MSS_UNINITIALIZED) &&
      ms->spRootPlaylist->IsVariant && this->pParentPlaylist->pParentRendition == nullptr) //not an alternate rendition playlist
    {
      this->SetCurrentState(DOWNLOADING);
      auto Cloaking = this->GetCloaking();
      if (Cloaking != nullptr)
      {
        Cloaking->SetCurrentState(MediaSegmentState::UNAVAILABLE);
        this->AddFailedCloaking(Cloaking->pParentPlaylist->pParentStream->Bandwidth);
      }
      else
        this->AddFailedCloaking(pParentPlaylist->pParentStream->Bandwidth);

      if (FAILED(AttemptBitrateShiftOnDownloadFailure(ms, downloader, tceSegmentDownloadCompleted)))
      {
        spDownloadRegistry->CancelAll();
        //    pParentPlaylist->spDownloadRegistry->Unregister(downloader);
        this->SetCurrentState(MediaSegmentState::UNAVAILABLE);
        tceSegmentDownloadCompleted.set(E_FAIL);
      }

    }
    else
    {
      this->SetCurrentState(MediaSegmentState::UNAVAILABLE);
      tceSegmentDownloadCompleted.set(E_FAIL);
    }

    return;
  });


  HRESULT hr = E_FAIL;

  if ((pParentPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned && (pParentPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate > 1.0 ||
    pParentPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate < 0.0) && Configuration::GetCurrent()->AutoAdjustTrickPlayBitrate) ||
    (pParentPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate == 0.0 && Configuration::GetCurrent()->AutoAdjustScrubbingBitrate))
    hr = AttemptBitrateShiftOnStreamThinning(ms, downloader, tceSegmentDownloadCompleted);

  if (FAILED(hr))
  {
    std::vector<shared_ptr<Cookie>> cookies;
    std::map<std::wstring, std::wstring> headers;
    Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
    bool MeasureDownload = (pParentPlaylist->pParentStream != nullptr /*&& pParentPlaylist->pParentStream->IsActive*/) || (pParentPlaylist->pParentRendition != nullptr && pParentPlaylist->pParentRendition->Type == Rendition::TYPEVIDEO);
    wstring url = MediaUri;
    if (IsHttpByteRange)
    {
      wostringstream byterange;
      byterange << "bytes=" << ByteRangeOffset << "-" << ByteRangeOffset + LengthInBytes - 1;
      //set HTTP Range header
      headers.insert(std::pair<wstring, wstring>(L"Range", byterange.str()));
    }


    ms->cpController->RaisePrepareResourceRequest(ResourceType::SEGMENT, url, cookies, headers, &external);
    downloader->Initialize(ref new Platform::String(url.data()));


    if (external == nullptr)
      downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr ? pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr,
      L"GET",
      cookies,
      headers,
      pParentPlaylist->AllowCache, L"", L"", pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->IsActive : false);
    else
      downloader->SetParameters(MeasureDownload && pParentPlaylist != nullptr && pParentPlaylist->pParentStream != nullptr   && pParentPlaylist->pParentStream->IsActive ?
      pParentPlaylist->cpMediaSource->spHeuristicsManager.get() : nullptr, external);


    pParentPlaylist->spDownloadRegistry->Register(downloader);

    downloader->DownloadAsync();
  }
  //attach error handler


  //LOGIF(pParentPlaylist != nullptr && pParentPlaylist->pParentStream != nullptr, "MediaSegment::DownloadSegmentDataAsync() - Seq=" << SequenceNumber << ",speed=" << pParentPlaylist->pParentStream->Bandwidth);
  //LOGIF(pParentPlaylist != nullptr && pParentPlaylist->pParentRendition != nullptr, "MediaSegment::DownloadSegmentDataAsync() - Seq=" << SequenceNumber << " for alt rend group " << pParentPlaylist->pParentRendition->GroupID << " (" << pParentPlaylist->pParentRendition->Name << ")");

  //return a task that can be waited on
  return task<HRESULT>(tceSegmentDownloadCompleted);
}

HRESULT MediaSegment::OnSegmentDownloadCompleted(
  CHLSMediaSource* ms,
  Microsoft::HLSClient::IHLSContentDownloader^ downloader,
  Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args,
  task_completion_event<HRESULT> tceSegmentDownloadCompleted)
{
  DefaultContentDownloader^ pdownloader = static_cast<DefaultContentDownloader^>(downloader);
  auto downloaderid = pdownloader->DownloaderID;

  try
  {
	/*if(pdownloader != nullptr)
		CHKTASK(pdownloader->CancellationToken());*/
    //in case of a redirect
    MediaUri = args->ContentUri->AbsoluteUri->Data();

    if (backbuffer.find(downloaderid) == backbuffer.end())
      throw E_FAIL;
    auto tsdata = backbuffer[downloaderid];

    auto encKey = this->GetCloaking() != nullptr ? this->GetCloaking()->EncKey : this->EncKey;

    //decrypt - if needed
    if (encKey != nullptr && encKey->Method != NOENCRYPTION && LengthInBytes > 0)
    {
      HRESULT hr = S_OK;

      try
      {
        if (encKey->cpCryptoKey == nullptr)//we do not have a crypto key yet
        {
          //possible scenarios:
          //a) we have a cached key that has a matching URI and a matching IV - set the key to the cached key
          //b)we have a cached key that has a matching URI but the IV does not match - copy the key data from the cached key but use the IV in the new key
          //c) we do not have a cached key or we do have one with a different URI - download key

          if (pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey != nullptr && pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey->cpCryptoKey != nullptr)
          {
            if (encKey->IsEqual(pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey))//case a)
              encKey = pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey;
            else if (encKey->IsEqualKeyOnly(pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey)) //case b)
              encKey->cpCryptoKey = pParentPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey->cpCryptoKey;
            else
              hr = encKey->DownloadKeyAsync().get();//wait() or get() ? 
          }
          else
          {
            hr = encKey->DownloadKeyAsync().get();//wait() or get() ? 
          }

        }

        if (FAILED(hr)) //serious
        {
          this->SetCurrentState(MediaSegmentState::UNAVAILABLE);
          tceSegmentDownloadCompleted.set(E_FAIL);
          this->pParentPlaylist->cpMediaSource->NotifyError(hr);
          throw E_FAIL;
        }

        //if we have a valid crypto key
        if (encKey->cpCryptoKey != nullptr)
        {
          //if we have valid IV
          if (!encKey->InitializationVector.empty())
          {
            auto decdata = AESCrypto::GetCurrent()->Decrypt(encKey->cpCryptoKey, tsdata->buffer.get(), LengthInBytes, encKey->InitializationVector);
            if (decdata != nullptr)
            {
              tsdata->buffer.reset(new BYTE[decdata->Length]);
              LengthInBytes = decdata->Length;
              memcpy_s(tsdata->buffer.get(), decdata->Length, decdata->Data, decdata->Length);

            }
            else
              throw E_FAIL;
          }
          //no IV in the playlist
          else
          {
            //convert sequence number to IV (per HLS spec)
            auto iv = encKey->ToInitializationVector(this->SequenceNumber);
            //decrypt and copy to buffer  
            auto decdata = AESCrypto::GetCurrent()->Decrypt(encKey->cpCryptoKey, tsdata->buffer.get(), LengthInBytes, &(*(iv->begin())), static_cast<unsigned int>(iv->size()));
            if (decdata != nullptr)
            {
              tsdata->buffer.reset(new BYTE[decdata->Length]);
              LengthInBytes = decdata->Length;
              memcpy_s(tsdata->buffer.get(), decdata->Length, decdata->Data, decdata->Length);

            }
            else
              throw E_FAIL;
          }
        }
        else
          throw E_FAIL;
      }
      catch (task_canceled)
      {
        if (GetCurrentState() != INMEMORYCACHE)
          SetCurrentState(MediaSegmentState::UNAVAILABLE);
        throw;
      }
      catch (...)
      {
        if (GetCurrentState() != INMEMORYCACHE)
          SetCurrentState(MediaSegmentState::UNAVAILABLE);
        LOG("Decryption failed: Segment(" << pdownloader->DownloaderID << ") using key(" << encKey->KeyUri << ")");
        throw E_FAIL;
      }
    }


	/*if (pdownloader != nullptr)
		CHKTASK(pdownloader->CancellationToken());*/

    if (tsdata->buffer == nullptr)
    {
      LengthInBytes = 0;
      if (GetCurrentState() != INMEMORYCACHE)
        SetCurrentState(MediaSegmentState::UNAVAILABLE);
      LOG("Decryption failed: Segment(" << pdownloader->DownloaderID << ") using key(" << encKey->KeyUri << ")");
      throw E_FAIL;
    }
    else
    {
      BINLOG(SequenceNumber, tsdata->buffer.get(), LengthInBytes);
      //if this is a valid MPEG2 Transport Stream 
      if ((IsTransportStream = TransportStreamParser::IsTransportStream(tsdata->buffer.get())))
      {
         
        TransportStreamParser tsparser;

        //parse TS 

        tsparser.Parse(tsdata->buffer.get(), LengthInBytes, tsdata->MediaTypePIDMap, GetPIDFilter(),
          tsdata->MetadataStreams, tsdata->UnreadQueues, tsdata->Timeline, tsdata->CCSamples);

        //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed TS(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
      }
      //treat as elementary audio stream
      else if (MediaSegment::ExtractInitialTimestampFromID3PRIV(tsdata->buffer.get(), LengthInBytes) != nullptr)
      {

        //if we cannot build audio samples now - save it for later
        BuildAudioElementaryStreamSamples(tsdata);
        if (pParentPlaylist->IsLive && nullptr != pParentPlaylist->cpMediaSource->cpVideoStream && pParentPlaylist->cpMediaSource->cpVideoStream->Selected())
          this->Discontinous = true;
        //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed Audio(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
      }
      else
      {
        LOG("ERROR: DownloadSegmentDataAsync::ResponseReceived() - Unknown content type(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
        tceSegmentDownloadCompleted.set(E_FAIL);
        throw E_FAIL;
      }
      //set up boundary timestamps

      {
        std::lock_guard<recursive_mutex> lock(LockSegment);

  

        this->MediaTypePIDMap = std::move(tsdata->MediaTypePIDMap);
        this->Timeline = std::move(tsdata->Timeline);
        this->CCSamples = std::move(tsdata->CCSamples);
        this->UnreadQueues = std::move(tsdata->UnreadQueues);
        this->MetadataStreams = std::move(tsdata->MetadataStreams);
        this->ReadQueues.clear();

        tsdata->buffer.swap(this->buffer);
        tsdata.reset();
        this->backbuffer.erase(downloaderid);
      }
      //set state 
      SetCurrentState(MediaSegmentState::INMEMORYCACHE);
      SetPTSBoundaries();

      tceSegmentDownloadCompleted.set(S_OK);

      if (ms->cpController != nullptr && ms->cpController->GetPlaylist() != nullptr)
      {
        auto SeqNum = this->GetSequenceNumber();
        auto pParent = this->pParentPlaylist;
        ms->protectionRegistry.Register(task<HRESULT>([ms, pParent, SeqNum, this]()
        {
          ms->cpController->GetPlaylist()->RaiseSegmentDataLoaded(pParent, SeqNum);

          return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
      }
    }
  }
  catch (task_canceled tc)
  {
    tceSegmentDownloadCompleted.set(E_FAIL);
  }
  catch (...)
  {
    tceSegmentDownloadCompleted.set(E_FAIL);

  }

  return S_OK;
}



std::map<ContentType, unsigned short> MediaSegment::GetPIDFilter()
{
  if (this->spPIDFilter == nullptr) //has not been set
  {
    return pParentPlaylist->GetPIDFilter();
  }
  else
    return *spPIDFilter;
}

void MediaSegment::ApplyPIDFilter(std::map<ContentType, unsigned short> filter, bool Force)
{

  //this method is called to applly any PID filters tht are inherited from the palylist but was not applied in time and the data has already been parsed
  if (this->GetCurrentState() != MediaSegmentState::INMEMORYCACHE || buffer == nullptr) //not in memory
    return;
  if (spPIDFilter != nullptr) //we have our own filter 
    return;
  if (pParentPlaylist->IsSegmentPlayingBack(GetSequenceNumber())) //playing
    return;

  for (auto itm : filter)
  {
    if (HasMediaType(itm.first) && MediaTypePIDMap[itm.first] != itm.second) //does not match current PID map
    {
      if (!Force)
        Force = true;
      break;
    }
  }
  if (Force)
  {

    if ((IsTransportStream = TransportStreamParser::IsTransportStream(buffer.get())))
    {
      TransportStreamParser tsparser;
      MediaTypePIDMap.clear();
      MetadataStreams.clear();
      UnreadQueues.clear();
      Timeline.clear();
      CCSamples.clear();
      //parse TS
      tsparser.Parse(buffer.get(), LengthInBytes, MediaTypePIDMap, filter,
        MetadataStreams, UnreadQueues, Timeline, CCSamples);


      //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed TS(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
    }
    //}
  }

}

bool MediaSegment::SetPIDFilter(shared_ptr<std::map<ContentType, unsigned short>> pidfilter)
{

  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE) //we have already parsed this
  {
    //check to see if this segment is currently being played
    if (pParentPlaylist->IsSegmentPlayingBack(GetSequenceNumber()))
      return false;

    //std::lock_guard<std::recursive_mutex> lock(pParentPlaylist->LockSegmentTracking);

    if ((IsTransportStream = TransportStreamParser::IsTransportStream(buffer.get())))
    {
      TransportStreamParser tsparser;
      MediaTypePIDMap.clear();
      MetadataStreams.clear();
      UnreadQueues.clear();
      Timeline.clear();
      CCSamples.clear();
      //parse TS
      tsparser.Parse(buffer.get(),
        LengthInBytes,
        MediaTypePIDMap,
        pidfilter == nullptr ? std::map<ContentType, unsigned short>() : *pidfilter,
        MetadataStreams,
        UnreadQueues,
        Timeline, CCSamples);


      //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed TS(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
    }
  }

  this->spPIDFilter = pidfilter;
  return true;
}

bool MediaSegment::ResetPIDFilter(ContentType forType)
{
  if (this->spPIDFilter->find(forType) == this->spPIDFilter->end()) return true;

  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE) //we have already parsed this
  {
    //check to see if this segment is currently being played
    if (pParentPlaylist->IsSegmentPlayingBack(GetSequenceNumber()))
      return false;

    this->spPIDFilter->erase(forType);
    //std::lock_guard<std::recursive_mutex> lock(pParentPlaylist->LockSegmentTracking);

    if ((IsTransportStream = TransportStreamParser::IsTransportStream(buffer.get())))
    {
      TransportStreamParser tsparser;
      MediaTypePIDMap.clear();
      MetadataStreams.clear();
      UnreadQueues.clear();
      Timeline.clear();
      CCSamples.clear();
      //parse TS
      tsparser.Parse(buffer.get(),
        LengthInBytes,
        MediaTypePIDMap,
        *(this->spPIDFilter),
        MetadataStreams,
        UnreadQueues,
        Timeline, CCSamples);


      //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed TS(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
    }
  }
  else
    this->spPIDFilter->erase(forType);

  return true;
}

bool MediaSegment::ResetPIDFilter()
{
  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE) //we have already parsed this
  {
    //check to see if this segment is currently being played
    if (pParentPlaylist->IsSegmentPlayingBack(GetSequenceNumber()))
      return false;

    this->spPIDFilter.reset();
    //std::lock_guard<std::recursive_mutex> lock(pParentPlaylist->LockSegmentTracking);

    if ((IsTransportStream = TransportStreamParser::IsTransportStream(buffer.get())))
    {
      TransportStreamParser tsparser;
      MediaTypePIDMap.clear();
      MetadataStreams.clear();
      UnreadQueues.clear();
      Timeline.clear();
      CCSamples.clear();
      //parse TS
      tsparser.Parse(buffer.get(),
        LengthInBytes,
        MediaTypePIDMap,
        std::map<ContentType, unsigned short>(),
        MetadataStreams,
        UnreadQueues,
        Timeline, CCSamples);
      //LOG("DownloadSegmentDataAsync::ResponseReceived() - Parsed TS(seq=" << SequenceNumber << ",speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ") [" << MediaUri << "]");
    }
  }
  else
    this->spPIDFilter.reset();

  return true;
}

shared_ptr<Timestamp> MediaSegment::GetFirstMinPTS()
{
  if (this->GetCurrentState() != INMEMORYCACHE) return nullptr;
  shared_ptr<Timestamp> retval = nullptr;
  for (auto mapitm : MediaTypePIDMap)
  {
    if (mapitm.first == ContentType::AUDIO || mapitm.first == ContentType::VIDEO)
    {
      auto nextsample = this->PeekNextSample(mapitm.second, MFRATE_DIRECTION::MFRATE_FORWARD);
      if (nextsample != nullptr)
      {
        retval = (retval == nullptr) ? (this->Discontinous ? nextsample->DiscontinousTS : nextsample->SamplePTS) :
          make_shared<Timestamp>(__min(retval->ValueInTicks, (this->Discontinous ? nextsample->DiscontinousTS : nextsample->SamplePTS)->ValueInTicks));
      }
    }
  }
  if (Timeline.size() > 0)
    retval = (retval == nullptr) ? *(Timeline.begin()) :
    make_shared<Timestamp>(__min(retval->ValueInTicks, (*(Timeline.begin()))->ValueInTicks));
  return retval;
}

///<summary>Sets the start and end Program timsetamps for the segment</summary>
void MediaSegment::SetPTSBoundaries()
{
  bool SlidingWindowChanged = false;
  {
    std::lock_guard<std::recursive_mutex> lock(LockSegment);


    bool SetPlaylistStartPTS = (pParentPlaylist->IsLive && pParentPlaylist->StartPTSOriginal == nullptr) ||
      (pParentPlaylist->IsLive == false && this->SequenceNumber == pParentPlaylist->Segments[0]->SequenceNumber);

    //if segment is not in memory or if the segment TS did not contain any timestamps
    if (this->GetCurrentState() != MediaSegmentState::INMEMORYCACHE || (Timeline.empty()))
    {
      //get sequence number

      //set start PTS to be the cumulative duration of previous segment, or 0 if this is the first segment
      StartPTSNormalized = std::make_shared<Timestamp>(this->SequenceNumber == pParentPlaylist->Segments[0]->SequenceNumber ? 0 : this->pParentPlaylist->GetNextSegment(this->SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE)->CumulativeDuration);
      //if this is the first segment
      if (SetPlaylistStartPTS && pParentPlaylist->IsLive == false && this->pParentPlaylist->pParentRendition == nullptr)
        //this is also the start PTS for the entire playlist
        pParentPlaylist->StartPTSOriginal = StartPTSNormalized;
    }
    //segment data is in memory
    else
    {
      if (pParentPlaylist->IsLive && pParentPlaylist->SlidingWindowStart == nullptr)
      {
        SlidingWindowChanged = true;
        if (pParentPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT)
        {
          pParentPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(Timeline.front()->ValueInTicks);
        }
        else
        {
          if (this->SequenceNumber != pParentPlaylist->Segments.front()->SequenceNumber)
            pParentPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(Timeline.front()->ValueInTicks - pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE)->CumulativeDuration);
          else
            pParentPlaylist->SlidingWindowStart = std::make_shared<Timestamp>(Timeline.front()->ValueInTicks);
        }
      }

      if (pParentPlaylist->IsLive && pParentPlaylist->SlidingWindowStart != nullptr && pParentPlaylist->SlidingWindowEnd == nullptr)
      {
        SlidingWindowChanged = true;
        pParentPlaylist->SlidingWindowEnd = std::make_shared<Timestamp>(pParentPlaylist->SlidingWindowStart->ValueInTicks + pParentPlaylist->Segments.back()->CumulativeDuration);

      }


      if (SetPlaylistStartPTS)
      {

        //calculate the appropriate start PTS

        unsigned long long startPTSVal = 0;


        if (this->pParentPlaylist->pParentStream != nullptr)
        {
          if (pParentPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr && pParentPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist != nullptr)
          {
            pParentPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->StartPTSOriginal = GetFirstMinPTS();
          }
          //parent playlist start PTS is the original (denormalized) value of the first PTS on this segment
          pParentPlaylist->StartPTSOriginal = GetFirstMinPTS();
        }
        else if (this->pParentPlaylist->pParentRendition != nullptr) //alternate rendition playlist
        {
          if (pParentPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->spPlaylist != nullptr)
            //parent playlist start PTS is the original (denormalized) value of the first PTS on this segment
            pParentPlaylist->StartPTSOriginal = pParentPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->spPlaylist->StartPTSOriginal;
        }
        else
        {
          pParentPlaylist->StartPTSOriginal = GetFirstMinPTS();
        }
 
      }
      //if this is the first segment
      if ((!pParentPlaylist->IsLive && this->SequenceNumber == pParentPlaylist->Segments[0]->SequenceNumber))
      {
        //the start PTS of the segment is the normalized value
        this->StartPTSNormalized = std::make_shared<Timestamp>(0, TimestampType::PTS);
      }
      else
      {
        //store normaized start PTS
        StartPTSNormalized = pParentPlaylist->IsLive == false && pParentPlaylist->StartPTSOriginal != nullptr ? std::make_shared<Timestamp>((*(Timeline.begin()))->ValueInTicks - pParentPlaylist->StartPTSOriginal->ValueInTicks) : std::make_shared<Timestamp>((*(Timeline.begin()))->ValueInTicks);
      }

    }


    //if segment is not in memory or we did not find any timestamps in the TS or we found just one (which would be used to mark start)
    if (this->GetCurrentState() != MediaSegmentState::INMEMORYCACHE || (Timeline.empty()) || Timeline.size() == 1)
      //use cumulative duration as end PTS
      EndPTSNormalized = std::make_shared<Timestamp>(CumulativeDuration, TimestampType::PTS);
    else
    {
   
      //store the higher of cumulative duration and the last PTS
      EndPTSNormalized = std::make_shared<Timestamp>(pParentPlaylist->IsLive == false && pParentPlaylist->StartPTSOriginal != nullptr ? Timeline.back()->ValueInTicks - pParentPlaylist->StartPTSOriginal->ValueInTicks : Timeline.back()->ValueInTicks);

    }
  }



  if (SlidingWindowChanged)
    this->pParentPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([this]()
  {
    if (this->pParentPlaylist->cpMediaSource != nullptr && this->pParentPlaylist->pParentStream != nullptr && //do not raise for alternate renditions
      this->pParentPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && this->pParentPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
      this->pParentPlaylist->cpMediaSource->cpController != nullptr && this->pParentPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
      this->pParentPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSlidingWindowChanged(this->pParentPlaylist);

    return S_OK;
  }));

}

/*

void MediaSegment::UpdateSampleDiscontinuityTimestampsVOD()
{

auto prevseg = pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE);
//serial version
if (this->GetCurrentState() == INMEMORYCACHE && Discontinous && !pParentPlaylist->IsLive)
{
for (auto itm : MediaTypePIDMap)
{
if (UnreadQueues.find(itm.second) != UnreadQueues.end())
{
for (auto sd : UnreadQueues[itm.second])
{
if (sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS = make_shared<Timestamp>(TSAbsoluteToDiscontinousRelative(sd->SamplePTS->ValueInTicks,
prevseg));
}

}
}
}
for (auto itm : MetadataStreams)
{
if (UnreadQueues.find(itm) != UnreadQueues.end())
{
for (auto sd : UnreadQueues[itm])
{
if (sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS = make_shared<Timestamp>(TSAbsoluteToDiscontinousRelative(sd->SamplePTS->ValueInTicks,
prevseg));
}
}
}
}
}


}

void MediaSegment::UpdateSampleDiscontinuityTimestampsVOD(shared_ptr<MediaSegment> prevplayedseg,
ContentType type,
shared_ptr<Timestamp> StartPTSOriginal)
{

if (Discontinous && !pParentPlaylist->IsLive)
{
unsigned long long maxtsfromprevseg = 0;
if (prevplayedseg != nullptr && prevplayedseg->GetCurrentState() == INMEMORYCACHE)
{

if (HasMediaType(type) == false) return;

auto prevpid = prevplayedseg->GetPIDForMediaType(type);

shared_ptr<SampleData> lastsample =
(prevplayedseg->IsReadEOS(prevpid)) ? prevplayedseg->ReadQueues[prevpid].back() :
prevplayedseg->UnreadQueues[prevpid].back();

maxtsfromprevseg = (prevplayedseg->Discontinous && lastsample->DiscontinousTS != nullptr) ?
lastsample->DiscontinousTS->ValueInTicks : lastsample->SamplePTS->ValueInTicks;

}
else
{
auto prevseg = pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE);
maxtsfromprevseg = (StartPTSOriginal != nullptr ?
StartPTSOriginal->ValueInTicks :
pParentPlaylist->StartPTSOriginal->ValueInTicks) + (prevseg != nullptr ? prevseg->CumulativeDuration : 0);

}

auto thispid = GetPIDForMediaType(type);
for (auto itr = UnreadQueues[thispid].begin(); itr != UnreadQueues[thispid].end(); itr++)
{
auto sd = *itr;
if (itr == UnreadQueues[thispid].begin() && sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS =
make_shared<Timestamp>((type == VIDEO ? pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance :
pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance) +
maxtsfromprevseg);
}
else if (sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS =
make_shared<Timestamp>((type == VIDEO ? pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance :
pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance) +
maxtsfromprevseg + sd->SamplePTS->ValueInTicks - UnreadQueues[thispid].front()->SamplePTS->ValueInTicks);
}
}

//serial version
if (type == VIDEO)//also do the metadata
{
for (auto itm : MetadataStreams)
{
if (UnreadQueues.find(itm) != UnreadQueues.end())
{

for (auto sd : UnreadQueues[itm])
{
if (sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS = make_shared<Timestamp>(sd->SamplePTS->ValueInTicks -
UnreadQueues[thispid].front()->SamplePTS->ValueInTicks + maxtsfromprevseg);
}
}
}
}
}

}


}

void MediaSegment::UpdateSampleDiscontinuityTimestampsLive(
shared_ptr<MediaSegment> prevplayedseg,
ContentType type,
shared_ptr<Timestamp> StartPTSOriginal)
{

assert(prevplayedseg != nullptr && prevplayedseg->GetCurrentState() == INMEMORYCACHE);

auto appxframedistance = type == VIDEO ?
pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance :
pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance;

auto livecumduration = type == VIDEO ?
pParentPlaylist->cpMediaSource->spRootPlaylist->LiveVideoPlaybackCumulativeDuration :
pParentPlaylist->cpMediaSource->spRootPlaylist->LiveAudioPlaybackCumulativeDuration;

LOG("UpdateSampleDiscontinuityTimestampsLive(), Seq " << SequenceNumber);
if (Discontinous && pParentPlaylist->IsLive)
{
if (HasMediaType(type) == false) return;
unsigned long long maxtsfromprevseg = 0;

auto prevpid = prevplayedseg->GetPIDForMediaType(type);
auto sd = prevplayedseg->IsReadEOS(prevpid) ? prevplayedseg->ReadQueues[prevpid].back() : prevplayedseg->UnreadQueues[prevpid].back();
// prevplayedseg->ReadQueues.find(prevpid) != prevplayedseg->ReadQueues.end() ? prevplayedseg->ReadQueues[prevpid].back() : prevplayedseg->UnreadQueues[prevpid].back();
maxtsfromprevseg = prevplayedseg->Discontinous && sd->DiscontinousTS != nullptr ? sd->DiscontinousTS->ValueInTicks : sd->SamplePTS->ValueInTicks;
LOG("UpdateSampleDiscontinuityTimestampsLive() - Using previous segment, Seq " << SequenceNumber);


auto thispid = GetPIDForMediaType(type);
for (auto itr = UnreadQueues[thispid].begin(); itr != UnreadQueues[thispid].end(); itr++)
{
auto sd = *itr;
if (itr == UnreadQueues[thispid].begin() && sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS =
make_shared<Timestamp>(appxframedistance + maxtsfromprevseg);
}
else if (sd->DiscontinousTS == nullptr)
{
auto prev = *(itr - 1);
auto diff = ((long long) sd->SamplePTS->ValueInTicks) - ((long long) prev->SamplePTS->ValueInTicks);
sd->DiscontinousTS =
make_shared<Timestamp>(prev->DiscontinousTS->ValueInTicks + diff);

}
}


//serial version
if (type == VIDEO)//also do the metadata
{
for (auto itm : MetadataStreams)
{
if (UnreadQueues.find(itm) != UnreadQueues.end())
{

for (auto sd : UnreadQueues[itm])
{
if (sd->DiscontinousTS == nullptr)
{
sd->DiscontinousTS = make_shared<Timestamp>(sd->SamplePTS->ValueInTicks - UnreadQueues[thispid].front()->SamplePTS->ValueInTicks + maxtsfromprevseg);
}
}
}
}
}

LOG("UpdateSampleDiscontinuityTimestampsLive() - Done for Seq " << SequenceNumber);
}
}
*/

shared_ptr<SampleData> MediaSegment::GetLastSample(ContentType type, bool IgnoreUnread)
{
  if (HasMediaType(type))
  {
    auto pid = GetPIDForMediaType(type);
    if (IgnoreUnread) //ignore any unread samples and return the last played
    {
      return ReadQueues.find(pid) != ReadQueues.end() ?
        ReadQueues[pid].back() :
        UnreadQueues[pid].back();
    }
    else //return the last sample
    {
      return UnreadQueues[pid].size() > 0 ?
        UnreadQueues[pid].back() :
        (ReadQueues.find(pid) != ReadQueues.end() ? ReadQueues[pid].back() : nullptr);
    }
  }
  else
    return nullptr;
}

shared_ptr<SampleData> MediaSegment::GetLastSample(bool IgnoreUnread)
{
  shared_ptr<SampleData> retval = nullptr;
  for (auto itm : MediaTypePIDMap)
  {
    shared_ptr<Timestamp> ptssaved = (retval != nullptr) ? (retval->DiscontinousTS != nullptr ? retval->DiscontinousTS : retval->SamplePTS) : nullptr;

    auto sd = GetLastSample(itm.first, IgnoreUnread);

    shared_ptr<Timestamp> ptsnew = (sd != nullptr) ? (sd->DiscontinousTS != nullptr ? sd->DiscontinousTS : sd->SamplePTS) : nullptr;;

    if (ptsnew == nullptr) continue;
    else
    {
      if (ptssaved != nullptr)
      {
        if (ptssaved->ValueInTicks < ptsnew->ValueInTicks)
          retval = sd;
      }
      else
      {
        retval = sd;
      }
    }
  }
  return retval;
}


void MediaSegment::UpdateSampleDiscontinuityTimestamps(shared_ptr<MediaSegment> prevplayedseg, bool IgnoreUnreadSamples)
{

  assert(prevplayedseg != nullptr && prevplayedseg->GetCurrentState() == INMEMORYCACHE);



  shared_ptr<SampleData> lastvidsamplefromprevseg = nullptr;
  shared_ptr<SampleData> lastaudsamplefromprevseg = nullptr;


  UpdateSampleDiscontinuityTimestamps(prevplayedseg->GetLastSample(VIDEO, IgnoreUnreadSamples),
    prevplayedseg->GetLastSample(AUDIO, IgnoreUnreadSamples));


}


void MediaSegment::UpdateSampleDiscontinuityTimestamps(shared_ptr<SampleData> lastvidsamplefromprevseg, shared_ptr<SampleData> lastaudsamplefromprevseg)
{
  if (!Discontinous) return;
  if (IsReadEOS()) return;

  if (HasMediaType(AUDIO) && pParentPlaylist->cpMediaSource->cpAudioStream != nullptr && pParentPlaylist->cpMediaSource->cpAudioStream->Selected()) 
    assert(pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0); 
  
  if (HasMediaType(VIDEO) && pParentPlaylist->cpMediaSource->cpVideoStream != nullptr && pParentPlaylist->cpMediaSource->cpVideoStream->Selected()) 
    assert(pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance != 0); 


  //Logic : We look at the earliest sample in the segment. 
  //If it is a video sample - we mark its discts = discts of last video sample + appxframedist_video
  //else if it is an audio sample - we mark its discts = discts of last audio sample + appxframedist_audio
  //from there on in each sample discts = first sample samplets - this sample samplets + first sample discts
  shared_ptr<SampleData> firstaudsample = nullptr;
  shared_ptr<SampleData> firstvidsample = nullptr;
  unsigned short audpid = 0;
  unsigned short vidpid = 0;



  if (HasMediaType(AUDIO))
  {
    audpid = GetPIDForMediaType(AUDIO);
    if (UnreadQueues.find(audpid) != UnreadQueues.end() && UnreadQueues[audpid].size() > 0)
      firstaudsample = UnreadQueues[audpid].front();
  }
  if (HasMediaType(VIDEO))
  {
    vidpid = GetPIDForMediaType(VIDEO);
    if (UnreadQueues.find(vidpid) != UnreadQueues.end() && UnreadQueues[vidpid].size() > 0)
      firstvidsample = UnreadQueues[vidpid].front();
  }


  if (firstvidsample != nullptr &&
    (firstaudsample == nullptr || firstvidsample->SamplePTS->ValueInTicks <= firstaudsample->SamplePTS->ValueInTicks))
  {

    auto lastts = (lastvidsamplefromprevseg != nullptr ?
      (lastvidsamplefromprevseg->DiscontinousTS != nullptr ? lastvidsamplefromprevseg->DiscontinousTS->ValueInTicks : lastvidsamplefromprevseg->SamplePTS->ValueInTicks) :
      (lastaudsamplefromprevseg->DiscontinousTS != nullptr ? lastaudsamplefromprevseg->DiscontinousTS->ValueInTicks : lastaudsamplefromprevseg->SamplePTS->ValueInTicks));

    firstvidsample->DiscontinousTS = make_shared<Timestamp>(lastts + pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance);

    for (auto itr = UnreadQueues[vidpid].begin() + 1; itr != UnreadQueues[vidpid].end(); itr++)
    {
      auto sd = *itr;
      sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
    }

    if (HasMediaType(AUDIO) && UnreadQueues.find(audpid) != UnreadQueues.end() && UnreadQueues[audpid].size() > 0)
    {
      for (auto itr = UnreadQueues[audpid].begin(); itr != UnreadQueues[audpid].end(); itr++)
      {
        auto sd = *itr;
        sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
      }
    }
    for (auto itm : MetadataStreams)
    {
      if (UnreadQueues.find(itm) != UnreadQueues.end())
      {

        for (auto sd : UnreadQueues[itm])
        {
          if (sd->DiscontinousTS == nullptr)
          {
            sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
          }
        }
      }
    }
  }
  else if (firstaudsample != nullptr &&
    (firstvidsample == nullptr || firstaudsample->SamplePTS->ValueInTicks <= firstvidsample->SamplePTS->ValueInTicks))
  {
    auto lastts = (lastaudsamplefromprevseg != nullptr ?
      (lastaudsamplefromprevseg->DiscontinousTS != nullptr ? lastaudsamplefromprevseg->DiscontinousTS->ValueInTicks : lastaudsamplefromprevseg->SamplePTS->ValueInTicks) :
      (lastvidsamplefromprevseg->DiscontinousTS != nullptr ? lastvidsamplefromprevseg->DiscontinousTS->ValueInTicks : lastvidsamplefromprevseg->SamplePTS->ValueInTicks));

    firstaudsample->DiscontinousTS = make_shared<Timestamp>(lastts + pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance);

    for (auto itr = UnreadQueues[audpid].begin() + 1; itr != UnreadQueues[audpid].end(); itr++)
    {
      auto sd = *itr;
      sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
    }
    if (HasMediaType(VIDEO) && UnreadQueues.find(vidpid) != UnreadQueues.end() && UnreadQueues[vidpid].size() > 0)
    {
      for (auto itr = UnreadQueues[vidpid].begin(); itr != UnreadQueues[vidpid].end(); itr++)
      {
        auto sd = *itr;
        sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
      }
    }
    for (auto itm : MetadataStreams)
    {
      if (UnreadQueues.find(itm) != UnreadQueues.end())
      {

        for (auto sd : UnreadQueues[itm])
        {
          if (sd->DiscontinousTS == nullptr)
          {
            sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
          }
        }
      }
    }
  }


  LOG("UpdateSampleDiscontinuityTimestampsLive() - Done for Seq " << SequenceNumber);

}

void MediaSegment::UpdateSampleDiscontinuityTimestamps(unsigned long long lastts)
{
  if (!Discontinous) return;
  if (IsReadEOS()) return;

  assert(pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance != 0 &&
    pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0);

  //Logic : We look at the earliest sample in the segment. 
  //If it is a video sample - we mark its discts = discts of last video sample + appxframedist_video
  //else if it is an audio sample - we mark its discts = discts of last audio sample + appxframedist_audio
  //from there on in each sample discts = first sample samplets - this sample samplets + first sample discts
  shared_ptr<SampleData> firstaudsample = nullptr;
  shared_ptr<SampleData> firstvidsample = nullptr;
  unsigned short audpid = 0;
  unsigned short vidpid = 0;



  if (HasMediaType(AUDIO))
  {
    audpid = GetPIDForMediaType(AUDIO);
    if (UnreadQueues.find(audpid) != UnreadQueues.end() && UnreadQueues[audpid].size() > 0)
      firstaudsample = UnreadQueues[audpid].front();
  }
  if (HasMediaType(VIDEO))
  {
    vidpid = GetPIDForMediaType(VIDEO);
    if (UnreadQueues.find(vidpid) != UnreadQueues.end() && UnreadQueues[vidpid].size() > 0)
      firstvidsample = UnreadQueues[vidpid].front();
  }


  if (firstvidsample != nullptr &&
    (firstaudsample == nullptr || firstvidsample->SamplePTS->ValueInTicks <= firstaudsample->SamplePTS->ValueInTicks))
  {

    firstvidsample->DiscontinousTS = make_shared<Timestamp>(lastts + pParentPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance);

    for (auto itr = UnreadQueues[vidpid].begin() + 1; itr != UnreadQueues[vidpid].end(); itr++)
    {
      auto sd = *itr;
      sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
    }

    if (HasMediaType(AUDIO) && UnreadQueues.find(audpid) != UnreadQueues.end() && UnreadQueues[audpid].size() > 0)
    {
      for (auto itr = UnreadQueues[audpid].begin(); itr != UnreadQueues[audpid].end(); itr++)
      {
        auto sd = *itr;
        sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
      }
    }
    for (auto itm : MetadataStreams)
    {
      if (UnreadQueues.find(itm) != UnreadQueues.end())
      {

        for (auto sd : UnreadQueues[itm])
        {
          if (sd->DiscontinousTS == nullptr)
          {
            sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstvidsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstvidsample->SamplePTS->ValueInTicks)));
          }
        }
      }
    }
  }
  else if (firstaudsample != nullptr &&
    (firstvidsample == nullptr || firstaudsample->SamplePTS->ValueInTicks <= firstvidsample->SamplePTS->ValueInTicks))
  {

    firstaudsample->DiscontinousTS = make_shared<Timestamp>(lastts + pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance);

    for (auto itr = UnreadQueues[audpid].begin() + 1; itr != UnreadQueues[audpid].end(); itr++)
    {
      auto sd = *itr;
      sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
    }
    if (HasMediaType(VIDEO) && UnreadQueues.find(vidpid) != UnreadQueues.end() && UnreadQueues[vidpid].size() > 0)
    {
      for (auto itr = UnreadQueues[vidpid].begin(); itr != UnreadQueues[vidpid].end(); itr++)
      {
        auto sd = *itr;
        sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
      }
    }
    for (auto itm : MetadataStreams)
    {
      if (UnreadQueues.find(itm) != UnreadQueues.end())
      {

        for (auto sd : UnreadQueues[itm])
        {
          if (sd->DiscontinousTS == nullptr)
          {
            sd->DiscontinousTS = make_shared<Timestamp>((unsigned long long)(firstaudsample->DiscontinousTS->ValueInTicks + ((long long) sd->SamplePTS->ValueInTicks - (long long) firstaudsample->SamplePTS->ValueInTicks)));
          }
        }
      }
    }
  }


  LOG("UpdateSampleDiscontinuityTimestampsLive() - Done for Seq " << SequenceNumber);

}



ULONG MediaSegment::HasAACTimestampTag(const BYTE *tsdata, ULONG size)
{
  ULONG idx = 0;
  //read 3 bytes at a time and carry on until you find "ID3"
  for (; idx < size; idx++)
  {
    BYTE tagid[4];
    ZeroMemory(tagid, 4);
    memcpy_s(tagid, 3, tsdata + idx, 3);
    if (string((char *) tagid) == "ID3")
    {
      idx += 3;
      break;
    }
  }
  if (idx >= size)
    return size;

  for (; idx < size; idx++)
  {
    BYTE tagid[5];
    ZeroMemory(tagid, 5);
    memcpy_s(tagid, 4, tsdata + idx, 4);
    auto s = string((char *) tagid);
    if (string((char *) tagid) == "PRIV")
    {
      idx += 4;
      break;
    }
  }
  if (idx >= size)
    return size;


  for (; idx < size; idx++)
  {
    BYTE tagid[45];
    ZeroMemory(tagid, 45);
    memcpy_s(tagid, 45, tsdata + idx, 45);
    if (string((char *) tagid) == "com.apple.streaming.transportStreamTimestamp")
    {
      idx += 45;
      break;
    }
  }
  return idx;
}

shared_ptr<Timestamp> MediaSegment::ExtractInitialTimestampFromID3PRIV(const BYTE *tsdata, ULONG size)
{
  ULONG idx = HasAACTimestampTag(tsdata, size);
  if (size <= idx) return nullptr;
  //our time stamp is a 33 bit number stored  in 8 octets   - with upper 31 bits stored as zero 

  unsigned long long _all = BitOp::ToInteger<unsigned long long>(tsdata + idx, 8);//eight octets = 64 bits
  return std::make_shared<Timestamp>(_all * (10000000 / 90000));
}

HRESULT MediaSegment::BuildAudioElementaryStreamSamples(shared_ptr<SegmentTSData> tsdata)
{
  HRESULT hr = S_OK;



  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //insert entry into the PID map
  tsdata->MediaTypePIDMap[ContentType::AUDIO] = 0;


  double numsec = (double) Duration / (pParentPlaylist->cpMediaSource->cpAudioStream != nullptr && pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0
    ? pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance : DEFAULT_AUDIOFRAME_LENGTH);
  //if duration = n seconds is an integer, then we use n samples for n seconds, 
  //else we use n + 1 samples with each sample being slightly less than 1 sec
  unsigned int samplecount = __max(1, (unsigned int) (std::ceil(numsec) == numsec ? numsec - 1 : numsec)); //need at least 1 sample in case numsec < 1
  //sample length in bytes = total length in bytes / number of samples
  unsigned int samplelen = (unsigned int) std::ceil(LengthInBytes / samplecount);


  unsigned long long timestampctr = 0;


  timestampctr = MediaSegment::ExtractInitialTimestampFromID3PRIV(tsdata->buffer.get(), LengthInBytes)->ValueInTicks;

  for (unsigned int remainder = LengthInBytes; remainder > 0; remainder -= samplelen)
  {
    //buffer size = sample length in bytes - except when the remaining bytes to be read is less than the sample length
    DWORD buffsize = remainder < samplelen ? remainder : samplelen;

    shared_ptr<SampleData> sd = make_shared<SampleData>();
    sd->SamplePTS = std::make_shared<Timestamp>(timestampctr);
    sd->elemData.push_back(tuple<BYTE*, unsigned int>(tsdata->buffer.get() + (LengthInBytes - remainder), buffsize));
    sd->TotalLen = buffsize;

    //push sample back into the unread queue
    tsdata->UnreadQueues[0].push_back(sd);
    //record timestamp
    tsdata->Timeline.push_back(sd->SamplePTS);
    timestampctr += (pParentPlaylist->cpMediaSource->cpAudioStream != nullptr && pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0 ?
      pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance : DEFAULT_AUDIOFRAME_LENGTH);
    //if remainder is less than sample length - no more bytes to read - we just read the last sample
    if (remainder < samplelen) break;
  }

  return S_OK;
}


HRESULT MediaSegment::BuildAudioElementaryStreamSamples()
{
  HRESULT hr = S_OK;




  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //insert entry into the PID map
  MediaTypePIDMap[ContentType::AUDIO] = 0;


  double numsec = (double) Duration / (pParentPlaylist->cpMediaSource->cpAudioStream != nullptr &&
    pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0 ?
    pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance : DEFAULT_AUDIOFRAME_LENGTH);
  //if duration = n seconds is an integer, then we use n samples for n seconds, 
  //else we use n + 1 samples with each sample being slightly less than 1 sec
  unsigned int samplecount = __max(1, (unsigned int) (std::ceil(numsec) == numsec ? numsec - 1 : numsec)); //need at least one sample - in case numsec < 1
  //sample length in bytes = total length in bytes / number of samples
  unsigned int samplelen = (unsigned int) std::ceil(LengthInBytes / samplecount);


  unsigned long long timestampctr = 0;


  timestampctr = MediaSegment::ExtractInitialTimestampFromID3PRIV(buffer.get(), LengthInBytes)->ValueInTicks;


  for (unsigned int remainder = LengthInBytes; remainder > 0; remainder -= samplelen)
  {
    //buffer size = sample length in bytes - except when the remaining bytes to be read is less than the sample length
    DWORD buffsize = remainder < samplelen ? remainder : samplelen;

    shared_ptr<SampleData> sd = make_shared<SampleData>();
    sd->SamplePTS = std::make_shared<Timestamp>(timestampctr);
    sd->elemData.push_back(tuple<BYTE*, unsigned int>(buffer.get() + (LengthInBytes - remainder), buffsize));
    sd->TotalLen = buffsize;

    //push sample back into the unread queue
    UnreadQueues[0].push_back(sd);
    //record timestamp
    Timeline.push_back(sd->SamplePTS);


    timestampctr += (pParentPlaylist->cpMediaSource->cpAudioStream != nullptr && pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0 ?
      pParentPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance : DEFAULT_AUDIOFRAME_LENGTH);
    //if remainder is less than sample length - no more bytes to read - we just read the last sample
    if (remainder < samplelen) break;
  }

  return S_OK;
}


///<summary>Advances the queue of unread samples for a specific PID to a specific timestamp</summary>
///<param name='PID'>The PID of the stream whose sample queue to advance</param>
///<param name='toPTS'>The timestamp to forward to</param>
///<param name='Exact'>If set to true, only forwards if an exact timestamp match is found. If set to false, 
///and no matching timestamp is found, forwards to the timestamp that is immediatel after the specified time point</param>
void MediaSegment::AdvanceUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //no samples
  if (UnreadQueues[PID].empty())
    return;

  if (pParentPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD)
  {
    if (toPTS == nullptr)
    {
      while (UnreadQueues[PID].empty() == false)
      {
        ReadQueues[PID].push_back(UnreadQueues[PID].front());
        UnreadQueues[PID].pop_front();
      }
    }
    else
    {


      auto closestsample = std::min_element(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
      {
        return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
      });

      if (closestsample != UnreadQueues[PID].end())
      {
        //pop from unread queue and push on to read queue
        auto diff = distance(UnreadQueues[PID].begin(), closestsample);
        for (auto idx = 0; idx < diff; idx++)
        {
          ReadQueues[PID].push_back(UnreadQueues[PID].front());
          UnreadQueues[PID].pop_front();
        }
      }
    }
  }
  else
  {
    if (toPTS == nullptr)
    {
      while (UnreadQueues[PID].empty() == false)
      {
        ReadQueues[PID].push_front(UnreadQueues[PID].back());
        UnreadQueues[PID].pop_back();
      }
    }
    else
    {


      auto closestsample = std::min_element(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
      {
        return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
      });

      if (closestsample != UnreadQueues[PID].rend())
      {
        //pop from unread queue and push on to read queue
        auto diff = distance(UnreadQueues[PID].rbegin(), closestsample);
        for (auto idx = 0; idx < diff; idx++)
        {
          ReadQueues[PID].push_front(UnreadQueues[PID].back());
          UnreadQueues[PID].pop_back();
        }
      }
    }
  }

}

void MediaSegment::AdvanceUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS, TimestampMatch Match)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //no samples
  if (UnreadQueues[PID].empty())
    return;

  if (pParentPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD)
  {
    if (toPTS == nullptr)
    {
      while (UnreadQueues[PID].empty() == false)
      {
        ReadQueues[PID].push_back(UnreadQueues[PID].front());
        UnreadQueues[PID].pop_front();
      }
    }
    else
    {

      if (Match == TimestampMatch::Exact)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks == toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_back(UnreadQueues[PID].front());
            UnreadQueues[PID].pop_front();
          }
        }
      }
      else if (Match == TimestampMatch::Closest)
      {
        auto matchingsample = std::min_element(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
        {
          return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_back(UnreadQueues[PID].front());
            UnreadQueues[PID].pop_front();
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreater)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks > toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_back(UnreadQueues[PID].front());
            UnreadQueues[PID].pop_front();
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreaterOrEqual)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks >= toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_back(UnreadQueues[PID].front());
            UnreadQueues[PID].pop_front();
          }
        }
      }
      else if (Match == TimestampMatch::ClosestLesser)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks < toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].rend())
        {
          AdvanceUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
      else if (Match == TimestampMatch::ClosestLesserOrEqual)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks <= toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].rend())
        {
          AdvanceUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
    }
  }
  else
  {
    if (toPTS == nullptr)
    {
      while (UnreadQueues[PID].empty() == false)
      {
        ReadQueues[PID].push_front(UnreadQueues[PID].back());
        UnreadQueues[PID].pop_back();
      }
    }
    else
    {

      if (Match == TimestampMatch::Exact)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks == toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_front(UnreadQueues[PID].back());
            UnreadQueues[PID].pop_back();
          }
        }
      }
      else if (Match == TimestampMatch::Closest)
      {
        auto matchingsample = std::min_element(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
        {
          return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
        });

        if (matchingsample != UnreadQueues[PID].rend())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].rbegin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_front(UnreadQueues[PID].back());
            UnreadQueues[PID].pop_back();
          }
        }

      }
      else if (Match == TimestampMatch::ClosestGreater)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks > toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_front(UnreadQueues[PID].back());
            UnreadQueues[PID].pop_back();
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreaterOrEqual)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks >= toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].end())
        {
          //pop from unread queue and push on to read queue
          auto diff = distance(UnreadQueues[PID].begin(), matchingsample);
          for (auto idx = 0; idx < diff; idx++)
          {
            ReadQueues[PID].push_front(UnreadQueues[PID].back());
            UnreadQueues[PID].pop_back();
          }
        }
      }
      else if (Match == TimestampMatch::ClosestLesser)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks < toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].rend())
        {
          AdvanceUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
      else if (Match == TimestampMatch::ClosestLesserOrEqual)
      {
        auto matchingsample = std::find_if(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks <= toPTS->ValueInTicks;
        });
        if (matchingsample != UnreadQueues[PID].rend())
        {
          AdvanceUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }

    }
  }
}



void MediaSegment::RewindUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //no read queue or read queue is empty
  if (ReadQueues.find(PID) == ReadQueues.end() || ReadQueues[PID].empty())
    return;
  //if we are moving forward
  if (pParentPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD)
  {

    if (toPTS == nullptr)
    {
      while (ReadQueues[PID].empty() == false)
      {
        UnreadQueues[PID].push_front(ReadQueues[PID].back());
        ReadQueues[PID].pop_back();
        if (UnreadQueues[PID].front()->spInBandCC != nullptr)
          UnreadQueues[PID].front()->SetCCRead(false);
      }
    }
    else
    {
      auto closestsample = std::min_element(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
      {
        return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
      });

      if (closestsample != ReadQueues[PID].end())
      {
        //pop from read queue and push back on to unread queue
        auto diff = distance(closestsample, ReadQueues[PID].end());
        for (auto idx = 0; idx < diff; idx++)
        {
          UnreadQueues[PID].push_front(ReadQueues[PID].back());
          ReadQueues[PID].pop_back();
          if (UnreadQueues[PID].front()->spInBandCC != nullptr)
            UnreadQueues[PID].front()->SetCCRead(false);
        }
      }
    }

  }
  else
  {
    if (toPTS == nullptr)
    {
      while (ReadQueues[PID].empty() == false)
      {
        UnreadQueues[PID].push_back(ReadQueues[PID].front());
        ReadQueues[PID].pop_front();
        if (UnreadQueues[PID].front()->spInBandCC != nullptr)
          UnreadQueues[PID].front()->SetCCRead(false);
      }
    }
    else
    {

      auto closestsample = std::min_element(ReadQueues[PID].rbegin(), ReadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
      {
        return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
      });

      //found
      if (closestsample != ReadQueues[PID].rend())
      {
        //pop from read queue and push back on to unread queue
        auto diff = distance(closestsample, ReadQueues[PID].rend());
        for (auto idx = 0; idx < diff; idx++)
        {
          UnreadQueues[PID].push_back(ReadQueues[PID].front());
          ReadQueues[PID].pop_front();
          if (UnreadQueues[PID].back()->spInBandCC != nullptr)
            UnreadQueues[PID].back()->SetCCRead(false);
        }
      }
    }
  }
}


void MediaSegment::RewindUnreadQueue(unsigned short PID, shared_ptr<Timestamp> toPTS, TimestampMatch Match)
{

  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //no read queue or read queue is empty
  if (ReadQueues.find(PID) == ReadQueues.end() || ReadQueues[PID].empty())
    return;
  //if we are moving forward
  if (pParentPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD)
  {

    if (toPTS == nullptr)
    {
      while (ReadQueues[PID].empty() == false)
      {
        UnreadQueues[PID].push_front(ReadQueues[PID].back());
        ReadQueues[PID].pop_back();
        if (UnreadQueues[PID].front()->spInBandCC != nullptr)
          UnreadQueues[PID].front()->SetCCRead(false);
      }
    }
    else
    {


      if (Match == TimestampMatch::Exact)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks == toPTS->ValueInTicks;
        });

        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::Closest)
      {
        auto matchingsample = std::min_element(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
        {
          return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreater)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks > toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreaterOrEqual)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks >= toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestLesser)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].rbegin(), ReadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks < toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].rend())
        {
          RewindUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
      else if (Match == TimestampMatch::ClosestLesserOrEqual)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].rbegin(), ReadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks <= toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].rend())
        {
          RewindUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
    }

  }
  else
  {
    if (toPTS == nullptr)
    {
      while (ReadQueues[PID].empty() == false)
      {
        UnreadQueues[PID].push_back(ReadQueues[PID].front());
        ReadQueues[PID].pop_front();
        if (UnreadQueues[PID].front()->spInBandCC != nullptr)
          UnreadQueues[PID].front()->SetCCRead(false);
      }
    }
    else
    {


      if (Match == TimestampMatch::Exact)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks == toPTS->ValueInTicks;
        });

        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_back(ReadQueues[PID].front());
            ReadQueues[PID].pop_front();
            if (UnreadQueues[PID].back()->spInBandCC != nullptr)
              UnreadQueues[PID].back()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::Closest)
      {
        auto matchingsample = std::min_element(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd1, std::shared_ptr<SampleData> sd2)
        {
          return abs((long long) (sd1->SamplePTS->ValueInTicks - toPTS->ValueInTicks)) < abs((long long) (sd2->SamplePTS->ValueInTicks - toPTS->ValueInTicks));
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_back(ReadQueues[PID].front());
            ReadQueues[PID].pop_front();
            if (UnreadQueues[PID].back()->spInBandCC != nullptr)
              UnreadQueues[PID].back()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreater)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks > toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestGreaterOrEqual)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].begin(), ReadQueues[PID].end(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks >= toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].end())
        {
          //pop from read queue and push back on to unread queue
          auto diff = distance(matchingsample, ReadQueues[PID].end());
          for (auto idx = 0; idx < diff; idx++)
          {
            UnreadQueues[PID].push_front(ReadQueues[PID].back());
            ReadQueues[PID].pop_back();
            if (UnreadQueues[PID].front()->spInBandCC != nullptr)
              UnreadQueues[PID].front()->SetCCRead(false);
          }
        }
      }
      else if (Match == TimestampMatch::ClosestLesser)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].rbegin(), ReadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks < toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].rend())
        {
          RewindUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      }
      else if (Match == TimestampMatch::ClosestLesserOrEqual)
      {
        auto matchingsample = std::find_if(ReadQueues[PID].rbegin(), ReadQueues[PID].rend(), [toPTS, this](std::shared_ptr<SampleData> sd)
        {
          return sd->SamplePTS->ValueInTicks <= toPTS->ValueInTicks;
        });
        if (matchingsample != ReadQueues[PID].rend())
        {
          RewindUnreadQueue(PID, (*matchingsample)->SamplePTS, TimestampMatch::Exact);
        }
      } 
    }
  }
} 


bool MediaSegment::HasMediaType(ContentType type)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  return (this->MediaTypePIDMap.find(type) != this->MediaTypePIDMap.end());
}

///<summary>Gets the MPEG2 TS PID for a given media type</summary>
///<param name='type'>The media type to look for</param>
///<returns>Program ID</returns>
unsigned short MediaSegment::GetPIDForMediaType(ContentType type)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  return this->MediaTypePIDMap.at(type);
}

///<summary>Normalizes a timestamp</summary>
///<param name='ts'>The timestamp to normalize</param>
///<returns>The normalized timestamp</returns>
///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
//and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
///that begins with a tiemstamp = 0</remarks>
shared_ptr<Timestamp> MediaSegment::TSAbsoluteToRelative(shared_ptr<Timestamp> ts)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //clamp to absolute start
  return pParentPlaylist->TSAbsoluteToRelative(ts);
}

///<summary>Normalizes a timestamp</summary>
///<param name='ts'>The timestamp to normalize</param>
///<returns>The normalized timestamp</returns>
///<remarks> Normalization is the process of reducing the start timestamp of a presentation (not just a segment) to 0, 
//and then offsetting every timestamp that follows by the original value of the start timestamp. This gives us a timeline
///that begins with a tiemstamp = 0</remarks>
shared_ptr<Timestamp> MediaSegment::TSAbsoluteToRelative(unsigned long long tsval)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //clamp to absolute start
  return pParentPlaylist->TSAbsoluteToRelative(tsval);
}

///<summary>Denormalizes a timestamp</summary>
///<param name='ts'>The timestamp to denormalize</param>
///<returns>The denormalized timestamp</returns>
///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
shared_ptr<Timestamp> MediaSegment::TSRelativeToAbsolute(shared_ptr<Timestamp> ts)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  return pParentPlaylist->TSRelativeToAbsolute(ts);
}

///<summary>Denormalizes a timestamp</summary>
///<param name='ts'>The timestamp to denormalize</param>
///<returns>The denormalized timestamp</returns>
///<remarks> Denormalization is the process of taking a normalized timestamp and converting it to its original value</remarks>
shared_ptr<Timestamp> MediaSegment::TSRelativeToAbsolute(unsigned long long tsval)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  return pParentPlaylist->TSRelativeToAbsolute(tsval);
}


///<summary>Calculates the time duration left in a segment from a given time point</summary>
///<param name='Position'>The timepoint to calculate duration from (in ticks)</param>
///<returns>The remaining duration (in ticks)</returns>
unsigned long long MediaSegment::GetSegmentLookahead(unsigned long long Position, MFRATE_DIRECTION direction)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  unsigned long long ret = 0;

  if (!pParentPlaylist->IsLive && Discontinous)
  {
    auto prevcumdur = pParentPlaylist->Segments.front()->SequenceNumber == SequenceNumber ? 0 : pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE)->CumulativeDuration;
    ret = (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? (Position > CumulativeDuration ? 0 : CumulativeDuration - Position) : (Position < prevcumdur ? 0 : Position - prevcumdur));
  }
  else
    ret = (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? (Position > CumulativeDuration ? 0 : CumulativeDuration - Position) : (Position < StartPTSNormalized->ValueInTicks ? 0 : Position - StartPTSNormalized->ValueInTicks));
  //LOG("GetSegmentLookAhead() : Sequence " << this->GetSequenceNumber() << ",value = " << ret);
  return ret;
}


///<summary>Calculates the time duration left in a segment from current position of the read head on the segment</summary> 
///<returns>The remaining duration (in ticks)</returns>
unsigned long long MediaSegment::GetSegmentLookahead(MFRATE_DIRECTION direction)
{


  unsigned long long ret = 0;
  //get the current position

  //if the segment is in memory
  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE)
  {
    std::unique_lock<recursive_mutex> lock(LockSegment, std::try_to_lock);

    if (lock.owns_lock())
    {

      //lets check to see if the entire segment is unread
      bool NothingRead = true;
      for (auto itr : ReadQueues)
      {
        //are we playing an alternate rendition for this content type right now ? If so ignore this content type
        if ((this->HasMediaType(ContentType::AUDIO) &&
          itr.first == this->GetPIDForMediaType(ContentType::AUDIO) &&
          pParentPlaylist->ActiveVariant != nullptr &&
          pParentPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr)
          ||
          (this->HasMediaType(ContentType::VIDEO) &&
          itr.first == this->GetPIDForMediaType(ContentType::VIDEO) &&
          pParentPlaylist->ActiveVariant != nullptr &&
          pParentPlaylist->ActiveVariant->GetActiveVideoRendition() != nullptr))
          continue;

        if (itr.second.size() != 0)
        {
          NothingRead = false;
          break;
        }
      }
      if (NothingRead)
        ret = this->Duration;
      else
      {
        std::vector<unsigned long long> retvals;
        //for all the demuxed streams
        for (auto itr : MediaTypePIDMap)
        {
          //are we playing an alternate rendition for this content type right now ? If so ignore this content type
          if ((itr.first == AUDIO &&
            pParentPlaylist->ActiveVariant != nullptr &&
            pParentPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr)
            ||
            (itr.first == VIDEO &&
            pParentPlaylist->ActiveVariant != nullptr &&
            pParentPlaylist->ActiveVariant->GetActiveVideoRendition() != nullptr))
            continue;

          //peek at the next sample on the unread queue
          auto tmp = PeekNextSample(itr.second, direction);
          if (tmp == nullptr) //at least one sample queue has been completely read - this segment has no buffer left
            break;

          //find the first sample in the segment for this PID
          auto firstsample = (ReadQueues.find(itr.second) != ReadQueues.end() &&
            ReadQueues[itr.second].size() > 0) ?
            ReadQueues[itr.second].front() : tmp;

          retvals.push_back(this->Duration - __max((tmp->SamplePTS->ValueInTicks - firstsample->SamplePTS->ValueInTicks), 0));
        }

        ret = retvals.size() > 1 ? __max(retvals[0], retvals[1]) : 0;
      }
      //LOG("GetSegmentLookAhead(Seq=" << SequenceNumber << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",state=" << (GetCurrentState() == INMEMORYCACHE ? "InMemory" : "NotInMemory") << ",value=" << ret << ")" << ",partially read = " << (NothingRead ? L"FALSE" : L"TRUE"));
    }
    else
    {
      ret = this->Duration / 3; //return approximate
      // LOG("GetSegmentLookAhead(Seq=" << SequenceNumber << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",state=" << (GetCurrentState() == INMEMORYCACHE ? "InMemory" : "NotInMemory") << ",value=" << ret);
    }


  }
  //segment not in memory
  else
  {
    //if this is the first segment, return start timestamp else return cumulative duration of the previous segment
    ret = direction == MFRATE_DIRECTION::MFRATE_FORWARD ?
      ((SequenceNumber == pParentPlaylist->Segments.front()->SequenceNumber) ? 0 : pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE)->CumulativeDuration) :
      ((SequenceNumber == pParentPlaylist->Segments.back()->SequenceNumber) ? pParentPlaylist->TotalDuration : pParentPlaylist->GetSegment(SequenceNumber)->CumulativeDuration);
    //LOG("GetSegmentLookAhead(Seq=" << SequenceNumber << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",state=" << (GetCurrentState() == INMEMORYCACHE ? "InMemory" : "NotInMemory") << ",value=" << ret << ")");
    auto a = 1;
  }

  return ret;
}

///<summary>Rewinds a segment(all unread sample queues for each PID) fully</summary>
void MediaSegment::RewindInCacheSegment()
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //for each PID in the segment
  for (auto itr : MediaTypePIDMap)
    //rewind the sample queue for that PID
  {
    LOG("REWIND Segment " << SequenceNumber);
    if (GetCurrentState() == INMEMORYCACHE)
    {
      if (Discontinous == false)
        RewindUnreadQueue(itr.second, pParentPlaylist->IsLive ? this->StartPTSNormalized : TSRelativeToAbsolute(this->StartPTSNormalized));
      else
      {
        if (ReadQueues.find(itr.second) != ReadQueues.end() && ReadQueues[itr.second].empty() == false)
        {
          auto firstsample = ReadQueues[itr.second].front();
          RewindUnreadQueue(itr.second, make_shared<Timestamp>(firstsample->SamplePTS->ValueInTicks - 1));
        }
      }
    }
  }

}

///<summary>Rewinds a segment(all unread sample queues for each PID) fully</summary>
void MediaSegment::RewindInCacheSegment(unsigned short PID)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //for each PID in the segment

  LOG("REWIND Segment " << SequenceNumber << ",PID " << PID)
    if (GetCurrentState() == INMEMORYCACHE && UnreadQueues.find(PID) != UnreadQueues.end() && UnreadQueues[PID].empty() == false)
    {

    if (Discontinous == false)
      RewindUnreadQueue(PID, pParentPlaylist->IsLive ? this->StartPTSNormalized : TSRelativeToAbsolute(this->StartPTSNormalized));
    else
    {
      if (ReadQueues.find(PID) != ReadQueues.end() && ReadQueues[PID].empty() == false)
      {
        auto firstsample = ReadQueues[PID].front();
        RewindUnreadQueue(PID, make_shared<Timestamp>(firstsample->SamplePTS->ValueInTicks - 1));
      }
    }
    }
}


///<summary>Attempts to set the current position withn a segment(all PIDs)</summary>
///<param name='PosInTicks'>The normalized position (in ticks)</param>
///<returns>A map keyed by content type, with values being the actual timepoints that the sample queue for that content type got set to</returns>
///<remarks>If there is not an exact match to the supplied timestamp in a specific sample queue, the queue gets 
///set to the nearest timestamp immediately after the supplied position</remarks>
std::map<ContentType, unsigned long long> MediaSegment::SetCurrentPosition(unsigned long long PosInTicks, MFRATE_DIRECTION direction, bool IsPositionAbsolute)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  std::map<ContentType, unsigned long long> mapret;

  //denormalize position
  shared_ptr<Timestamp> relpos = nullptr;

  /*if (this->Discontinous && pParentPlaylist->IsLive == false)
  {
  relpos = IsPositionAbsolute ? make_shared<Timestamp>(TSDiscontinousAbsoluteToAbsolute(PosInTicks)) : make_shared<Timestamp>(TSDiscontinousRelativeToAbsolute(PosInTicks));
  }
  else*/
  relpos = IsPositionAbsolute ? make_shared<Timestamp>(PosInTicks) : TSRelativeToAbsolute(PosInTicks);
  //for each PID
  for (auto itr : MediaTypePIDMap)
  {
    //if requested position is beyond current position
    if (UnreadQueues[itr.second].front()->SamplePTS->ValueInTicks < relpos->ValueInTicks)
      //advance unread queue
      AdvanceUnreadQueue(itr.second, relpos);
    //else if requested position is before current position
    else if (UnreadQueues[itr.second].front()->SamplePTS->ValueInTicks > relpos->ValueInTicks)
      ///rewind unread queue
      RewindUnreadQueue(itr.second, relpos);
    //get the first unread sample
    auto nextsample = PeekNextSample(itr.second, direction);

    if (nextsample != nullptr)
    {
      if (this->Discontinous && pParentPlaylist->IsLive == false)
      {
        //mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToDiscontinousRelative(nextsample->SamplePTS->ValueInTicks) : TSAbsoluteToDiscontinousAbsolute(nextsample->SamplePTS->ValueInTicks)));
        mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->DiscontinousTS)->ValueInTicks : nextsample->DiscontinousTS->ValueInTicks));
      }
      else
        mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->SamplePTS)->ValueInTicks : nextsample->SamplePTS->ValueInTicks));
    }
    //add it to the return map

  }
  return mapret;
}

std::map<ContentType, unsigned long long> MediaSegment::SetCurrentPosition(unsigned long long PosInTicks,TimestampMatch match, MFRATE_DIRECTION direction, bool IsPositionAbsolute)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  std::map<ContentType, unsigned long long> mapret;

  //denormalize position
  shared_ptr<Timestamp> relpos = nullptr;

  /*if (this->Discontinous && pParentPlaylist->IsLive == false)
  {
  relpos = IsPositionAbsolute ? make_shared<Timestamp>(TSDiscontinousAbsoluteToAbsolute(PosInTicks)) : make_shared<Timestamp>(TSDiscontinousRelativeToAbsolute(PosInTicks));
  }
  else*/
  relpos = IsPositionAbsolute ? make_shared<Timestamp>(PosInTicks) : TSRelativeToAbsolute(PosInTicks);
  //for each PID
  for (auto itr : MediaTypePIDMap)
  {
    //if requested position is beyond current position
    if (UnreadQueues[itr.second].front()->SamplePTS->ValueInTicks < relpos->ValueInTicks)
      //advance unread queue
      AdvanceUnreadQueue(itr.second, relpos, match);
    //else if requested position is before current position
    else if (UnreadQueues[itr.second].front()->SamplePTS->ValueInTicks > relpos->ValueInTicks)
      ///rewind unread queue
      RewindUnreadQueue(itr.second, relpos, match);
    //get the first unread sample
    auto nextsample = PeekNextSample(itr.second, direction);

    if (nextsample != nullptr)
    {
      if (this->Discontinous && pParentPlaylist->IsLive == false)
      {
        //mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToDiscontinousRelative(nextsample->SamplePTS->ValueInTicks) : TSAbsoluteToDiscontinousAbsolute(nextsample->SamplePTS->ValueInTicks)));
        mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->DiscontinousTS)->ValueInTicks : nextsample->DiscontinousTS->ValueInTicks));
      }
      else
        mapret.emplace(std::pair<ContentType, unsigned long long>(itr.first, !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->SamplePTS)->ValueInTicks : nextsample->SamplePTS->ValueInTicks));
    }
    //add it to the return map

  }
  return mapret;
}

///<summary>Attempts to set the current position within a segment for a specific PID</summary>
///<param name='PID'>PID to set the position for</param>
///<param name='PosInTicks'>The normalized position (in ticks)</param>
///<returns>The actual timepoints that the sample queue for the supplied PID got set to</returns>
///<remarks>If there is not an exact match to the supplied timestamp in a specific sample queue, the queue gets 
///set to the nearest timestamp immediately after the supplied position</remarks>
unsigned long long MediaSegment::SetCurrentPosition(unsigned short PID, unsigned long long PosInTicks, MFRATE_DIRECTION direction, bool IsPositionAbsolute)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  unsigned long long ret;
  shared_ptr<Timestamp> relpos = nullptr;
  //denormalize position
  if (this->Discontinous && pParentPlaylist->IsLive == false)
  {
    relpos = IsPositionAbsolute ? make_shared<Timestamp>(TSDiscontinousAbsoluteToAbsolute(PosInTicks)) : make_shared<Timestamp>(TSDiscontinousRelativeToAbsolute(PosInTicks));
  }
  else
    relpos = IsPositionAbsolute ? make_shared<Timestamp>(PosInTicks) : TSRelativeToAbsolute(PosInTicks);
  if (UnreadQueues[PID].empty())
  {
    if (direction == MFRATE_FORWARD)
      return this->CumulativeDuration;
    else
      return this->StartPTSNormalized->ValueInTicks;
  }
  //if requested position is beyond current position
  if (UnreadQueues[PID].front()->SamplePTS->ValueInTicks < relpos->ValueInTicks)
    //advance unread queue
    AdvanceUnreadQueue(PID, relpos);
  //else if requested position is before current position
  else if (UnreadQueues[PID].front()->SamplePTS->ValueInTicks > relpos->ValueInTicks)
    ///rewind unread queue
    RewindUnreadQueue(PID, relpos);
  //get the first unread sample
  auto nextsample = PeekNextSample(PID, direction);

  if (nextsample != nullptr)
  {
    if (this->Discontinous && pParentPlaylist->IsLive == false)
    {
      ret = !IsPositionAbsolute ? TSAbsoluteToDiscontinousRelative(nextsample->SamplePTS->ValueInTicks) : TSAbsoluteToDiscontinousAbsolute(nextsample->SamplePTS->ValueInTicks);
    }
    else
      ret = !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->SamplePTS)->ValueInTicks : nextsample->SamplePTS->ValueInTicks;
  }
  //set to return it


  return ret;
}


unsigned long long MediaSegment::SetCurrentPosition(unsigned short PID, unsigned long long PosInTicks,TimestampMatch match, MFRATE_DIRECTION direction, bool IsPositionAbsolute)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  unsigned long long ret;
  shared_ptr<Timestamp> relpos = nullptr;
  //denormalize position
  if (this->Discontinous && pParentPlaylist->IsLive == false)
  {
    relpos = IsPositionAbsolute ? make_shared<Timestamp>(TSDiscontinousAbsoluteToAbsolute(PosInTicks)) : make_shared<Timestamp>(TSDiscontinousRelativeToAbsolute(PosInTicks));
  }
  else
    relpos = IsPositionAbsolute ? make_shared<Timestamp>(PosInTicks) : TSRelativeToAbsolute(PosInTicks);
  if (UnreadQueues[PID].empty())
  {
    if (direction == MFRATE_FORWARD)
      return this->CumulativeDuration;
    else
      return this->StartPTSNormalized->ValueInTicks;
  }
  //if requested position is beyond current position
  if (UnreadQueues[PID].front()->SamplePTS->ValueInTicks < relpos->ValueInTicks)
    //advance unread queue
    AdvanceUnreadQueue(PID, relpos, match);
  //else if requested position is before current position
  else if (UnreadQueues[PID].front()->SamplePTS->ValueInTicks > relpos->ValueInTicks)
    ///rewind unread queue
    RewindUnreadQueue(PID, relpos, match);
  //get the first unread sample
  auto nextsample = PeekNextSample(PID, direction);

  if (nextsample != nullptr)
  {
    if (this->Discontinous && pParentPlaylist->IsLive == false)
    {
      ret = !IsPositionAbsolute ? TSAbsoluteToDiscontinousRelative(nextsample->SamplePTS->ValueInTicks) : TSAbsoluteToDiscontinousAbsolute(nextsample->SamplePTS->ValueInTicks);
    }
    else
      ret = !IsPositionAbsolute ? TSAbsoluteToRelative(nextsample->SamplePTS)->ValueInTicks : nextsample->SamplePTS->ValueInTicks;
  }
  //set to return it


  return ret;
}
///<summary>Gets the current position within a segment</summary>
///<returns>The current position in ticks</returns>
///<remarks>Returns the maximum of current positions across all PIDs</remarks>
unsigned long long MediaSegment::GetCurrentPosition(MFRATE_DIRECTION direction)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  unsigned long long ret = 0;
  //if the segment is in memory
  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE)
  {
    std::shared_ptr<Timestamp> maxPTS = nullptr;
    //for all the demuxed streams
    for (auto itr : MediaTypePIDMap)
    {
      //are we playing an alternate rendition for this content type right now ? If so ignore this content type
      if ((itr.first == AUDIO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr) ||
        (itr.first == VIDEO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveVideoRendition() != nullptr))
        continue;
      //peek at the next sample on the unread queue
      auto tmp = PeekNextSample(itr.second, direction);
      std::shared_ptr<Timestamp> tmppts;


      //take the normalized PTS from the sample, or if none found (i.e. for this PID every sample has been read already), use the segment cumulative duration
      tmppts = (tmp == nullptr) ? (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? std::make_shared<Timestamp>(this->CumulativeDuration) : StartPTSNormalized) : TSAbsoluteToRelative(tmp->SamplePTS);


      if (maxPTS == nullptr || tmppts->ValueInTicks > maxPTS->ValueInTicks)
        maxPTS = tmppts;
    }

    if (pParentPlaylist->IsLive == false && Discontinous)
    {
      ret = TSAbsoluteToDiscontinousRelative(maxPTS->ValueInTicks);
    }
    else
      ret = maxPTS->ValueInTicks;

  }
  //segment not in memory
  else
  {
    //if this is the first segment, return start timestamp else return cumulative duration of the previous segment
    ret = direction == MFRATE_DIRECTION::MFRATE_FORWARD ?
      ((SequenceNumber == pParentPlaylist->Segments.front()->SequenceNumber) ? 0 : pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE)->CumulativeDuration) :
      ((SequenceNumber == pParentPlaylist->Segments.back()->SequenceNumber) ? pParentPlaylist->TotalDuration : pParentPlaylist->GetSegment(SequenceNumber)->CumulativeDuration);
  }
  return ret;
}

///<summary>Gets the current position within a segment for a specific PID</summary>
///<returns>The current position in ticks</returns> 
unsigned long long MediaSegment::GetCurrentPosition(unsigned short PID, MFRATE_DIRECTION direction)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  unsigned long long ret = 0;

  //if the segment is in memory
  if (this->GetCurrentState() == MediaSegmentState::INMEMORYCACHE)
  {   //peek at the next sample on the unread queue
    auto tmp = PeekNextSample(PID, direction);
    //take the normalized PTS from the sample, or if none found (i.e. for this PID every sample has been read already), use the segment cumulative duration

    if (pParentPlaylist->IsLive == false && Discontinous)
    {
      ret = (tmp == nullptr) ?
        (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? this->CumulativeDuration : (pParentPlaylist->Segments.front()->SequenceNumber == this->SequenceNumber ? 0 : pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE)->CumulativeDuration)) :
        TSAbsoluteToDiscontinousRelative(tmp->SamplePTS->ValueInTicks);
    }
    else

      ret = (tmp == nullptr) ?
      (direction == MFRATE_DIRECTION::MFRATE_FORWARD ? this->CumulativeDuration : StartPTSNormalized->ValueInTicks) :
      TSAbsoluteToRelative(tmp->SamplePTS)->ValueInTicks;
  }
  //segment not in memory
  else
  {
    //if this is the first segment, return start timestamp else return cumulative duration of the previous segment
    ret = direction == MFRATE_DIRECTION::MFRATE_FORWARD ?
      ((SequenceNumber == pParentPlaylist->Segments.front()->SequenceNumber) ? 0 : pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE)->CumulativeDuration) :
      ((SequenceNumber == pParentPlaylist->Segments.back()->SequenceNumber) ? pParentPlaylist->TotalDuration : pParentPlaylist->GetSegment(SequenceNumber)->CumulativeDuration);
  }
  return ret;
}





bool MediaSegment::IncludesTimePoint(unsigned long long TimePoint, MFRATE_DIRECTION Direction)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  bool ret = false;


  if (!Discontinous)
  {
    //if this is the last segment or the only segment
    if (SequenceNumber == pParentPlaylist->Segments.back()->SequenceNumber || pParentPlaylist->Segments.size() == 1)
      //check if the time point falls between the start PTS and the larger of the cumulative duration or end PTS
      ret = TimePoint >= this->StartPTSNormalized->ValueInTicks &&
      TimePoint <= __max(this->CumulativeDuration, this->EndPTSNormalized->ValueInTicks);
    //this is not the last segment and we have more than one segment
    else if (pParentPlaylist->Segments.size() > 1)
    {
      auto nextseg = pParentPlaylist->GetNextSegment(SequenceNumber, Direction);
      //check it time point falls between startPTS on this seg and next seg 
      ret = TimePoint >= this->StartPTSNormalized->ValueInTicks &&
        TimePoint <= this->StartPTSNormalized->ValueInTicks + this->Duration;
    }
  }
  else//discontinous VOD
  {
    //if this is the last segment or the only segment
    if (SequenceNumber == pParentPlaylist->Segments.front()->SequenceNumber || pParentPlaylist->Segments.size() == 1)
      //check if the time point falls between the start PTS and the larger of the cumulative duration or end PTS
      ret = TimePoint >= 0 && TimePoint <= this->CumulativeDuration;
    //this is not the first segment and we have more than one segment
    else if (pParentPlaylist->Segments.size() > 1)
    {
      auto prevseg = pParentPlaylist->GetNextSegment(SequenceNumber, MFRATE_REVERSE);
      //check it time point falls between startPTS on this seg and next seg 
      ret = TimePoint >= prevseg->CumulativeDuration && TimePoint <= this->CumulativeDuration;
    }
  }



  return ret;
}
///<summary>Finds next nearest keyframe sample for a specific program at a given timestamp</summary>
///<param name='Timepoint'>The time stamp at which we are trying to find a sample</param>
///<param name='PID'>The PID for the program</param>
///<param name='Direction'>Forward or Reverse</param>
///<returns>Returns a the SampleData instance for a matching sample if found, or nullptr if not</returns>
shared_ptr<SampleData> MediaSegment::FindNearestIDRSample(unsigned long long Timepoint,
  unsigned short PID, MFRATE_DIRECTION Direction, bool IsTimepointDiscontinous, unsigned short differenceType)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  shared_ptr<SampleData> ret = nullptr;

  if ((Direction == MFRATE_FORWARD && UnreadQueues.find(PID) == UnreadQueues.end()) ||
    (Direction == MFRATE_REVERSE && ReadQueues.find(PID) == ReadQueues.end()))
  {
    ret = nullptr;
  }
  else
  {
    auto queue = (Direction == MFRATE_FORWARD ? UnreadQueues[PID] : ReadQueues[PID]);

    std::vector<tuple<unsigned long long, shared_ptr<SampleData>, bool>> diffs;
    diffs.resize(queue.size());
    std::transform(queue.begin(), queue.end(), diffs.begin(), [this, Timepoint, IsTimepointDiscontinous, differenceType](std::shared_ptr<SampleData> sd)
    {
      return tuple<unsigned long long, shared_ptr<SampleData>, bool>(
        (unsigned long long)abs((long long) ((IsTimepointDiscontinous && sd->DiscontinousTS != nullptr ? sd->DiscontinousTS->ValueInTicks : sd->SamplePTS->ValueInTicks) - Timepoint)), sd,
        differenceType == 0 ? true :
        (differenceType > 0 ? (IsTimepointDiscontinous && sd->DiscontinousTS != nullptr ? sd->DiscontinousTS->ValueInTicks : sd->SamplePTS->ValueInTicks) > Timepoint :
      (IsTimepointDiscontinous && sd->DiscontinousTS != nullptr ? sd->DiscontinousTS->ValueInTicks : sd->SamplePTS->ValueInTicks) < Timepoint));
    });
    std::sort(diffs.begin(), diffs.end(), [this](tuple<unsigned long long, shared_ptr<SampleData>, bool> v1, tuple<unsigned long long, shared_ptr<SampleData>, bool> v2)
    {
      return std::get<0>(v1) < std::get<0>(v2);
    });
    auto match = std::find_if(diffs.begin(), diffs.end(), [this](tuple<unsigned long long, shared_ptr<SampleData>, bool> t)
    {
      return std::get<1>(t)->IsSampleIDR == true && std::get<2>(t) == true;
    });

    if (match != diffs.end())
      return std::get<1>(*match);


  }
  return ret;
}

shared_ptr<Timestamp> MediaSegment::PositionQueueAtNextIDR(unsigned short PID, MFRATE_DIRECTION Direction, unsigned short IDRSkipCount)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  std::shared_ptr<Timestamp> ret = nullptr;

  auto sdnext = PeekNextSample(PID, Direction);
  ret = (sdnext != nullptr ? sdnext->SamplePTS : nullptr);


  unsigned short skipped = 0;
  if (Direction == MFRATE_FORWARD)
  {
    while (UnreadQueues[PID].empty() == false)
    {
      if (UnreadQueues[PID].front()->IsSampleIDR)
      {
        if (IDRSkipCount == skipped)
          break;
        else
          skipped++;
      }

      auto sd = UnreadQueues[PID].front();
      //pop front from unread queue
      UnreadQueues[PID].pop_front();
      //push back to read queue
      ReadQueues[PID].push_back(sd);
      auto sdnext = PeekNextSample(PID, Direction);
      ret = (sdnext != nullptr ? sdnext->SamplePTS : nullptr);
    }
  }
  else
  {
    while (UnreadQueues[PID].empty() == false)
    {
      if (UnreadQueues[PID].back()->IsSampleIDR)
      {
        if (IDRSkipCount == skipped)
          break;
        else
          skipped++;
      }
      auto sd = UnreadQueues[PID].back();
      //pop front from unread queue
      UnreadQueues[PID].pop_back();
      //push back to read queue
      ReadQueues[PID].push_front(sd);
      auto sdnext = PeekNextSample(PID, Direction);
      ret = (sdnext != nullptr ? sdnext->SamplePTS : nullptr);
    }

  }

  return ret;
}

shared_ptr<SampleData> MediaSegment::PeekNextIDR(MFRATE_DIRECTION Direction, unsigned short IDRSkipCount)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  std::shared_ptr<SampleData> ret = nullptr;

  if (HasMediaType(ContentType::VIDEO) == false) return nullptr;

  auto PID = GetPIDForMediaType(ContentType::VIDEO);
  if (UnreadQueues.find(PID) == UnreadQueues.end() || UnreadQueues[PID].empty()) return nullptr;


  unsigned short skipped = 0;
  if (Direction == MFRATE_FORWARD)
  {
    auto found = std::find_if(UnreadQueues[PID].begin(), UnreadQueues[PID].end(), [this, &skipped, IDRSkipCount](shared_ptr<SampleData> sd)
    {
      if (!sd->IsSampleIDR)
        return false;
      if (IDRSkipCount != skipped)
      {
        skipped++;
        return false;
      }
      return true;
    });
    if (found != UnreadQueues[PID].end())
      ret = *found;
  }
  else
  {
    auto found = std::find_if(UnreadQueues[PID].rbegin(), UnreadQueues[PID].rend(), [this, &skipped, IDRSkipCount](shared_ptr<SampleData> sd)
    {
      if (!sd->IsSampleIDR)
        return false;
      if (IDRSkipCount != skipped)
      {
        skipped++;
        return false;
      }
      return true;
    });
    if (found != UnreadQueues[PID].rend())
      ret = *found;
  }

  return ret;
}
///<summary>Gets the next unread sample for a program</summary>
///<param name='PID'>The PID for the program in which we are looking for a sample</param>
///<returns>MF Sample</returns>
void MediaSegment::GetNextSample(unsigned short PID, MFRATE_DIRECTION Direction, IMFSample **ppSample)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  std::shared_ptr<SampleData> sd = nullptr;
  //if unread queue is empty - return nullptr 
  sd = UnreadQueues[PID].empty() == false ? (Direction == MFRATE_DIRECTION::MFRATE_FORWARD ? UnreadQueues[PID].front() : UnreadQueues[PID].back()) : nullptr;

  if (sd != nullptr)
  {
    if (Direction == MFRATE_DIRECTION::MFRATE_FORWARD)
    {
      //pop front from unread queue
      UnreadQueues[PID].pop_front();
      //push back to read queue
      ReadQueues[PID].push_back(sd);
    }
    else if (Direction == MFRATE_DIRECTION::MFRATE_REVERSE)
    {
      //pop front from unread queue
      UnreadQueues[PID].pop_back();
      //push back to read queue
      ReadQueues[PID].push_front(sd);
    }

    HRESULT hr = S_OK;

    try
    {
      ComPtr<IMFMediaBuffer> mediabuffer = nullptr;
      auto len = sd->TotalLen;
      if (FAILED(hr = ::MFCreateSample(ppSample)))
        throw hr;


      if (FAILED(hr = MFCreateMemoryBuffer(len, &mediabuffer))) throw hr;
      if (FAILED(hr = (*ppSample)->AddBuffer(mediabuffer.Get()))) throw hr;

      BYTE *pRawBuff = nullptr;
      DWORD cbMaxLen, cbCurLen;
      if (FAILED(hr = mediabuffer->Lock(&pRawBuff, &cbMaxLen, &cbCurLen))) throw hr;

      unsigned int offset = 0;
      for (auto itr : sd->elemData)
      {
        memcpy_s(pRawBuff + offset, std::get<1>(itr), std::get<0>(itr), std::get<1>(itr));
        offset += std::get<1>(itr);
      }

      if (FAILED(hr = mediabuffer->SetCurrentLength(len))) throw hr;
      if (FAILED(hr = mediabuffer->Unlock())) throw hr;
    }
    catch (...)
    {
      if (*ppSample != nullptr)
      {
        (*ppSample)->Release();
        *ppSample = nullptr;
      }
    }

    if (sd->IsSampleIDR)
      (*ppSample)->SetUINT32(MFSampleExtension_CleanPoint, TRUE); //key frame

    unsigned long long ts = 0;
    //normalize timestamp 
    if (sd->SamplePTS != nullptr)
    {
      if (pParentPlaylist->IsLive)
      {
        if (Discontinous && sd->DiscontinousTS != nullptr)
          ts = sd->DiscontinousTS->ValueInTicks;
        else
          ts = sd->SamplePTS->ValueInTicks;
      }
      else
      {
        if (Discontinous && sd->DiscontinousTS != nullptr)
          ts = TSAbsoluteToRelative(sd->DiscontinousTS)->ValueInTicks;
        else
          ts = TSAbsoluteToRelative(sd->SamplePTS)->ValueInTicks;
      }
    }

    //if the sample has a PTS - we apply the timestamp. Our logic for applying a timestamp is as follows:
    //If we are playing content from the main track (audio or video) or alternate track video we always apply a timestamp  
    //If we are playing an alternate track audio then
    //  If configuration setting EnableTimestampsOnAlternateAudio forces us - we apply TS to all alternate audio samples
    //  else we only apply TS to the first alternate audio sample right after a seek (indicated by the TimestampOnNextAudioSample flag) and turn the flag off
    //This helps us avoid a current issue of audio timestamps getting out of sync with video when alternate audio is being played and a seek happens.

    if (sd->SamplePTS != nullptr)
    {

      //only log this for the first sample and last sample in the segment for now

      LOGIF(MediaTypePIDMap.find(ContentType::AUDIO) != MediaTypePIDMap.end() && MediaTypePIDMap[ContentType::AUDIO] == PID,
        "AUDIO Sample(ts=" << ts << ", Index = " << sd->Index << ",Seg =" << SequenceNumber << ",PID=" << PID << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",real ts=" << sd->SamplePTS->ValueInTicks << ",start PTS=" << (pParentPlaylist->StartPTSOriginal != nullptr ? pParentPlaylist->StartPTSOriginal->ValueInTicks : 0) << ",Discontinous : " << (Discontinous ? L"TRUE" : L"FALSE") << ", " << this->MediaUri << ")");

      LOGIF(MediaTypePIDMap.find(ContentType::VIDEO) != MediaTypePIDMap.end() && MediaTypePIDMap[ContentType::VIDEO] == PID,
        "VIDEO Sample(ts=" << ts << ", Index = " << sd->Index << ",Seg =" << SequenceNumber << ",IDR = " << (sd->IsSampleIDR ? "Yes" : "No") << ",PID=" << PID << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",real ts=" << sd->SamplePTS->ValueInTicks << ",start PTS=" << (pParentPlaylist->StartPTSOriginal != nullptr ? pParentPlaylist->StartPTSOriginal->ValueInTicks : 0) << ",Discontinous : " << (Discontinous ? L"TRUE" : L"FALSE") << ", " << this->MediaUri << ")");


      (*ppSample)->SetSampleTime(ts);

    }
    else
    {

      /* LOGIF(MediaTypePIDMap.find(ContentType::AUDIO) != MediaTypePIDMap.end() && MediaTypePIDMap[ContentType::AUDIO] == PID,
      "AUDIO Sample(ts=0,Seq=" << SequenceNumber << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",real ts=" << sd->SamplePTS->ValueInTicks << ",start PTS=" << (pParentPlaylist->StartPTSOriginal != nullptr ? pParentPlaylist->StartPTSOriginal->ValueInTicks : 0) << ")");

      LOGIF(MediaTypePIDMap.find(ContentType::VIDEO) != MediaTypePIDMap.end() && MediaTypePIDMap[ContentType::VIDEO] == PID,
      "VIDEO Sample(ts=0,Seq=" << SequenceNumber << ",Speed=" << (pParentPlaylist->pParentStream != nullptr ? pParentPlaylist->pParentStream->Bandwidth : 0) << ",real ts=" << sd->SamplePTS->ValueInTicks << ",start PTS=" << (pParentPlaylist->StartPTSOriginal != nullptr ? pParentPlaylist->StartPTSOriginal->ValueInTicks : 0) << ")");*/
    }


  }

  return;
}

///<summary>Checks to see if all streams within the segment are at an end i.e. the segment has been fully read</summary>
///<returns>True or False</returns>
bool MediaSegment::IsReadEOS()
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  bool ret = true;

  //for each stream in the segment 
  for (auto itr : MediaTypePIDMap)
  {
    //are we playing an alternate rendition for this content type right now ? If so ignore this content type 
    if ((itr.first == AUDIO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr) ||
      (itr.first == VIDEO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveVideoRendition() != nullptr))
      continue;
    if (itr.first != AUDIO && itr.first != VIDEO) continue;
    //check if the sample queue for the stream is empty. If not we break - at least one stream in the segment still has samples to be read.
    ret = this->IsEmptySampleQueue(itr.second);
    if (!ret)
      break;
  }
  return ret;
}

bool MediaSegment::HasUnreadCCData()
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  if (HasMediaType(ContentType::VIDEO))
  {
    auto vidpid = GetPIDForMediaType(ContentType::VIDEO);
    return ((UnreadQueues[vidpid].size() > 0 && std::find_if(begin(UnreadQueues[vidpid]), end(UnreadQueues[vidpid]), [this](shared_ptr<SampleData> sd) { return sd->spInBandCC != nullptr; }) != end(UnreadQueues[vidpid])) ||
      (ReadQueues.find(vidpid) != end(ReadQueues) && ReadQueues[vidpid].size() > 0 && std::find_if(begin(ReadQueues[vidpid]), end(ReadQueues[vidpid]), [this](shared_ptr<SampleData> sd) { return sd->spInBandCC != nullptr && sd->GetCCRead() == false; }) != end(ReadQueues[vidpid])));
  }
  else
    return false;
}
bool MediaSegment::CanScavenge()
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  bool ret = true;

  //for each stream in the segment 
  for (auto itr : MediaTypePIDMap)
  {
    //are we playing an alternate rendition for this content type right now ? If so ignore this content type 
    if ((itr.first == AUDIO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveAudioRendition() != nullptr) ||
      (itr.first == VIDEO && pParentPlaylist->ActiveVariant != nullptr && pParentPlaylist->ActiveVariant->GetActiveVideoRendition() != nullptr))
      continue;
    //check if the sample queue for the stream is empty. If not we break - at least one stream in the segment still has samples to be read.
    ret = this->IsEmptySampleQueue(itr.second);
    if (!ret)
      return false;
  }

  return ret;
}


///<summary>Checks to see if a stream within the segment is at an end</summary>
///<param name='PID'>The PID for the stream</param>
///<returns>True or False</returns>
bool MediaSegment::IsReadEOS(unsigned short PID)
{
  std::lock_guard<std::recursive_mutex> lock(LockSegment);
  //is the sample queue for the PID empty ?
  return this->IsEmptySampleQueue(PID);
}

///<summary>Returns the sequence number of the current segment relative to a zero base(i.e. offset by the playlist base sequence number i.e. EXT-X-MEDIA-SEQUENCE)</summary>
///<remarks>We want a zero based sequence number to stay in tune with zero based collection indexing. If the sequence number of the first segment is non zero, this returns the current sequence number minus the base sequence number for the playlist.</remarks>
///<returns>Sequence Number</returns>
//unsigned int MediaSegment::GetSequenceNumberZeroBased()
//{
//  //we want a zero based sequence number to stay in tune with zero based collection addressing  
//  return (pParentPlaylist->BaseSequenceNumber == 0) ? SequenceNumber : (SequenceNumber >= pParentPlaylist->BaseSequenceNumber ? SequenceNumber - pParentPlaylist->BaseSequenceNumber : 0);
//} 

///<summary>Gets the current state of the segment</summary>
///<returns>Current segment state</returns>
MediaSegmentState MediaSegment::GetCurrentState()
{
  //std::lock_guard<std::recursive_mutex> lock(LockSegment);
  return State;
}
