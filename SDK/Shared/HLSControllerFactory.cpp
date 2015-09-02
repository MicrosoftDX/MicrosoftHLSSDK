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
#include "HLSControllerFactory.h"
#include "HLSResourceRequestEventArgs.h" 
#include "Interfaces.h"

using namespace Concurrency;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

void Microsoft::HLSClient::HLSControllerFactory::RaisePrepareResourceRequest(ResourceType restype, 
  wstring& szUrl, 
  std::vector<shared_ptr<Cookie>>& cookies, 
  std::map<wstring, 
  wstring>& headers,
  IHLSContentDownloader^* externaldownloader)
{
  HLSResourceRequestEventArgs^ args = ref new HLSResourceRequestEventArgs(restype, szUrl, cookies, headers);
  if (args != nullptr && IsPrepareResourceRequestSubscribed())
  {
    task<void> t(args->_tceSubmitted); 
    _prepareResourceRequest(this, args);
    t.wait();
    szUrl = args->_resourceUrl;
    headers = args->_headers;
    cookies = args->_cookies;
    *externaldownloader = args->GetDownloader();
  }
}
void Microsoft::HLSClient::HLSControllerFactory::RaiseControllerReady(IHLSController^ pController)
{
  _controllerReady(this, pController);
}