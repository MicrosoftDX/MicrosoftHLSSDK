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
#include <vector>
#include <ppltasks.h>
#include "Playlist.h"
#include "Cookie.h"
#include "HLSController.h"
#include "Cookie.h"  
#include "Rendition.h" 
#include "ContentDownloader.h"


using namespace std;
using namespace Concurrency;
using namespace Microsoft::HLSClient::Private;


const std::wstring Rendition::TYPEAUDIO = L"AUDIO";
const std::wstring Rendition::TYPEVIDEO = L"VIDEO";
const std::wstring Rendition::TYPESUBTITLES = L"SUBTITLES";
const std::wstring Rendition::TYPEUNKNOWN = L"UNKNOWN";


/// <summary>Rendition constructor</summary>
/// <param name='tagData'>String representing the data for the tag including any attributes</param>
/// <param name='parentPlaylist'>Pointer to the parent variant (not the master) playlist</param>
Rendition::Rendition(std::wstring& tagData,
  Playlist *parentplaylist) :
  Type(Rendition::TYPEUNKNOWN),
  GroupID(L""),
  Language(L""),
  Name(L""),
  Default(false),
  AutoSelect(false),
  Forced(false),
  PlaylistUri(L""),
  IsActive(false),
  pParentPlaylist(parentplaylist),
  spPlaylistRefresh(nullptr)
{
  //nothing to parse
  if (tagData.empty()) return;
  //read the attribute list
  std::wstring allattribs = Helpers::ReadAttributeList(tagData);

  //if the supplied uri is not absolute - combine with base uri to make absolute
  std::wstring uritemp;
  if (Helpers::ReadNamedAttributeValue(allattribs, L"URI", uritemp))
  {
    if (Helpers::IsAbsoluteUri(uritemp))
      PlaylistUri = uritemp;
    else
      PlaylistUri = Helpers::JoinUri(parentplaylist->BaseUri, uritemp);
  }

  //get rendition type
  if (!Helpers::ReadNamedAttributeValue(allattribs, L"TYPE", this->Type))
    this->Type = Rendition::TYPEUNKNOWN;
  //get group ID
  Helpers::ReadNamedAttributeValue(allattribs, L"GROUP-ID", this->GroupID);
  //get language
  Helpers::ReadNamedAttributeValue(allattribs, L"LANGUAGE", this->Language);
  //get name
  Helpers::ReadNamedAttributeValue(allattribs, L"NAME", this->Name);
  //get default flag
  Helpers::ReadNamedAttributeValue(allattribs, L"DEFAULT", this->Default);
  //get autoselect flag
  Helpers::ReadNamedAttributeValue(allattribs, L"AUTOSELECT", this->AutoSelect);
  //get forced flag
  Helpers::ReadNamedAttributeValue(allattribs, L"FORCED", this->Forced);
  //get characteristics
  Helpers::ReadNamedAttributeValue(allattribs, L"CHARACTERISTICS", ',', this->Characteristics);
}


/// <summary>Downloads and parses an alternate rendition playlist</summary>
/// <param name='tcePlaylistDownloaded'>Used to signal download completion</param>
/// <returns>Task to wait on for download completion</returns>
task<HRESULT> Rendition::DownloadRenditionPlaylistAsync(task_completion_event<HRESULT> tcePlaylistDownloaded)
{
  if (PlaylistUri.empty()) return task<HRESULT>([this](){ return E_FAIL; });

  CHLSMediaSource * ms = pParentPlaylist->cpMediaSource;

  if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tcePlaylistDownloaded.set(E_FAIL);
    return task<HRESULT>(tcePlaylistDownloaded);
  }

  std::map<std::wstring, std::wstring> headers;
  std::vector<shared_ptr<Cookie>> cookies;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;

  wstring url = PlaylistUri;
  pParentPlaylist->cpMediaSource->cpController->RaisePrepareResourceRequest(ResourceType::ALTERNATERENDITIONPLAYLIST, url, cookies, headers, &external);
  //downloader 
  DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();
  //start the async download
  downloader->Initialize(ref new Platform::String(url.data()));
  if (external == nullptr)
    downloader->SetParameters(nullptr, L"GET", cookies, headers);
  else
    downloader->SetParameters(nullptr, external);
  //attach full response handler


  downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
    [this, tcePlaylistDownloaded, ms](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
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
          this->OnPlaylistDownloadCompleted(MemoryCache, args, tcePlaylistDownloaded);
      }
      else
        tcePlaylistDownloaded.set(E_FAIL);
    }

 //   pParentPlaylist->spDownloadRegistry->Unregister(downloader);
  });

  downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
    [this, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
  {
    DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);
 //   pParentPlaylist->spDownloadRegistry->Unregister(downloader);
    tcePlaylistDownloaded.set(E_FAIL);
  });

  this->pParentPlaylist->spDownloadRegistry->Register(downloader);

  downloader->DownloadAsync();

  return task<HRESULT>(tcePlaylistDownloaded); //return task 
}
HRESULT Rendition::OnPlaylistDownloadCompleted(std::vector<BYTE> MemoryCache, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args, task_completion_event<HRESULT> tcePlaylistDownloaded)
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
  
  if (spPlaylist == nullptr)
  {
    //create the playlist 
    spPlaylist = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename, this);
    std::lock_guard<std::recursive_mutex> lockmerge(spPlaylist->LockMerge);
    //associate the MF MediaSource
    spPlaylist->AttachMediaSource(this->pParentPlaylist->cpMediaSource);
    spPlaylist->SetLastModifiedSince(lastmod, etag);
    //parse
    spPlaylist->Parse();
  }
  else
  {
    std::lock_guard<std::recursive_mutex> lockmerge(spPlaylist->LockMerge);
    spPlaylistRefresh = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename, this); 
    spPlaylist->SetLastModifiedSince(lastmod, etag);
    if (spPlaylist->szData !=  spPlaylistRefresh->szData)
      spPlaylistRefresh->Parse();
  }
  tcePlaylistDownloaded.set(S_OK);
  return S_OK;
}

 

std::shared_ptr<Rendition> Rendition::FindMatching(Rendition *ren, std::vector<std::shared_ptr<Rendition>> In)
{
  auto found = std::find_if(In.begin(), In.end(), [ren](shared_ptr<Rendition> spRen)
  {
    return (ren->GroupID == spRen->GroupID &&
      ren->Name == spRen->Name &&
      ren->AutoSelect == spRen->AutoSelect &&
      ren->Language == spRen->Language &&
      ren->Default == spRen->Default &&
      ren->Forced == spRen->Forced &&
      ren->Type == spRen->Type  &&
      ren->PlaylistUri == spRen->PlaylistUri);
  });

  if (found == In.end()) return nullptr;
  else
    return (*found);
}

bool Rendition::IsMatching(Rendition *ren)
{
  return (ren->GroupID == GroupID &&
    ren->Name == Name &&
    ren->AutoSelect == AutoSelect &&
    ren->Language == Language &&
    ren->Default == Default &&
    ren->Forced == Forced &&
    ren->Type == Type  &&
    ren->PlaylistUri == PlaylistUri);
}
