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
#include <vector>
#include <map>
#include <memory> 

#include "Interfaces.h" 
#include "Playlist\Cookie.h"

 
using namespace std;
using namespace Microsoft::HLSClient::Private;

namespace Microsoft {
  namespace HLSClient {

    [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
    [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
    public ref class HLSControllerFactory sealed : public Microsoft::HLSClient::IHLSControllerFactory
    {
    internal:
      void RaisePrepareResourceRequest(ResourceType restype,
        wstring& szUrl,
        std::vector<shared_ptr<Cookie>>& cookies,
        std::map<wstring,
        wstring>& headers,
        IHLSContentDownloader^* externaldownloader);
      void RaiseControllerReady(IHLSController^ pController);
      unsigned int _prepareresrequestsubscriptioncount;
      event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSResourceRequestEventArgs^>^ _prepareResourceRequest;
      event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>^ _controllerReady;
    public:

      HLSControllerFactory() : _prepareresrequestsubscriptioncount(0)
      {
      }

      bool IsPrepareResourceRequestSubscribed() { 
        return _prepareresrequestsubscriptioncount > 0; 
      }
      virtual event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSResourceRequestEventArgs^>^ PrepareResourceRequest
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSResourceRequestEventArgs^>^ handler)
        {
          InterlockedIncrement(&_prepareresrequestsubscriptioncount);
          return _prepareResourceRequest += handler;
        }
        void remove(Windows::Foundation::EventRegistrationToken token)
        {
          InterlockedDecrement(&_prepareresrequestsubscriptioncount);
          _prepareResourceRequest -= token;
        }
        void raise(IHLSControllerFactory^ sender, IHLSResourceRequestEventArgs^ args)
        { 
          return _prepareResourceRequest(sender, args);
        }
      }
      virtual event Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>^ HLSControllerReady
      {
        Windows::Foundation::EventRegistrationToken add(Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>^ handler)
        {
          return _controllerReady += handler;
        }
        void remove(Windows::Foundation::EventRegistrationToken token)
        {
          _controllerReady -= token;
        }
        void raise(IHLSControllerFactory^ sender, IHLSController^ args)
        {
          return _controllerReady(sender, args);
        }
      }

    };
  }
}  