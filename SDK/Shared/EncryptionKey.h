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
#include <memory>
#include <string>
#include <mutex>
#include <vector>
#include <wtypes.h>
#include <ppltasks.h>
#include <windows.security.cryptography.core.h> 

using namespace std;
using namespace Concurrency;
using namespace Microsoft::WRL;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class MediaSegment;
      class Playlist;
      ///<summary>The allowed encryption modes</summary>
      enum EncryptionMethod
      {
        NOENCRYPTION, AES_128, SAMPLE_AES
      };

      ///<summary>Type represents a key required to decrypt an encrypted  segment</summary>
      class EncryptionKey
      {
      private:

        std::recursive_mutex LockKey;
      public:
        std::wstring attriblist;
        ///<summary>Initialization vector string from the playlist</summary>
        std::wstring InitializationVector;
        EncryptionMethod Method;
        std::wstring KeyUri;
        ///<summary>Key Length</summary>
        unsigned long KeyLengthInBytes;
        ///<sumary>Initialization vector as an array of bytes</summary>
        unique_ptr<BYTE[]> IV;
        /* ///<summary> Key data as an array of bytes</summary>
         unique_ptr<BYTE[]> KeyData;*/

        Playlist * pParentPlaylist;

        bool IsKeyformatIdentity;

        Windows::Security::Cryptography::Core::CryptographicKey ^ cpCryptoKey;

        ///<summary> EncryptionKey constructor</summary>
        ///<param name='tagWithAttributes'>EXT-X-KEY tag string with attributes</param>
        ///<param name='pparent'>Parent (variant - not master) playlist</param>
        EncryptionKey(std::wstring& tagWithAttributes, Playlist * pParent);

        ///<summary>Downloads a key from a key URL</summary>
        ///<returns>Task to wait on</returns>
        task<HRESULT> DownloadKeyAsync(task_completion_event<HRESULT> tceKeyDownloadCompleted = task_completion_event<HRESULT>());

        HRESULT OnKeyDownloadCompleted(std::vector<BYTE> memorycache, task_completion_event<HRESULT> tceKeyDownloadCompleted);
        ///<summary>Converts a segment sequence number to an IV</summary>
        ///<param name='number'>The segment sequence number</param>
        ///<returns>A shared pointer to a vector of bytes representing the IV</returns>
        std::shared_ptr<std::vector<BYTE>> ToInitializationVector(unsigned int number);

        ///<summary>
        ~EncryptionKey()
        {

        }


        bool IsEqual(shared_ptr<EncryptionKey> otherKey)
        {
          if (otherKey == nullptr) return false;
          if (Method == NOENCRYPTION && otherKey->Method == NOENCRYPTION)
            return true;
          return (KeyUri == otherKey->KeyUri && 
            InitializationVector == otherKey->InitializationVector); 
        }

        bool IsEqualKeyOnly(shared_ptr<EncryptionKey> otherKey)
        {
          if (otherKey == nullptr) return false;
          if (Method == NOENCRYPTION && otherKey->Method == NOENCRYPTION)
            return true;
          return (KeyUri == otherKey->KeyUri);
        }
      };


    }
  }
} 