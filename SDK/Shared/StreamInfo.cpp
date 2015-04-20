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
#include <memory>
#include <ppltasks.h>
#include "Cookie.h"
#include "StreamInfo.h" 
#include "Rendition.h"
#include "Playlist.h" 
#include "TSConstants.h" 
#include "HLSController.h"  
#include "Cookie.h"
#include "FileLogger.h"
#include "ContentDownloader.h"
#include "ContentDownloadRegistry.h"
#include <robuffer.h>
#include <windows.storage.streams.h>

using namespace std;
using namespace concurrency;
using namespace Microsoft::HLSClient::Private;
using namespace Windows::Storage::Streams;

StreamInfo::~StreamInfo()
{
  
  spDownloadRegistry->CancelAll(true);

  if (spPlaylist != nullptr)
    spPlaylist.reset();
}
/// <summary>StreamInfo constructor</summary>
/// <param name='tagWithAttributes'>The EXT-X-STREAM-INF tag with the attributes</param>
/// <param name='playlisturi'>The uri for the playlist for this variant</param> 
/// <param name='parentPlaylist'>The parent (master) playlist</param>
StreamInfo::StreamInfo(std::wstring& tagWithAttributes, std::wstring& playlisturi, Playlist *parentPlaylist) :
Bandwidth(0),
AudioRenditionGroupID(L""),
VideoRenditionGroupID(L""),
SubtitlesRenditionGroupID(L""),
IsActive(false),
HorizontalResolution(0),
VerticalResolution(0),
PlaylistUri(L""),
pParentPlaylist(parentPlaylist),
spPlaylist(nullptr),
VideoMediaType(GUID_NULL),
AudioMediaType(GUID_NULL),
AudioRenditions(nullptr),
VideoRenditions(nullptr),
SubtitleRenditions(nullptr),
pActiveAudioRendition(nullptr),
pActiveVideoRendition(nullptr),
DownloadFailureCount(0),
FailureCountMeasureTimestamp(0)
{

  spDownloadRegistry = make_shared<ContentDownloadRegistry>();
  //if the supplied URI is not absolute, combine with base URI to make absolute
  if (Helpers::IsAbsoluteUri(playlisturi))
    PlaylistUri = playlisturi;
  else
    PlaylistUri = Helpers::JoinUri(parentPlaylist->BaseUri, playlisturi);


  //parse attributes
  std::wstring allattribs = Helpers::ReadAttributeList(tagWithAttributes);
  //bandwidth for this variant
  Helpers::ReadNamedAttributeValue(allattribs, L"BANDWIDTH", Bandwidth);
  //codecs
  Helpers::ReadNamedAttributeValue(allattribs, L"CODECS", Codecs);
  //parse the codecs
  ParseCodecsString(Codecs);
  //program id
  HasProgramID = Helpers::ReadNamedAttributeValue(allattribs, L"PROGRAM-ID", ProgramID);
  //resolution
  HasResolution = Helpers::ReadNamedAttributeValue(allattribs, L"RESOLUTION", HorizontalResolution, VerticalResolution);
  //audio rendition group id
  HasAudioRenditionGroup = Helpers::ReadNamedAttributeValue(allattribs, Rendition::TYPEAUDIO, AudioRenditionGroupID);
  //video rendition group id
  HasVideoRenditionGroup = Helpers::ReadNamedAttributeValue(allattribs, Rendition::TYPEVIDEO, VideoRenditionGroupID);
  //subtitle rendition group id
  HasSubtitlesRenditionGroup = Helpers::ReadNamedAttributeValue(allattribs, Rendition::TYPESUBTITLES, SubtitlesRenditionGroupID);

}

bool StreamInfo::IsQuarantined()
{
  if (DownloadFailureCount < MAXALLOWEDDOWNLOADFAILURES)
    return false;

  if (DownloadFailureCount == MAXALLOWEDDOWNLOADFAILURES)
  {  
    ULONGLONG tc = ::GetTickCount64();
    if (FailureCountMeasureTimestamp > 0 && (tc - FailureCountMeasureTimestamp) < DOWNLOADFAILUREQUARANTINEDURATION) 
      return true;
    else if (FailureCountMeasureTimestamp == 0)
    {
      FailureCountMeasureTimestamp = tc;
      return true;
    }
    else
    {
      FailureCountMeasureTimestamp = 0;
      DownloadFailureCount = 0;
      return false;
    }
  }

  return false;
}
void StreamInfo::AddBackupPlaylistUri(wstring uri)
{
  if (!Helpers::IsAbsoluteUri(uri))
    uri = Helpers::JoinUri(pParentPlaylist->BaseUri, uri);

  if (std::find_if(BackupPlaylistUris.begin(), BackupPlaylistUris.end(), [uri](wstring u) { return u == uri; }) == BackupPlaylistUris.end())
    BackupPlaylistUris.push_back(uri);
}

/// <summary>Parses the codec string and translates to matching media foundation media subtypes</summary>
/// <remarks>We only support H.264 and AAC in this version. In case no codec string is supplied we assume H.264 and AAC.</remarks>
/// <param name='codecstring'>The codec string extracted from the playlist</param>
void StreamInfo::ParseCodecsString(std::wstring& codecstring)
{
  //something supplied
  if (!codecstring.empty())
  {
    auto codecup = Helpers::ToUpper(codecstring);
    //audio
    if (codecup.find(L"MP4A") != std::wstring::npos)
    {
      if (codecup.find(L".40.34") != std::wstring::npos) //mp3 audio
      {
        this->AudioMediaType = MFAudioFormat_MP3;
      }
      else
      {
        this->AudioMediaType = MFAudioFormat_AAC;
      }
    }
      
    //H.264 video 
    if (codecup.find(L"AVC") != std::wstring::npos)
      this->VideoMediaType = MFVideoFormat_H264;
  }
  else //nothing supplied
  {
    this->AudioMediaType = MFAudioFormat_AAC;
    this->VideoMediaType = MFVideoFormat_H264;
  }
}


/// <summary>Activates or deactivates current variant</summary>
/// <param name='Active'>If true, activates the variant, else if false deactivates the variant</param>
/// <returns>Task to wait on</returns>
void StreamInfo::MakeActive(bool Active)
{
  //if activating
  if (Active)
  {
    //mark flag
    this->IsActive = true;
    //set the active variant on the parent playlist to point to this variant
    pParentPlaylist->ActiveVariant = this;
    
    if (pParentPlaylist->ActiveVariant->spPlaylist != nullptr )
      pParentPlaylist->ActiveVariant->spPlaylist->SetPIDFilter(
      pParentPlaylist->GetPIDFilter().empty() ? 
      nullptr : 
      make_shared<std::map<ContentType, unsigned short>>(pParentPlaylist->GetPIDFilter()));

  }
  else
  {
    //mark inactive
    this->IsActive = false;
    //cancel any pending downloads for this playlist
    spDownloadRegistry->CancelAll();
    spPlaylist->spDownloadRegistry->CancelAll();
    FailureCountMeasureTimestamp = 0;
    DownloadFailureCount = 0;
  }
   
  return;
}

/// <summary>Downloads and parses variant playlist</summary>
/// <param name='tcePlaylistDownloaded'>Task completion event that is used to create a waitable task to be returned</param>
/// <returns>Task to wait on</returns>


task<HRESULT> StreamInfo::DownloadPlaylistAsync(task_completion_event<HRESULT> tcePlaylistDownloaded)
{
  CHLSMediaSource * ms = pParentPlaylist->cpMediaSource;
  

  if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED || IsQuarantined())
  {
    tcePlaylistDownloaded.set(E_FAIL);
    return task<HRESULT>(tcePlaylistDownloaded);
  }


  std::map<wstring, wstring> headers;
  std::vector<shared_ptr<Cookie>> cookies;
  wstring url = PlaylistUri;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
  ms->cpController->RaisePrepareResourceRequest(ResourceType::PLAYLIST, url, cookies, headers, &external);

  DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();
  //start the async download
  downloader->Initialize(ref new Platform::String(url.data()));
  if (external == nullptr)
    downloader->SetParameters(  nullptr, L"GET", cookies, headers);
  else
    downloader->SetParameters(  nullptr, external);

  downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
    [this, tcePlaylistDownloaded, ms](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);

    

    if ((ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED))
    {
      DownloadFailureCount++;
      tcePlaylistDownloaded.set(E_FAIL);
    }
    else
    {
      if (args->Content != nullptr && args->IsSuccessStatusCode)
      {
        std::vector<BYTE> MemoryCache = DefaultContentDownloader::BufferToVector(args->Content);
        if (MemoryCache.size() == 0)
        {
          DownloadFailureCount++;
          tcePlaylistDownloaded.set(E_FAIL);
        } 
        else
        {
          //in case there was a redirect
          PlaylistUri = args->ContentUri->AbsoluteUri->Data();

          this->OnPlaylistDownloadCompleted(MemoryCache, tcePlaylistDownloaded);
        }
      }
      else
      {
        DownloadFailureCount++;
        tcePlaylistDownloaded.set(E_FAIL);
      } 
    }

 //   spDownloadRegistry->Unregister(downloader);
  });

  downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
    [this, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);
  ///  spDownloadRegistry->Unregister(downloader);
    if (args->StatusCode != Windows::Web::Http::HttpStatusCode::Ok)
    {
      DownloadFailureCount++;
      tcePlaylistDownloaded.set(E_FAIL);
    }
    else
      tcePlaylistDownloaded.set(S_OK);//possibly task cancellation
      
  });

  spDownloadRegistry->Register(downloader);
  downloader->DownloadAsync();

  //return a task wrapperd around the TCE that was passed in - will be signalled once download completes
  return task<HRESULT>(tcePlaylistDownloaded);
}

HRESULT StreamInfo::OnPlaylistDownloadCompleted(std::vector<BYTE> MemoryCache, task_completion_event<HRESULT> tcePlaylistDownloaded)
{

  if (this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR || this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tcePlaylistDownloaded.set(E_FAIL);
    return E_FAIL;
  }

  std::wstring baseuri, filename;
  //split the uri for later use
  Helpers::SplitUri(PlaylistUri, baseuri, filename);
  if (spPlaylist == nullptr) //first time download
  {

    //create the playlist
    spPlaylist = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename, this);

    std::lock_guard<std::recursive_mutex> lockmerge(spPlaylist->LockMerge);
    //attach the media source
    spPlaylist->AttachMediaSource(this->pParentPlaylist->cpMediaSource);

    LOG(" *** PLAYLIST DOWNLOAD *** ");
    LOG(spPlaylist->szData);
    //parse the playlist
    spPlaylist->Parse();

    if (spPlaylist->IsValid == false)
    {
      tcePlaylistDownloaded.set(E_FAIL);
      return E_FAIL;
    }

    if (this->pParentPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING){
      auto audioren = this->GetActiveAudioRendition();
      auto videoren = this->GetActiveVideoRendition();
      //are we playing an alternate rendition ? 
      if (audioren != nullptr)
      {
        if (audioren->spPlaylist == nullptr &&
          FAILED(audioren->DownloadRenditionPlaylistAsync().get()))
        {
          tcePlaylistDownloaded.set(E_FAIL);
          return E_FAIL;
        }
      }
      if (videoren != nullptr)
      {
        if (videoren->spPlaylist == nullptr &&
          FAILED(videoren->DownloadRenditionPlaylistAsync().get()))
        {
          tcePlaylistDownloaded.set(E_FAIL);
          return E_FAIL;
        }
      }
    }

    if (spPlaylist->IsLive && spPlaylist->pParentStream->IsActive)
      spPlaylist->SetStopwatchForNextPlaylistRefresh((spPlaylist->DerivedTargetDuration) / 2); 

  }
  else //we will try to merge
  {
    std::lock_guard<std::recursive_mutex> lockmerge(spPlaylist->LockMerge);
    spPlaylistRefresh = make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename, this);
    if (spPlaylist->szData != spPlaylistRefresh->szData)
      spPlaylistRefresh->Parse();
    if (spPlaylistRefresh->Segments.size() > 0 && spPlaylistRefresh->Segments.back()->SequenceNumber > spPlaylist->Segments.back()->SequenceNumber) //only do next if the main playlist changes
    {

      auto audioren = this->GetActiveAudioRendition(); 
      //are we playing an alternate rendition ? 
      if (audioren != nullptr)
      {
        if ((audioren->spPlaylist == nullptr || audioren->spPlaylist->IsLive) &&
          FAILED(audioren->DownloadRenditionPlaylistAsync().get()))
        {
          tcePlaylistDownloaded.set(E_FAIL);
          return E_FAIL;
        }
      }
    } 

    LOG(" *** PLAYLIST REFRESH *** ");
    LOG(spPlaylistRefresh->szData);

  }

  if (BatchItems.size() > 0)
  {
    for (auto itm : BatchItems)
    {
      shared_ptr<Playlist> pl = nullptr;
      wstring url = itm.first;
      HRESULT hr = DownloadBatchPlaylist(pl, url).get(); 

      BatchItems[url] = pl;

      if (FAILED(hr) || !this->spPlaylist->CombinePlaylistBatch(pl))
      {
        tcePlaylistDownloaded.set(E_FAIL);
        return S_OK;
      }

    }
  }

  //signal async completion 
  tcePlaylistDownloaded.set(S_OK);
  return S_OK;
}

task<HRESULT> StreamInfo::DownloadBatchPlaylist(shared_ptr<Playlist>& spPlaylist, wstring& uri, task_completion_event<HRESULT> tcePlaylistDownloaded)
{
  //create a downloader
  CHLSMediaSource * ms = pParentPlaylist->cpMediaSource;
 
  if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tcePlaylistDownloaded.set(E_FAIL);
    return task<HRESULT>(tcePlaylistDownloaded);
  }

  std::map<wstring, wstring> headers;
  std::vector<shared_ptr<Cookie>> cookies;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;

  wstring url = uri;

  ms->cpController->RaisePrepareResourceRequest(ResourceType::PLAYLIST, url, cookies, headers, &external);
  DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();
  //start the async download
  downloader->Initialize(ref new Platform::String(url.data()));
  if (external == nullptr)
    downloader->SetParameters(  nullptr, L"GET", cookies, headers, false, (spPlaylist != nullptr ? spPlaylist->LastModified : L""), (spPlaylist != nullptr ? spPlaylist->ETag : L""));
  else
    downloader->SetParameters(  nullptr, external);

  downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
    [this, &spPlaylist, tcePlaylistDownloaded, ms](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);

    if ((ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED))
    {
      tcePlaylistDownloaded.set(E_FAIL);
    }
    else
    {

      if (args->Content != nullptr && args->IsSuccessStatusCode)
      {
        std::vector<BYTE> MemoryCache = DefaultContentDownloader::BufferToVector(args->Content);
        if (MemoryCache.size() == 0)
          tcePlaylistDownloaded.set(E_FAIL);
        else
          this->OnBatchPlaylistDownloadCompleted(spPlaylist, args, MemoryCache, tcePlaylistDownloaded);
      }
      else
        tcePlaylistDownloaded.set(E_FAIL);
    }

  //  spDownloadRegistry->Unregister(downloader);

  });

  downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
    [this, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);
//    spDownloadRegistry->Unregister(downloader);
    tcePlaylistDownloaded.set(E_FAIL);
  });


  this->spDownloadRegistry->Register(downloader);
  downloader->DownloadAsync();
  return task<HRESULT>(tcePlaylistDownloaded);
}

HRESULT StreamInfo::OnBatchPlaylistDownloadCompleted(shared_ptr<Playlist>& pPlaylist,
  Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args,
  std::vector<BYTE> MemoryCache,
  task_completion_event<HRESULT> tcePlaylistDownloaded)
{

  if (this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR || this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tcePlaylistDownloaded.set(E_FAIL);
    return E_FAIL;
  }

  std::wstring baseuri, filename;
  //in case of a redirect
  PlaylistUri = args->ContentUri->AbsoluteUri->Data();
  //split the uri for later use
  Helpers::SplitUri(PlaylistUri, baseuri, filename);

  std::wstring lastmod, etag;

  if (args->ResponseHeaders->HasKey(L"Last-Modified"))
    lastmod = args->ResponseHeaders->Lookup(L"Last-Modified")->Data();
  if (args->ResponseHeaders->HasKey(L"ETag"))
    etag = args->ResponseHeaders->Lookup(L"ETag")->Data();

  if (pPlaylist == nullptr) //first  download
  {
    //create the playlist
    pPlaylist = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename, (StreamInfo*)nullptr);
    //attach the media source
    pPlaylist->AttachMediaSource(this->pParentPlaylist->cpMediaSource);
    pPlaylist->SetLastModifiedSince(lastmod, etag);
    //parse the playlist
    pPlaylist->Parse();



    LOG(" *** PLAYLIST DOWNLOAD *** ");
    LOG(pPlaylist->szData);
  }

  //signal async completion 
  tcePlaylistDownloaded.set(S_OK);
  return S_OK;
}

/// <summary>Prepare the stream for a switch to/from an alternate rendition</summary>
/// <param name="RenditionType">The type of rendition we are switching to</param>
/// <param name="pTargetRendition">The rendition instance we are switching to - can be nullptr if we are switching back to the main track</param>
/// <returns>Returns the target playlist</returns>
shared_ptr<Playlist> StreamInfo::PrepareRenditionSwitch(std::wstring RenditionType, Rendition *pTargetRendition)
{

  //switching back to main playlist
  if (pTargetRendition == nullptr || (pTargetRendition->Default && pTargetRendition->PlaylistUri.empty()))
  {
    shared_ptr<Playlist> retval = nullptr;

    //if we are switching audio
    if (RenditionType == Rendition::TYPEAUDIO)
    {
      for (auto itr = AudioRenditions->begin(); itr != AudioRenditions->end(); itr++)
      {
        if (itr->get()->IsMatching(pTargetRendition) == false)
          itr->get()->IsActive = false;
      }
      //mark the current rendition inactive

      if (GetActiveAudioRendition() != nullptr)
        SetActiveAudioRendition(nullptr);
    }
    //else if we are switching video
    else if (RenditionType == Rendition::TYPEVIDEO)
    {
      for (auto itr = VideoRenditions->begin(); itr != VideoRenditions->end(); itr++)
      {
        if (itr->get()->IsMatching(pTargetRendition) == false)
          itr->get()->IsActive = false;
      }
      //mark the current video rendition inactive

      if (GetActiveVideoRendition() != nullptr)
        SetActiveVideoRendition(nullptr);
    }
    //return the main playlist as the target playlist
    retval = spPlaylist;

    return retval;
  }
  //we are switching to an alternate rendition playlist
  else
  {
    //we only handle alternate audio and video here - subtitles are handled differently
    if (RenditionType != Rendition::TYPEAUDIO && RenditionType != Rendition::TYPEVIDEO)
      return nullptr;

    unsigned int targetSegIdx = 0;
    //map the rendition type to content type
    ContentType contentType = RenditionType == Rendition::TYPEAUDIO ? ContentType::AUDIO : ContentType::VIDEO;

    auto targetSeg = pTargetRendition->spPlaylist->GetNextSegment(spPlaylist->GetCurrentSegmentTracker(contentType)->SequenceNumber, pTargetRendition->spPlaylist->cpMediaSource->curDirection);//try switch starting at the next segment

    //if we are not past the end of the target rendition
    if (targetSeg != nullptr)
    {

      //we set the current index counter on the target rendition playlist 
      pTargetRendition->spPlaylist->SetCurrentSegmentTracker(contentType, targetSeg);
      //set the start timestamp to mirror the timestamp on the main playlist 
      pTargetRendition->spPlaylist->StartPTSOriginal = spPlaylist->StartPTSOriginal;


      //set the active rendition variables on the variant to point to the appropriate rendition instances
      if (RenditionType == Rendition::TYPEAUDIO)
        SetActiveAudioRendition(pTargetRendition);
      else if (RenditionType == Rendition::TYPEVIDEO)
        SetActiveVideoRendition(pTargetRendition);
      //set the active flag on the target rendition
      pTargetRendition->IsActive = true;

      //start downloading the target segment of the main playlist - just to kick start the alternate segment download
      Playlist::StartStreamingAsync(spPlaylist.get(), targetSeg->SequenceNumber, true, true, true);//wait() or get() ? 

      //return the playlist for the target rendition
      return pTargetRendition->spPlaylist;
    }
    else //nothing to do
    {
      return nullptr;
    }

    //if switching at segment boundaries


  }
}
