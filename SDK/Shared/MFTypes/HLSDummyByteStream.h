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
#include <mfapi.h>
#include <mfidl.h>
#include <synchapi.h> 
#include "HLSMediaSource.h"
#include "ABI\HLSController.h" 
#include <windows.storage.h>  
#include "Playlist\Playlist.h"
#include "Playlist\Cookie.h" 
#include <wrl.h> 

using namespace Microsoft::WRL;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      // {93B4184A-6204-4480-9FFC-78866251D391}
      EXTERN_GUID(PLAYLIST_DATA,
        0x93b4184a, 0x6204, 0x4480, 0x9f, 0xfc, 0x78, 0x86, 0x62, 0x51, 0xd3, 0x91);

      class CHLSDummyByteStream : public RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IMFByteStream>
      {
        // InspectableClass(RuntimeClass_Microsoft_HLSClient_HLSDummyByteStream,BaseTrust);
      private:
        ComPtr<IMFAttributes> cpAttrStore;
        unique_ptr<BYTE[]> data;
        size_t size;
      public:
        CHLSDummyByteStream() : size(0), data(nullptr)
        {
        }
        ~CHLSDummyByteStream()
        {
          data.reset(nullptr);
          cpAttrStore.Reset();
        }
        HRESULT RuntimeClassInitialize(LPCWSTR mimeType, LPCWSTR url, wstring PlaylistData)
        {
          if (FAILED(MFCreateAttributes(&cpAttrStore, 2)))
            return E_FAIL;
          if (FAILED(cpAttrStore->SetString(MF_BYTESTREAM_CONTENT_TYPE, mimeType)))
            return E_FAIL;
          if (FAILED(cpAttrStore->SetString(MF_BYTESTREAM_ORIGIN_NAME, url)))
            return E_FAIL;
          if (FAILED(cpAttrStore->SetUINT32(PLAYLIST_DATA, TRUE)))
            return E_FAIL;

          size = PlaylistData.size() * 2 + 1; //Wide Character - double byte plus null terminator
          auto raw = new BYTE[size];
          ZeroMemory(raw, size);
          memcpy_s(raw, size, (BYTE*) PlaylistData.c_str(), size);
          data.reset(raw);

          return S_OK;
        }

        IFACEMETHODIMP QueryInterface(REFIID iid, void **ppvObject)
        {
          if (iid == __uuidof(IMFAttributes))
          {
            return cpAttrStore.Get()->QueryInterface(iid, ppvObject);
          }
          else
            return RuntimeClass<RuntimeClassFlags<RuntimeClassType::ClassicCom>, IMFByteStream>::QueryInterface(iid, ppvObject);
        }


        IFACEMETHODIMP GetCapabilities(
          DWORD *pdwCapabilities)
        {
          return MFBYTESTREAM_IS_READABLE;
        }

        IFACEMETHODIMP GetLength(
          QWORD *pqwLength)
        {
          *pqwLength = (QWORD) size;
          return S_OK;
        }

        IFACEMETHODIMP SetLength(
          QWORD qwLength)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP GetCurrentPosition(
          QWORD *pqwPosition)
        {
          return 0;
        }

        IFACEMETHODIMP SetCurrentPosition(
          QWORD qwPosition)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP IsEndOfStream(
          BOOL *pfEndOfStream)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP Read(
          BYTE *pb,
          ULONG cb,
          ULONG *pcbRead)
        {
          memcpy_s(pb, __min(cb, size), data.get(), __min(cb, size));
          *pcbRead = __min(cb, (ULONG) size);
          return S_OK;
        }

        IFACEMETHODIMP BeginRead(
          BYTE *pb,
          ULONG cb,
          IMFAsyncCallback *pCallback,
          IUnknown *punkState)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP EndRead(
          IMFAsyncResult *pResult,
          ULONG *pcbRead)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP Write(
          const BYTE *pb,
          ULONG cb,
          ULONG *pcbWritten)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP BeginWrite(
          const BYTE *pb,
          ULONG cb,
          IMFAsyncCallback *pCallback,
          IUnknown *punkState)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP EndWrite(IMFAsyncResult *pResult,
          ULONG *pcbWritten)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP Seek(
          MFBYTESTREAM_SEEK_ORIGIN SeekOrigin,
          LONGLONG llSeekOffset,
          DWORD dwSeekFlags,
          QWORD *pqwCurrentPosition)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP Flush(void)
        {
          return E_NOTIMPL;
        }

        IFACEMETHODIMP Close(void)
        {
          return S_OK;
        }


      };
    }
  }
} 