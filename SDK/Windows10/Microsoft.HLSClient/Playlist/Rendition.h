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
#include <vector>
#include <string>
#include <ppltasks.h>
#include "ABI\Interfaces.h"

using namespace std;
using namespace Concurrency; 

namespace Microsoft {
  namespace HLSClient {
    namespace Private {


      class Playlist;

      class Rendition
      {
      public:

        static const std::wstring TYPEAUDIO;
        static const std::wstring TYPEVIDEO;
        static const std::wstring TYPESUBTITLES;
        static const std::wstring TYPEUNKNOWN;
        std::wstring PlaylistUri;
        std::wstring Type;
        std::wstring GroupID;
        std::wstring Language;
        std::wstring Name;
        bool Default;
        bool AutoSelect;
        bool Forced;
        bool IsActive;
        std::vector<std::wstring> Characteristics;
      public:
        ///<summary>Parent playlist (variant - not master)</summary>
        Playlist *pParentPlaylist;
        ///<summary>Playlist for this rendition</summary>
        shared_ptr<Playlist> spPlaylist;
        shared_ptr<Playlist> spPlaylistRefresh;
        /// <summary>Rendition constructor</summary>
        /// <param name='tagData'>String representing the data for the tag including any attributes</param>
        /// <param name='parentPlaylist'>Pointer to the parent variant (not the master) playlist</param>
        Rendition(std::wstring& tagData, Playlist *parentPlaylist);

        /// <summary>Downloads and parses an alternate rendition playlist</summary>
        /// <param name='tcePlaylistDownloaded'>Used to signal download completion</param>
        /// <returns>Task to wait on for download completion</returns>
        task<HRESULT> DownloadRenditionPlaylistAsync(task_completion_event<HRESULT> tcePlaylistDownloaded = task_completion_event<HRESULT>());
        HRESULT OnPlaylistDownloadCompleted(std::vector<BYTE> memorycache, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args, task_completion_event<HRESULT> tcePlaylistDownloaded);
        bool IsMatching(Rendition *ren);
        static std::shared_ptr<Rendition> FindMatching(Rendition *ren, std::vector<std::shared_ptr<Rendition>> In);
      };
    }
  }
}
 