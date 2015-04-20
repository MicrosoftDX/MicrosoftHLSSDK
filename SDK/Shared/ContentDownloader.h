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

#pragma once



#include <string>
#include <map>
#include <memory>
#include <vector> 
#include <wtypes.h>
#include <ppltasks.h>
#include "Interfaces.h"
#include "TaskRegistry.h"
 

 
namespace Microsoft {
  namespace HLSClient {
    namespace Private
    {

      class HeuristicsManager;
      class Cookie;

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      ref class DefaultContentDownloadCompletedArgs sealed : Microsoft::HLSClient::IHLSContentDownloadCompletedArgs
      {
      private:
        Windows::Storage::Streams::IBuffer^ _buffer;
        Windows::Web::Http::HttpResponseMessage^ _data;
        bool _isSuccessStatusCode;
        Windows::Web::Http::HttpStatusCode _statusCode;
        Windows::Foundation::Uri^ _contentUri;
      public:
        DefaultContentDownloadCompletedArgs(Windows::Storage::Streams::IBuffer^ buffer, Windows::Foundation::Uri^ contentUri, Windows::Web::Http::HttpResponseMessage^ data) :
          _data(data), _buffer(buffer), _isSuccessStatusCode(false), _statusCode(Windows::Web::Http::HttpStatusCode::BadRequest), _contentUri(contentUri)
        {

        }

        DefaultContentDownloadCompletedArgs(Windows::Storage::Streams::IBuffer^ buffer, Windows::Foundation::Uri^ contentUri, bool isSuccessStatusCode, Windows::Web::Http::HttpStatusCode statusCode) :
          _data(nullptr), _buffer(buffer), _isSuccessStatusCode(isSuccessStatusCode), _statusCode(statusCode), _contentUri(contentUri)
        {

        }

        property Windows::Foundation::Uri^ ContentUri
        {
          virtual Windows::Foundation::Uri^ get()
          {
            return _contentUri;
          }
        }
        property Windows::Foundation::Collections::IMapView<Platform::String^, Platform::String^>^ ResponseHeaders
        {
          virtual Windows::Foundation::Collections::IMapView<Platform::String^, Platform::String^>^ get()
          {
            if (_data != nullptr)
              return _data->Headers->GetView();
            else
              return nullptr;
          }
        }
        property Windows::Storage::Streams::IBuffer^ Content
        {
          virtual Windows::Storage::Streams::IBuffer^ get()
          {
            return _buffer;
          }
        }
        property bool IsSuccessStatusCode
        {
          virtual bool get()
          {
            return _data != nullptr ? _data->IsSuccessStatusCode : _isSuccessStatusCode;
          }
        }

        property Windows::Web::Http::HttpStatusCode StatusCode
        {
          virtual Windows::Web::Http::HttpStatusCode get()
          {
            return _data != nullptr ? _data->StatusCode : _statusCode;
          }
        }



      };

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      ref class DefaultContentDownloadErrorArgs sealed : Microsoft::HLSClient::IHLSContentDownloadErrorArgs
      {
      private:
        Windows::Web::Http::HttpStatusCode _data;
      public:
        DefaultContentDownloadErrorArgs(Windows::Web::Http::HttpStatusCode data) : _data(data)
        {

        }

        property Windows::Web::Http::HttpStatusCode StatusCode
        {
          virtual Windows::Web::Http::HttpStatusCode get()
          {
            return _data;
          }
        }

      };

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      ref class DefaultContentDownloader sealed : public Microsoft::HLSClient::IHLSContentDownloader
      { 
      internal:

        property std::wstring DownloaderID
        {
          std::wstring get()
          {
            return _downloaderid;
          }
        }

       
         
        void SetDownloaderID(std::wstring id)
        {
          _downloaderid = id;
        }

        void SetParameters( 
          Microsoft::HLSClient::Private::HeuristicsManager *pHeuristicsManager,
          std::wstring verb = L"GET",
          std::vector<std::shared_ptr<Microsoft::HLSClient::Private::Cookie>>& cookies = std::vector<std::shared_ptr<Microsoft::HLSClient::Private::Cookie>>(),
          std::map<std::wstring, std::wstring>& headers = std::map<std::wstring, std::wstring>(),
          bool Cache = false,
          std::wstring IfModifiedSince = L"",
          std::wstring ETag = L"", bool ActiveMeasure = true);

        void SetParameters( 
          Microsoft::HLSClient::Private::HeuristicsManager *pHeuristicsManager,
          IHLSContentDownloader^ externalDownloader);

         static std::vector<BYTE> BufferToVector(Windows::Storage::Streams::IBuffer^ buffer);
         static unsigned int BufferToBlob(Windows::Storage::Streams::IBuffer^ buffer,unique_ptr<BYTE[]>& blob);
       /*  static unsigned int BufferToBlob(Windows::Storage::Streams::IBuffer^ buffer, BYTE** blob);*/

        void CancelDownload(bool WaitForCancellationCompletion = false)
        {

          LOG("Cancelling download for " << _downloaderid << ", wait = " << (WaitForCancellationCompletion ? L"TRUE" : L"FALSE"));
          _downloadtasks.CancelAll(WaitForCancellationCompletion);
         
        }
		 
      public:

        DefaultContentDownloader();

        virtual void Initialize(Platform::String^ ContentUrl);

        virtual Windows::Foundation::IAsyncAction^ DownloadAsync();

        property bool SupportsCancellation
        {
          virtual bool get() { return _externalDownloader != nullptr ? _externalDownloader->SupportsCancellation : true; }
        }

        property bool IsBusy
        {
          virtual bool get() { return _externalDownloader != nullptr ? _externalDownloader->IsBusy : _isbusy; }
        }

        virtual void Cancel()
        {
          if (_externalDownloader != nullptr && _externalDownloader->SupportsCancellation)
            _externalDownloader->Cancel();
          else
            CancelDownload();
        }

	
        virtual event Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs^>^ Completed;

        virtual event Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs^>^ Error;

        virtual event Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader^, Windows::Foundation::Uri^>^ Redirect;

      private:
        Platform::String^ _url;
        std::wstring _verb, _IfModifiedSince, _ETag;
        bool _cache;
        bool _activeMeasure;
        std::vector<std::shared_ptr<Microsoft::HLSClient::Private::Cookie>> _cookies;
        std::map<std::wstring, std::wstring> _headers;
        Microsoft::HLSClient::Private::HeuristicsManager* _pHeuristicsManager;
        std::wstring _downloaderid;
        Microsoft::HLSClient::Private::TaskRegistry<void> _downloadtasks;
        std::wstring _measureid;
        bool _isbusy;
        unsigned long long _downloadedbytecount,_heuristicsupdatebytecounter;
        Microsoft::HLSClient::IHLSContentDownloader^ _externalDownloader;
	 
        /* event Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs^>^ _Completed;
        event Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs^>^ _Error;*/
        Windows::Web::Http::HttpRequestMessage^ PrepareRequestMessage();

      };
    }
  }
} 