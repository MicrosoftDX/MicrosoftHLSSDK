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

#include <algorithm> 
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ppltasks.h> 
#include <collection.h>
#include "Interfaces.h"
#include "Playlist\Cookie.h"  
#include "HLSHttpHeaderEntry.h" 

using namespace Concurrency;
using namespace Platform::Collections;
using namespace Microsoft::HLSClient;

 
namespace Microsoft {
  namespace HLSClient { 
    ref class HLSControllerFactory;
    
    namespace Private {

      ref class HLSController;
      
      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSResourceRequestEventArgs sealed : public IHLSResourceRequestEventArgs
      {
        friend ref class HLSController;
        friend ref class Microsoft::HLSClient::HLSControllerFactory;
      private:
        ResourceType _type;
        std::wstring _resourceUrl;
        std::map<std::wstring, std::wstring> _headers;
        std::vector<std::shared_ptr<Cookie>> _cookies;
        task_completion_event<void> _tceSubmitted;
        Microsoft::HLSClient::IHLSContentDownloader^ _externalDownloader;
      internal:
        HLSResourceRequestEventArgs(ResourceType resType, std::wstring url, std::vector<std::shared_ptr<Cookie>>& cookies, std::map<std::wstring, std::wstring>& Headers) :
          _resourceUrl(url), _headers(Headers), _cookies(cookies), _type(resType)
        {
        }

        IHLSContentDownloader^ GetDownloader()
        {
          return _externalDownloader;
        }

      public:

        property ResourceType Type
        {
          virtual ResourceType get()
          {
            return _type;
          }
        }

       
        property Platform::String^ ResourceUrl
        {
          virtual Platform::String^ get()
          {
            return ref new Platform::String(_resourceUrl.data());
          }
          virtual void set(Platform::String^ val)
          {
            _resourceUrl = (val == nullptr ? L"" : val->Data());
          }
        }

        virtual void SetDownloader(IHLSContentDownloader^ downloader)
        {
          _externalDownloader = downloader;
        }
        virtual Windows::Foundation::Collections::IVector<IHLSHttpHeaderEntry^>^ GetHeaders()
        {
          auto retval = ref new Vector<IHLSHttpHeaderEntry^>((unsigned int)_headers.size());

          std::transform(_headers.begin(), _headers.end(), begin(retval), [](std::pair<std::wstring, std::wstring> mapentry) {
            return ref new HLSHttpHeaderEntry(mapentry.first, mapentry.second);
          });

          return retval;
        }

        virtual Windows::Foundation::Collections::IVector<IHLSHttpHeaderEntry^>^ GetCookies()
        {
          auto retval = ref new Vector<IHLSHttpHeaderEntry^>((unsigned int)_cookies.size());

          std::transform(_cookies.begin(), _cookies.end(), begin(retval), [](std::shared_ptr<Cookie> spcookie) {
            return ref new HLSHttpHeaderEntry(spcookie->Name, spcookie->Value);
          });

          return retval;
        }

        virtual void SetHeader(Platform::String^ key, Platform::String^ value)
        {

          if (key == nullptr) throw ref new Platform::InvalidArgumentException();
          if (value == nullptr) throw ref new Platform::InvalidArgumentException();
          std::wstring k = key->Data();
          std::wstring v = value->Data();
          if (Helpers::ToUpper(k) == L"COOKIE") throw ref new Platform::InvalidArgumentException();

          if (_headers.find(k) != _headers.end())
          {
            _headers[k] = v;
          }
          else
          {
            _headers.emplace(std::pair<std::wstring, std::wstring>(k, v));
          }

          return;
        }

        virtual Platform::String^ RemoveHeader(Platform::String^ key)
        {
          Platform::String^ retval = nullptr;
          if (key == nullptr) throw ref new Platform::InvalidArgumentException();
          std::wstring k = key->Data();
          if (Helpers::ToUpper(k) == L"COOKIE") throw ref new Platform::InvalidArgumentException();

          if (_headers.find(k) != _headers.end())
          {
            if (_headers[k].empty() == false)
              retval = ref new Platform::String(_headers[k].data());
            _headers.erase(k);
          }
          return retval;
        }

        virtual void SetCookie(Platform::String^ key, Platform::String^ value, Windows::Foundation::TimeSpan ExpirationUTCTime, bool IsHttpOnly, bool IsPersistent)
        {
          if (key == nullptr || value == nullptr) throw ref new Platform::InvalidArgumentException();

          std::wstring k = key->Data();
          std::wstring v = value->Data();


          auto found = std::find_if(_cookies.begin(), _cookies.end(), [k](std::shared_ptr<Cookie> ck) { return ck->Name == k; });
          if (found != _cookies.end())
          {
            (*found)->Value = v;
            (*found)->nExpiration = (unsigned long long )ExpirationUTCTime.Duration;
            (*found)->IsHttpOnly = IsHttpOnly;
            (*found)->Persistent = IsPersistent;
          }
          else
          {
            auto c = make_shared<Cookie>();
            c->Name = k;
            c->Value = v;
            c->nExpiration = (unsigned long long)ExpirationUTCTime.Duration;
            c->IsHttpOnly = IsHttpOnly;
            c->Persistent = IsPersistent;
            _cookies.push_back(c);
          }

          return;
        }

        virtual Platform::String^ RemoveCookie(Platform::String^ key)
        {
          Platform::String^ retval = nullptr;
          if (key == nullptr) throw ref new Platform::InvalidArgumentException();
          auto found = std::find_if(begin(_cookies), end(_cookies), [key](std::shared_ptr<Cookie> spCookie)
          {
            return spCookie->Name == key->Data();
          });
          if (found != end(_cookies))
          {
            retval = ref new Platform::String((*found)->Name.data());
          }
          return retval;
        }

        virtual void Submit()
        {
          _tceSubmitted.set();
          return;
        }
      };
    }
  }
} 