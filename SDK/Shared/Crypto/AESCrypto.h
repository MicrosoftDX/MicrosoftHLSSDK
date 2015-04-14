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
#include <string>
#include <memory>
#include <windows.security.cryptography.h>
#include <windows.security.cryptography.core.h>
#include <windows.storage.h>



namespace WSC = ::Windows::Security::Cryptography;
namespace WSCC = ::Windows::Security::Cryptography::Core;
namespace WSS = ::Windows::Storage::Streams;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class AESCrypto
      {
      private:
        std::wstring algorithm_name;
        std::wstring key_provider_name;
        WSCC::ISymmetricKeyAlgorithmProvider^ algo_provider;
        WSCC::IKeyDerivationAlgorithmProvider^ key_provider;
        static std::shared_ptr<AESCrypto> _current;

        void InitializeCrypto(void)
        {
          if (nullptr == algo_provider)
            algo_provider = WSCC::SymmetricKeyAlgorithmProvider::OpenAlgorithm(ref new Platform::String(algorithm_name.data()));

          if (nullptr == key_provider)
            WSCC::KeyDerivationAlgorithmProvider::OpenAlgorithm(ref new Platform::String(key_provider_name.data()));
        }

      public:
        AESCrypto()
        {
          algorithm_name = L"AES_CBC_PKCS7";
          key_provider_name = L"PBKDF2_SHA1";
          InitializeCrypto();
        }
        static std::shared_ptr<AESCrypto> GetCurrent()
        {
          if (_current == nullptr)
            _current = std::make_shared<AESCrypto>();
          return _current;
        }


        /** ICryptographicKey* GenerateKey(BYTE* KeyVal) **/
        /** USe CryptBuffer -> CreateFromByteArray() **/

        WSCC::CryptographicKey^ GenerateKey(BYTE *stringkey, unsigned int size)
        {
          try{
            WSS::IBuffer^ source_buffer = WSC::CryptographicBuffer::CreateFromByteArray(ref new Platform::Array<BYTE>(stringkey, size));
            if (source_buffer == nullptr)
              return nullptr;
            return algo_provider->CreateSymmetricKey(source_buffer);
          }
          catch (...)
          {
            return nullptr;
          }

        }

        /** HRESULT Decrypt(ICryptographicKey* key, BYTE* encryptedData, BYTE* InitVector, BYTE** resultDecryptedData) **/
        /** USe cryptbuffer -> CopyToByteArray() **/

        Platform::Array<BYTE>^ Decrypt(WSCC::CryptographicKey^ key, BYTE* encryptedData, unsigned int datasize, BYTE* initVector, unsigned int ivsize)
        {
          WSS::IBuffer^ input_buffer, ^iv_buffer, ^result_buffer;
          try {
            input_buffer = WSC::CryptographicBuffer::CreateFromByteArray(ref new Platform::Array<BYTE>(encryptedData, datasize));
            if (input_buffer == nullptr) return nullptr;
            iv_buffer = WSC::CryptographicBuffer::CreateFromByteArray(ref new Platform::Array<BYTE>(initVector, ivsize));
            if (iv_buffer == nullptr) return nullptr;
            result_buffer = WSCC::CryptographicEngine::Decrypt(key, input_buffer, iv_buffer);
            if (result_buffer == nullptr) return nullptr;
            Platform::Array<BYTE>^ resultDecryptedData = nullptr;
            WSC::CryptographicBuffer::CopyToByteArray(result_buffer, &resultDecryptedData);
            return resultDecryptedData;
          }
          catch (...)
          {
            return nullptr;
          }
        }

        Platform::Array<BYTE>^ Decrypt(WSCC::CryptographicKey^ key, BYTE* encryptedData, unsigned int datasize, std::wstring& initVector)
        {
          WSS::IBuffer ^input_buffer, ^iv_buffer, ^result_buffer;

          //remove the 0x
          if (initVector.substr(0, 2) == L"0x" || initVector.substr(0, 2) == L"0X")
            initVector = initVector.substr(2, initVector.size() - 2);
          //we need a hex encoded that represents a 16-octet number. So we need a string with exacly 32 characters - where each pair represents the hex value of a byte(octet)
          //if we do not have that - we pad with leading zeros to leave the number unchanged but the format correct. Otherwise decoding will fail.
          if (initVector.size() < 32 != 0)
          {
            size_t diff = (size_t) 32 - initVector.size();
            for (size_t i = 0; i < diff; i++)
              initVector.insert(0, L"0");
          }

          try{
            input_buffer = WSC::CryptographicBuffer::CreateFromByteArray(ref new Platform::Array<BYTE>(encryptedData, datasize));
            if (input_buffer == nullptr) return nullptr;
            iv_buffer = WSC::CryptographicBuffer::DecodeFromHexString(ref new Platform::String(initVector.data()));
            if (iv_buffer == nullptr) return nullptr;
            result_buffer = WSCC::CryptographicEngine::Decrypt(key, input_buffer, iv_buffer);
            if (result_buffer == nullptr) return nullptr;
            Platform::Array<BYTE>^ resultDecryptedData = nullptr;
            WSC::CryptographicBuffer::CopyToByteArray(result_buffer, &resultDecryptedData);
            return resultDecryptedData;
          }
          catch (...)
          {
            return nullptr;
          }
        }
      };

    }
  }
}
 