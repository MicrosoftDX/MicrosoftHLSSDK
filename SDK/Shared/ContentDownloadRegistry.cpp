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
#include <map>
#include <string>
#include "ContentDownloader.h"
#include "ContentDownloadRegistry.h"



using namespace std;
using namespace Microsoft::HLSClient::Private;

void ContentDownloadRegistry::Register(DefaultContentDownloader^ downloader)
{
  std::lock_guard<recursive_mutex> lock(_lockthis);

  auto pos = downloader->DownloaderID.find_first_of(L" % % ");
  if (pos != wstring::npos)
    downloader->SetDownloaderID(downloader->DownloaderID.substr(0, pos));

  auto itr = std::find_if(_registrydata.begin(), _registrydata.end(), [this, downloader](std::pair<std::wstring, DefaultContentDownloader^> itm)
  {
    auto pos = itm.first.find_first_of(L" % % ");
    return itm.first.substr(0, pos) == downloader->DownloaderID;
  });

  if (itr != _registrydata.end())
  {
    DefaultContentDownloader^ running = itr->second;
    running->CancelDownload(true);
  }

  CleanupCompleted();

  WCHAR buff[128];
  GUID id = GUID_NULL;
  CoCreateGuid(&id);
  ::StringFromGUID2(id, buff, 128);

  downloader->SetDownloaderID(downloader->DownloaderID + L" % % " + wstring(buff));
  _registrydata[downloader->DownloaderID] = downloader;
  LOG("Registering download for " << downloader->DownloaderID); 
}

void ContentDownloadRegistry::CleanupCompleted()
{
  std::lock_guard<recursive_mutex> lock(_lockthis);
  auto itr = std::find_if(_registrydata.begin(), _registrydata.end(), [this](std::pair<wstring, DefaultContentDownloader^> p)
  {
    return p.second->IsBusy == false;
  });
  while (itr != _registrydata.end())
  {
    //LOG("ContentDownloadRegistry::CleanupCompleted(): Removing completed download for " << itr->first);
    _registrydata.erase(itr->first);
    itr = std::find_if(_registrydata.begin(), _registrydata.end(), [this](std::pair<wstring, DefaultContentDownloader^> p)
    {
      return p.second->IsBusy == false;
    });
  }

}

//
//void ContentDownloadRegistry::Unregister(Microsoft::HLSClient::Private::DefaultContentDownloader^ downloader)
//{
//  std::lock_guard<recursive_mutex> lock(_lockthis);
//
//  CleanupCompleted();
//
//  if (_registrydata.find(downloader->DownloaderID) != _registrydata.end() &&
//    _registrydata[downloader->DownloaderID]->IsBusy == false)
//  {
//    _registrydata.erase(downloader->DownloaderID);
//  }
//
//
//  LOG("Unregistering download for " << downloader->DownloaderID);
//}

void ContentDownloadRegistry::Cancel(std::wstring url, bool WaitForRunningTasks)
{
  std::lock_guard<recursive_mutex> lock(_lockthis);

  auto itr = std::find_if(_registrydata.begin(), _registrydata.end(), [this, url](std::pair<std::wstring, DefaultContentDownloader^> itm)
  {
    return itm.first.substr(0, itm.first.find_first_of(L" % % ")) == url;
  });

  if (itr != _registrydata.end() && itr->second->SupportsCancellation)
  {
    //LOG("ContentDownloadRegistry::Cancel() - Cancelling download " << itr->second->DownloaderID);
    itr->second->CancelDownload(WaitForRunningTasks);
  }

  CleanupCompleted();

}

void ContentDownloadRegistry::CancelAll(bool WaitForRunningTasks)
{
  
  std::lock_guard<recursive_mutex> lock(_lockthis);

  for (auto itm : _registrydata)
  {
    if (itm.second->SupportsCancellation)
    {
      //LOG("ContentDownloadRegistry::CancelAll() - Cancelling download " << itm.second->DownloaderID);
      itm.second->CancelDownload(WaitForRunningTasks);
    }
  }



  CleanupCompleted();

}
