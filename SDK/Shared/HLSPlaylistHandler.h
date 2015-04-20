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

#include "pch.h" 
#include <synchapi.h>
#include <windows.media.h>  
#include <windows.storage.h>  
#include "HLSMediaSource.h"
#include "HLSController.h"
#include "HLSControllerFactory.h" 
#include "Playlist.h"
#include "HLSDummyByteStream.h"
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {  

      ///<summary>Byte stream handler to handle HLS playlists</summary>
      ///<remarks>The byte stream handler activatable type is registered using the media extension manager in the player. 
      //Media Foundation invokes BeginCreateObject() when an HLS playlist is set as a source on either a MediaElement or a HTML5 <video> tag </remarks>
      
      class DECLSPEC_UUID("691742C1-A2EC-47ED-957B-755B9AECE58E") CHLSPlaylistHandler : 
      public RuntimeClass<
        RuntimeClassFlags<RuntimeClassType::WinRtClassicComMix>,
        ABI::Windows::Media::IMediaExtension, 
        IMFByteStreamHandler, 
        IMFSchemeHandler, 
        FtmBase>
      {

        InspectableClass(L"Microsoft.HLSClient.HLSPlaylistHandler", BaseTrust);
      private:

        HLSControllerFactory^ cpControllerFactory;
        ComPtr<CHLSMediaSource> spMediaSource;
        ComPtr<CHLSDummyByteStream> cpByteStream;
        wstring mimetype;
        HSTRING *tunnelto;
      public:


        static const wstring SchemeMSHLS;
        static const wstring SchemeMSHLSS;
        IFACEMETHOD(BeginCreateObject)(LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps,
          IUnknown **ppIUnknownCancelCookie, IMFAsyncCallback *pCallback, IUnknown *punkState);
        ///<summary>See IMFByteStreamHandler in MSDN</summary>
        IFACEMETHODIMP BeginCreateObject(IMFByteStream *pByteStream, LPCWSTR pwszURL, DWORD dwFlags, IPropertyStore *pProps,
          IUnknown **ppIUnknownCancelCookie, IMFAsyncCallback *pCallback, IUnknown *punkState)
        {

          HRESULT hr = S_OK;
          //check for valid parameters

          if ((dwFlags & MF_RESOLUTION_MEDIASOURCE) != MF_RESOLUTION_MEDIASOURCE || pCallback == nullptr)
            return E_INVALIDARG;

          ComPtr<IMFByteStream> cpbs(pByteStream);
          ComPtr<IMFAttributes> cattribs;
          cpbs.As(&cattribs);

          wstring pldata;
          UINT32 HasPlaylistData = FALSE;
          if (SUCCEEDED(cattribs->GetUINT32(PLAYLIST_DATA, &HasPlaylistData)))
          {
            ////read the byte stream
            if (pByteStream != nullptr)
            {

              QWORD size = 0;
              pByteStream->GetLength(&size);
              if (size > 0)
              {
                pldata.resize((size_t) size);
                pByteStream->SetCurrentPosition(0);
                ULONG read = 0;
                pByteStream->Read((BYTE*) &(*(pldata.begin())), (ULONG) size, &read);

                if (read != size)
                {
                  pldata.clear();
                }
              }
            }
          }

          wstring Url = pwszURL;
          wstring ControllerID = L"";
          auto foundControllerID = Url.find(L"##");
          if (foundControllerID != wstring::npos)
          {
            ControllerID = Url.substr(foundControllerID + 2, Url.size() - (foundControllerID + 2));
            Url = Url.substr(0, foundControllerID);
          }
          ComPtr<IMFAsyncResult> spAsyncResult;
          //create the callback for the media source to call back on upon completing initialization
          if (FAILED(hr = ::MFCreateAsyncResult(nullptr, pCallback, punkState, &spAsyncResult)))
            return hr;
          //create the media source
          spMediaSource = Microsoft::WRL::Make<CHLSMediaSource>(pwszURL);
          //create a controller 
          HLSController^ spController = ref new HLSController(spMediaSource.Get(), ControllerID);
          //if the player passed in a controller factory

          //associate it with the media source (the media source will raise an event back to the player to pass back the controller)
          static_cast<CHLSMediaSource*>(spMediaSource.Get())->SetHLSController(spController);

          if (pldata.size() > 0)
            //start initializing the media source
            return spMediaSource->BeginOpen(spAsyncResult.Get(), cpControllerFactory, std::move(pldata));
          else
            return spMediaSource->BeginOpen(spAsyncResult.Get(), cpControllerFactory);
        }

        ///<summary>See IMFByteStreamHandler in MSDN</summary>
        IFACEMETHODIMP CancelObjectCreation(IUnknown *pIUnknownCancelCookie)
        {
          return E_NOTIMPL;
        }

        ///<summary>See IMFByteStreamHandler in MSDN</summary>
        IFACEMETHODIMP EndCreateObject(IMFAsyncResult *pResult, MF_OBJECT_TYPE *pObjectType, IUnknown **ppObject)
        {
          HRESULT hr = pResult->GetStatus();
          if (FAILED(hr)) return hr;
          if (spMediaSource != nullptr)
          {
            //media source is done initializing - pass it back out to MF
            if (FAILED(hr = spMediaSource.Get()->QueryInterface(IID_PPV_ARGS(ppObject))))
              return hr;
            *pObjectType = MF_OBJECT_TYPE::MF_OBJECT_MEDIASOURCE; 
          }
          else if (cpByteStream != nullptr)
          {
            //media source is done initializing - pass it back out to MF
            if (FAILED(hr = cpByteStream.Get()->QueryInterface(__uuidof(IMFByteStream), (void**) ppObject)))
              return hr;
            *pObjectType = MF_OBJECT_TYPE::MF_OBJECT_BYTESTREAM;
          }
          else
          {
            *pObjectType = MF_OBJECT_TYPE::MF_OBJECT_INVALID;
            return E_FAIL;
          }

          return S_OK;
        }

        ///<summary>See IMFByteStreamHandler in MSDN</summary>
        IFACEMETHODIMP GetMaxNumberOfBytesRequiredForResolution(QWORD *pqwBytes)
        {
          return E_NOTIMPL;
        }


        ///<summary>See IMediaExtension in MSDN</summary>
        IFACEMETHODIMP SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *props)
        {

          if (props == nullptr)
            return S_OK;
          ComPtr<ABI::Windows::Foundation::Collections::IPropertySet> spProps(props);
          ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable*>> cpMap;
          ComPtr<IInspectable> cpTemp = nullptr;
          ComPtr<IInspectable> cpTempFactory = nullptr;
          ComPtr<ABI::Windows::Foundation::IPropertyValue> cpPVMimeType = nullptr;
          boolean found = false;
          //get the property set as a map
          if (SUCCEEDED(spProps.As(&cpMap)))
          {
            //do we have a controller factory ?
            if (SUCCEEDED(cpMap->HasKey(Microsoft::WRL::Wrappers::HStringReference(L"ControllerFactory").Get(), &found)) && found)
            {
              if (SUCCEEDED(cpMap->Lookup(Microsoft::WRL::Wrappers::HStringReference(L"ControllerFactory").Get(), &cpTempFactory)))
              {
                IInspectable * pInsp = nullptr;
                if(SUCCEEDED(cpTempFactory.Get()->QueryInterface(IID_PPV_ARGS(&pInsp))))
                  cpControllerFactory = reinterpret_cast<HLSControllerFactory^>(pInsp);
              }
            }
              
            if (SUCCEEDED(cpMap->HasKey(Microsoft::WRL::Wrappers::HStringReference(L"MimeType").Get(), &found)) && found)
            {
              if (SUCCEEDED(cpMap->Lookup(Microsoft::WRL::Wrappers::HStringReference(L"MimeType").Get(), &cpTemp)))
              {
                cpTemp.As(&cpPVMimeType);
                ABI::Windows::Foundation::PropertyType type;
                if (SUCCEEDED(cpPVMimeType->get_Type(&type)) && type == ABI::Windows::Foundation::PropertyType::PropertyType_String)
                {
                  HSTRING val;
                  if (SUCCEEDED(cpPVMimeType->GetString(&val)))
                  {
                    UINT32 len = 0;
                    mimetype = WindowsGetStringRawBuffer(val, &len);
                  }
                }
              }
            }

          }


          return S_OK;
        }
        CHLSPlaylistHandler() : mimetype(L"application/x-mpegurl")
        {
        }

        ~CHLSPlaylistHandler()
        {
        }
      };




      ActivatableClass(CHLSPlaylistHandler)

    }
  }  
