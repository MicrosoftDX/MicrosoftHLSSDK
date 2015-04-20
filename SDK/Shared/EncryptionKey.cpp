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
#include <ppltasks.h>
#include "Cookie.h"
#include "PlaylistHelpers.h"
#include "AESCrypto.h"  
#include "Playlist.h"
#include "MediaSegment.h"
#include "HLSMediaSource.h" 
#include "Cookie.h"
#include "FileLogger.h"
#include "HLSController.h"
#include "ContentDownloader.h"
#include "EncryptionKey.h"



using namespace std;
using namespace Concurrency;
using namespace Microsoft::HLSClient::Private;



///<summary> EncryptionKey constructor</summary>
///<param name='tagWithAttributes'>EXT-X-KEY tag string with attributes</param>
///<param name='pparent'>Parent (variant - not master) playlist</param>
EncryptionKey::EncryptionKey(std::wstring& tagWithAttributes, Playlist * pParent) :
KeyUri(L""),
InitializationVector(L""),
//  KeyData(nullptr),
KeyLengthInBytes(0), IsKeyformatIdentity(true), pParentPlaylist(pParent)
{

  attriblist = Helpers::ReadAttributeList(tagWithAttributes);
  
  std::wstring _method = L"";
  //extract the encryption method
  bool ret = Helpers::ReadNamedAttributeValue(attriblist, L"METHOD", _method);
  if (ret)
    Helpers::ToUpper(_method);
  //although we parse for both AES-128 and SAMPLE-AES, in this version we only support for segment encryption (i.e. AES-128)
  Method = (ret == false || (_method != L"AES-128" && _method != L"SAMPLE-AES")) ? EncryptionMethod::NOENCRYPTION : ((_method == L"AES-128") ? EncryptionMethod::AES_128 : EncryptionMethod::SAMPLE_AES);
  if (Method == NOENCRYPTION) return;
  //extract key URL
  Helpers::ReadNamedAttributeValue(attriblist, L"URI", KeyUri);
  //if KeyUri is not absolute, merge with base URI on the parent playlist to make absolute
  if (!Helpers::IsAbsoluteUri(KeyUri))
    KeyUri = Helpers::JoinUri(pParent->BaseUri, KeyUri);
  std::wstring keyformat;
  //extract keyformat - set to IDENTITY if missing(per HLS spec)
  Helpers::ReadNamedAttributeValue(attriblist, L"KEYFORMAT", keyformat);
  if (!keyformat.empty() && Helpers::ToUpper(keyformat) != L"IDENTITY")
    IsKeyformatIdentity = false;

  if (IsKeyformatIdentity)
    //get initialization vector
    Helpers::ReadNamedAttributeValue(attriblist, L"IV", InitializationVector);
}



///<summary>Downloads a key from a key URL</summary>
///<returns>Task to wait on</returns>


task<HRESULT> EncryptionKey::DownloadKeyAsync(task_completion_event<HRESULT> tceKeyDownloadCompleted)
{
 
  CHLSMediaSource * ms = this->pParentPlaylist->cpMediaSource;
 
  if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tceKeyDownloadCompleted.set(E_FAIL);
    return task<HRESULT>(tceKeyDownloadCompleted);
  } 

  std::map<wstring, wstring> headers;
  std::vector<shared_ptr<Cookie>> cookies;
  Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
  wstring url = KeyUri;
  pParentPlaylist->cpMediaSource->cpController->RaisePrepareResourceRequest(ResourceType::KEY, url, cookies, headers,&external);

  DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();

  if (external == nullptr)
  {
    downloader->Initialize(ref new Platform::String(url.data()));
    downloader->SetParameters(  nullptr, L"GET", cookies, headers);
  }
  else
  {
    downloader->Initialize(ref new Platform::String(attriblist.data()));
    downloader->SetParameters(  nullptr, external);
  }

   

  downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
    [this, tceKeyDownloadCompleted, ms](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
  {
    if ((ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED))
    {
      tceKeyDownloadCompleted.set(E_FAIL);
    }
    else
    {
      if (args->Content != nullptr && args->IsSuccessStatusCode)
      {
        std::vector<BYTE> MemoryCache = DefaultContentDownloader::BufferToVector(args->Content);
        if (MemoryCache.size() == 0)
          tceKeyDownloadCompleted.set(E_FAIL);
        else
        {
          //in case of a redirect
          KeyUri = args->ContentUri->AbsoluteUri->Data();
           
          this->OnKeyDownloadCompleted(MemoryCache, tceKeyDownloadCompleted);
        }
      }
      else
        tceKeyDownloadCompleted.set(E_FAIL);
    }
  });
  downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
    [this, tceKeyDownloadCompleted](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
  {
    tceKeyDownloadCompleted.set(E_FAIL);
  });

  downloader->DownloadAsync();
  //return a task wrapperd around the TCE that was passed in - will be signalled once download completes
  return task<HRESULT>(tceKeyDownloadCompleted);


}

HRESULT EncryptionKey::OnKeyDownloadCompleted(std::vector<BYTE> memorycache, task_completion_event<HRESULT> tceKeyDownloadCompleted)
{
  if (this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR || this->pParentPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
  {
    tceKeyDownloadCompleted.set(E_FAIL);
    return E_FAIL;
  } 
  try
  {
    cpCryptoKey = AESCrypto::GetCurrent()->GenerateKey(&(*(memorycache.begin())), (unsigned int)memorycache.size());
  }
  catch (...)
  {
    LOG(L"DownloadKeyAsync::ResponseReceived() - Processing failed :" << this->KeyUri);
    tceKeyDownloadCompleted.set(E_FAIL);
    return E_FAIL;
  }
  

  //LOGIF(cpCryptoKey != nullptr, L"DownloadKeyAsync::ResponseReceived() - Processed:" << this->KeyUri);
  tceKeyDownloadCompleted.set(S_OK);
  return S_OK;
}

 
 

///<summary>Converts a segment sequence number to an IV per HLS spec</summary>
///<param name='number'>The segment sequence number</param>
///<returns>A shared pointer to a vector of bytes representing the IV</returns>
std::shared_ptr<std::vector<BYTE>> EncryptionKey::ToInitializationVector(unsigned int number)
{
  //convert number to big endian order
  auto bigendian = std::make_shared<std::vector<BYTE>>(16);
  
  for (int i = 0; i < 4; i++)
    (*bigendian)[15 - i] = (number >> (i * 8));
  return bigendian; 
}

