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
#include "Playlist\Playlist.h" 
#include "HLSPlaylistHandler.h"
#include "Downloader\ContentDownloader.h"
 
using namespace Microsoft::HLSClient; 

const wstring Microsoft::HLSClient::CHLSPlaylistHandler::SchemeMSHLS = L"MS-HLS:";
const wstring Microsoft::HLSClient::CHLSPlaylistHandler::SchemeMSHLSS = L"MS-HLS-S:";

IFACEMETHODIMP Microsoft::HLSClient::CHLSPlaylistHandler::BeginCreateObject(LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps,
  IUnknown **ppIUnknownCancelCookie, IMFAsyncCallback *pCallback, IUnknown *punkState)
{

  //we only support the following schemes ms-hls: (==http:) and ms-hls-s:(==https:)
  wstring url = pwszURL;
  auto colonpos = url.find(':', 0);
  if (colonpos == wstring::npos || colonpos == url.size() - 1) return MF_E_UNSUPPORTED_BYTESTREAM_TYPE;
  wstring curscheme = Helpers::ToUpper(url.substr(0, colonpos + 1));

  if (curscheme == SchemeMSHLS)
    url = url.replace(0, colonpos + 1, L"http:");
  else if (curscheme == SchemeMSHLSS)
    url = url.replace(0, colonpos + 1, L"https:");
  else
    return MF_E_UNSUPPORTED_BYTESTREAM_TYPE;

  wstring urltrimmed = url;
  auto foundControllerID = urltrimmed.find(L"##");
  if (foundControllerID != wstring::npos)
  {
    urltrimmed = urltrimmed.substr(0, foundControllerID);
  }

  if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) == MF_RESOLUTION_MEDIASOURCE)
  {
    return E_ACCESSDENIED;
  }
  else if ((dwFlags & MF_RESOLUTION_BYTESTREAM) == MF_RESOLUTION_BYTESTREAM)
  {
    ComPtr<IMFAsyncResult> spAsyncResult;

    //try to acquire the resource
    HRESULT hr = S_OK;
    std::shared_ptr<Playlist> spRootPlaylist;
    DefaultContentDownloader^ pDownloader = ref new DefaultContentDownloader();
    wstring playlistdata = L"";
    try{
      hr = Playlist::DownloadPlaylistAsync(cpControllerFactory, pDownloader, urltrimmed, spRootPlaylist).get();
      if (FAILED(hr)) 
        return E_ACCESSDENIED;

      playlistdata = spRootPlaylist->szData;
      spRootPlaylist->Parse();
      if (spRootPlaylist->IsValid == false || (spRootPlaylist->IsVariant == false && spRootPlaylist->Segments.size() == 0))
        return MF_E_UNSUPPORTED_BYTESTREAM_TYPE;

    }
    catch (...)
    {
      return E_ACCESSDENIED;
    }
    //create the callback for the media source to call back on upon completing initialization
    if (FAILED(::MFCreateAsyncResult(nullptr, pCallback, punkState, &spAsyncResult)))
      return E_FAIL;
    if (FAILED(WRL::MakeAndInitialize<CHLSDummyByteStream>(&cpByteStream, mimetype.c_str(), (spRootPlaylist->BaseUri + spRootPlaylist->FileName).data(), playlistdata)))
      return E_FAIL;

    return MFInvokeCallback(spAsyncResult.Get());
  }
  else
    return E_FAIL;

}
