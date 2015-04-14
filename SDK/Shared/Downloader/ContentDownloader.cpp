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
#include "Adaptive\AdaptiveHeuristics.h"
#include "Playlist\Cookie.h"
#include <ppltasks.h>
#include <wrl.h>
#include <robuffer.h>
#include "Utilities\TaskRegistry.h"
#include "ContentDownloader.h"


#include <windows.web.http.h>
#include <windows.web.http.headers.h>
#include <windows.web.http.filters.h>
#include <windows.storage.streams.h>

using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;
using namespace std;
using namespace Concurrency; 
using namespace Windows::Web::Http;
using namespace Windows::Web::Http::Headers;
using namespace Windows::Web::Http::Filters;
using namespace Windows::Foundation;
using namespace Windows::Storage::Streams;
using namespace Microsoft::WRL;



DefaultContentDownloader::DefaultContentDownloader() :
_pHeuristicsManager(nullptr),
_cache(false),
_verb(L"GET"),
_isbusy(false),
_externalDownloader(nullptr),
_activeMeasure(true),
_downloadedbytecount(0), _heuristicsupdatebytecounter(0)
{

}
void DefaultContentDownloader::Initialize(Platform::String^ contenturl)
{
  _url = contenturl;

}

void DefaultContentDownloader::SetParameters(
  HeuristicsManager *pHeuristicsManager,
  wstring verb,
  vector<shared_ptr<Cookie>>& cookies,
  map<wstring, wstring>& headers,
  bool Cache,
  wstring IfModifiedSince,
  wstring ETag,
  bool ActiveMeasure)
{

  _pHeuristicsManager = pHeuristicsManager;
  _verb = verb;
  _cookies = cookies;
  _headers = headers;
  _cache = Cache;
  _IfModifiedSince = IfModifiedSince;
  _ETag = ETag;
  _activeMeasure = ActiveMeasure;
  if (_headers.find(L"Range") != _headers.end())
  {
    wostringstream tag;
    tag << _url->Data() << L"@" << _headers[L"Range"];
    tag.flush();
    _downloaderid = tag.str();
  }
  else
    _downloaderid = _url->Data();

}

void DefaultContentDownloader::SetParameters(
  HeuristicsManager *pHeuristicsManager,
  IHLSContentDownloader^ externalDownloader)
{

  _pHeuristicsManager = pHeuristicsManager;
  _externalDownloader = externalDownloader;
  _downloaderid = _url->Data();
  if (_externalDownloader != nullptr)
    _externalDownloader->Initialize(_url);
}

HttpRequestMessage^ DefaultContentDownloader::PrepareRequestMessage()
{
  HttpRequestMessage^ requestMessage = ref new HttpRequestMessage(ref new HttpMethod(ref new Platform::String(_verb.data())), ref new Windows::Foundation::Uri(_url));

  if (_cookies.size() > 0)
  {
    auto cookies = requestMessage->Headers->Cookie;
  }

  if (_headers.size() > 0)
  {
    auto headers = requestMessage->Headers;
    for (auto pair : _headers)
    {
      headers->Append(ref new Platform::String(pair.first.data()), ref new Platform::String(pair.second.data()));
    }
  }
  if (_IfModifiedSince.empty() == false)
  {
    requestMessage->Headers->Insert(ref new Platform::String(L"If-Modified-Since"), ref new Platform::String(_IfModifiedSince.data()));
    if (_ETag.empty() == false)
      requestMessage->Headers->Insert(ref new Platform::String(L"If-None-Match"), ref new Platform::String(_ETag.data()));
  }
  else if (!_cache)
  {
    requestMessage->Headers->Insert(ref new Platform::String(L"Cache-Control"), ref new Platform::String(L"no-cache"));
    requestMessage->Headers->Insert(ref new Platform::String(L"Pragma"), ref new Platform::String(L"no-cache"));
    requestMessage->Headers->Insert(ref new Platform::String(L"If-Modified-Since"), ref new Platform::String(L"Tue, 01 Jan 1901 00:00:00 GMT"));
  }



  return requestMessage;

}

std::vector<BYTE> DefaultContentDownloader::BufferToVector(IBuffer^ buffer)
{
  ComPtr<IBufferByteAccess> cpbufferbytes;
  std::vector<BYTE> MemoryCache;
  if (SUCCEEDED(reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&cpbufferbytes))))
  {

    MemoryCache.resize(buffer->Length);
    BYTE* tmpbuff = nullptr;
    cpbufferbytes->Buffer(&tmpbuff);
    memcpy_s(&(*(MemoryCache.begin())), buffer->Length, tmpbuff, buffer->Length);

    cpbufferbytes.Reset();
  }

  return MemoryCache;
}


unsigned int DefaultContentDownloader::BufferToBlob(Windows::Storage::Streams::IBuffer^ buffer, unique_ptr<BYTE[]>& blob)
{
  ComPtr<IBufferByteAccess> cpbufferbytes;

  if (SUCCEEDED(reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&cpbufferbytes))))
  {

    BYTE* tmpbuff = nullptr;
    if (SUCCEEDED(cpbufferbytes->Buffer(&tmpbuff)))
    {
      blob.reset(new BYTE[buffer->Length]);
      memcpy_s(blob.get(), buffer->Length, tmpbuff, buffer->Length); 
    }
    
    cpbufferbytes.Reset();
  }

  return buffer->Length;
}

//unsigned int DefaultContentDownloader::BufferToBlob(Windows::Storage::Streams::IBuffer^ buffer, BYTE** blob)
//{
//  ComPtr<IBufferByteAccess> cpbufferbytes;
//
//  if (SUCCEEDED(reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&cpbufferbytes))))
//  {
//
//    BYTE* tmpbuff = nullptr;
//    if (SUCCEEDED(cpbufferbytes->Buffer(&tmpbuff)))
//    {
//      *blob = new BYTE[buffer->Length];
//      memcpy_s(*blob, buffer->Length, tmpbuff, buffer->Length);
//    }
//
//    cpbufferbytes.Reset();
//  }
//
//  return buffer->Length;
//}


IAsyncAction^ DefaultContentDownloader::DownloadAsync()
{

  if (_externalDownloader == nullptr)
  {
    auto requestmessage = PrepareRequestMessage();
    HttpBaseProtocolFilter^ filter = ref new HttpBaseProtocolFilter();
    //  filter->AllowAutoRedirect = false;
    HttpClient^ client = ref new HttpClient(filter);

    if (!_cache)
    {
      filter->CacheControl->ReadBehavior = HttpCacheReadBehavior::MostRecent;
      filter->CacheControl->WriteBehavior = HttpCacheWriteBehavior::NoCache;
    }
    else
    {
      filter->CacheControl->ReadBehavior = HttpCacheReadBehavior::Default;
      filter->CacheControl->WriteBehavior = HttpCacheWriteBehavior::Default;
    }

    if (_cookies.size() > 0)
    {
      auto cookieManager = filter->CookieManager;
      for (auto itm : _cookies)
      {
        HttpCookie^ cookie = ref new HttpCookie(ref new Platform::String(itm->Name.data()), requestmessage->RequestUri->Domain, nullptr);
        if (itm->nExpiration != 0)
        {
          DateTime expires = { (long long)itm->nExpiration };
          cookie->Expires = ref new Platform::Box<DateTime>(expires);
        }
        cookie->Value = ref new Platform::String(itm->Value.data());
        cookieManager->SetCookie(cookie);
      }
    }

    cancellation_token_source currenttokensrc;
    auto currenttoken = currenttokensrc.get_token(); 
    this->_isbusy = true;

    task_completion_event<void> tceDownloadCompleted = task_completion_event<void>();

    auto op = client->SendRequestAsync(requestmessage, HttpCompletionOption::ResponseContentRead);

    if (_pHeuristicsManager != nullptr && Configuration::GetCurrent()->EnableBitrateMonitor)
    {
      op->Progress = ref new AsyncOperationProgressHandler<Windows::Web::Http::HttpResponseMessage^, Windows::Web::Http::HttpProgress>(
        [this, currenttoken](IAsyncOperationWithProgress<Windows::Web::Http::HttpResponseMessage^, Windows::Web::Http::HttpProgress>^ asyncinfo, Windows::Web::Http::HttpProgress prog)
      {
        if (currenttoken.is_canceled())
          return;
        switch (prog.Stage)
        {
        case Windows::Web::Http::HttpProgressStage::ReceivingHeaders:

          if (_pHeuristicsManager != nullptr) {
            _measureid = _pHeuristicsManager->StartDownloadMeasure(_activeMeasure);
          }
          break;

        case Windows::Web::Http::HttpProgressStage::ReceivingContent:
          if (_pHeuristicsManager != nullptr) {
            _pHeuristicsManager->UpdateDownloadMeasure(_measureid, prog.BytesReceived, _activeMeasure);
          }
          break;
        default:
          break;
        }
      });
    }


    _downloadtasks.Register(create_task(op, task_options(currenttoken)).
      then([this, tceDownloadCompleted, currenttoken](HttpResponseMessage^ response)
    {
      CHKTASK(currenttoken)

        response->EnsureSuccessStatusCode();
      {
        IBuffer^ buffer = create_task(response->Content->ReadAsBufferAsync(), task_options(currenttoken)).get();
        
        CHKTASK(currenttoken)

          if (buffer == nullptr || buffer->Length == 0)
            throw ref new Platform::NullReferenceException();
          else
          {
            if (_pHeuristicsManager != nullptr && !_measureid.empty())
              _pHeuristicsManager->CompleteDownloadMeasure(_measureid, false, _activeMeasure);

            CHKTASK(currenttoken);

            Completed(this, ref new DefaultContentDownloadCompletedArgs(buffer, response->RequestMessage->RequestUri, response));
            response = nullptr;

            LOG("Download Completed : " << this->DownloaderID);
          }
      }

    }, task_options(currenttoken, task_continuation_context::use_current())).
      then([this, tceDownloadCompleted](task<void> t) //error handler
    {
      try
      {

        t.get();
      }
      catch (task_canceled tc)
      {
        if (_pHeuristicsManager != nullptr && !_measureid.empty())
          _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);

        Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::Ok));
        LOG("Download Task Canceled : " << this->DownloaderID);
      }
      catch (Platform::COMException^ comex)
      {
        if (_pHeuristicsManager != nullptr && !_measureid.empty())
          _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);
        Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::BadRequest));
        LOG("Download Error : " << comex->Message->Data() << " [ " << this->DownloaderID << " ] ");
      }
      catch (...)
      {
        if (_pHeuristicsManager != nullptr && !_measureid.empty())
          _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);
        Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::BadRequest));
        LOG("Download Error : " << this->DownloaderID);
      }
      this->_isbusy = false;

      tceDownloadCompleted.set();
    }, task_options(currenttoken, task_continuation_context::use_current())), currenttokensrc);

    return create_async([tceDownloadCompleted, currenttoken](){
      return task<void>(tceDownloadCompleted, task_options(currenttoken, task_continuation_context::use_default())); });
  }
  else
  {
if (_pHeuristicsManager != nullptr && Configuration::GetCurrent()->EnableBitrateMonitor) 
    _measureid = _pHeuristicsManager->StartDownloadMeasure(_activeMeasure);

    _externalDownloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
      [this](Microsoft::HLSClient::IHLSContentDownloader^ downloader, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs^ args)
    {
      auto buffer = args->Content;
      auto statuscode = args->StatusCode;
      if (buffer == nullptr)
      {
        if (args->IsSuccessStatusCode == false)
        {
          if (_pHeuristicsManager != nullptr && !_measureid.empty())
            _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);
          Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::Ok));
        }
        else
        {
          if (_pHeuristicsManager != nullptr && !_measureid.empty())
            _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);
          Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::BadRequest));
        }
      }


      if (_pHeuristicsManager != nullptr && !_measureid.empty())
		{
		_pHeuristicsManager->UpdateDownloadMeasure(_measureid, buffer->Length, _activeMeasure);
        _pHeuristicsManager->CompleteDownloadMeasure(_measureid, false, _activeMeasure);
		}
      Completed(this, ref new DefaultContentDownloadCompletedArgs(buffer, args->ContentUri, args->IsSuccessStatusCode, args->StatusCode));


    });

    _externalDownloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
      [this](Microsoft::HLSClient::IHLSContentDownloader^ downloader, Microsoft::HLSClient::IHLSContentDownloadErrorArgs^ args)
    {


      if (_pHeuristicsManager != nullptr && !_measureid.empty())
        _pHeuristicsManager->CompleteDownloadMeasure(_measureid, true);
      Error(this, ref new DefaultContentDownloadErrorArgs(HttpStatusCode::BadRequest));


    });

    return _externalDownloader->DownloadAsync();
  }

}




