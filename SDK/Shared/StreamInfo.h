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
#include <ppltasks.h>
#include <functional>   
#include <memory>
#include <map>
#include <mutex>
#include "Interfaces.h"

using namespace std;
using namespace Concurrency;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {



      class Playlist;
      class Rendition;
      class ContentDownloadRegistry;

      class StreamInfo
      {

      public:
        std::shared_ptr<ContentDownloadRegistry> spDownloadRegistry;
        //Is variant active ?
        bool IsActive;
        //IS resolution info present ?
        bool HasResolution;
        //Do we have program ID ?
        bool HasProgramID;
        //Do we have an alternate audio rendition associated ? 
        bool HasAudioRenditionGroup;
        //Do we have an alternate video rendition associated ? 
        bool HasVideoRenditionGroup;
        //Do we have a subtitle rendition associated ? 
        bool HasSubtitlesRenditionGroup;
        //Bitrate
        unsigned int Bandwidth;
        //Horizontal Resolution
        unsigned int HorizontalResolution;
        //Vertical Resolution
        unsigned int VerticalResolution;
        //Program ID 
        unsigned int ProgramID;
        //MF media subtypes
        GUID VideoMediaType, AudioMediaType;
        //Codecs string
        std::wstring Codecs;
        //Audio rendition group (if present)
        std::wstring AudioRenditionGroupID;
        //Video rendition group (if present)
        std::wstring VideoRenditionGroupID;
        //Subtitle rendition group (if present)
        std::wstring SubtitlesRenditionGroupID;
        //Variant Playlist URL
        std::wstring PlaylistUri;
        std::vector<wstring> BackupPlaylistUris;
        //Variant playlist instance
        shared_ptr<Playlist> spPlaylist;
        shared_ptr<Playlist> spPlaylistRefresh;
        std::map<wstring, shared_ptr<Playlist>> BackupPlaylists;
        //Collection of associated alternate audio renditions
        shared_ptr<std::vector<std::shared_ptr<Rendition>>> AudioRenditions;
        //Collection of associated alternate video renditions
        shared_ptr<std::vector<std::shared_ptr<Rendition>>> VideoRenditions;
        //Collection of associated subtitle renditions
        shared_ptr<std::vector<std::shared_ptr<Rendition>>> SubtitleRenditions;
        //Parent (master ) playlist
        Playlist *pParentPlaylist;
        recursive_mutex LockStream;
        std::map<std::wstring, shared_ptr<Playlist>> BatchItems;
     
        
      private:
        const unsigned int MAXALLOWEDDOWNLOADFAILURES = 4;
        const unsigned int DOWNLOADFAILUREQUARANTINEDURATION = 30000;
        unsigned int DownloadFailureCount;
        ULONGLONG FailureCountMeasureTimestamp;
        //active alternate renditions
        Rendition  *pActiveAudioRendition, *pActiveVideoRendition;
        /// <summary>Parses the codec string and translates to matching media foundation media subtypes</summary>
        /// <remarks>We only support H.264 and AAC in this version. In case no codec string is supplied we assume H.264 and AAC.</remarks>
        /// <param name='codecstring'>The codec string extracted from the playlist</param>
        void ParseCodecsString(std::wstring& codecstring); 
        HRESULT OnPlaylistDownloadCompleted(std::vector<BYTE> MemoryCache, task_completion_event<HRESULT> tcePlaylistDownloaded);
        HRESULT OnBatchPlaylistDownloadCompleted(
          shared_ptr<Playlist>& spPlaylist, 
          Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args, 
          std::vector<BYTE> MemoryCache, 
          task_completion_event<HRESULT> tcePlaylistDownloaded);
      public:

        ~StreamInfo();

        bool IsQuarantined();
        /// <summary>Activates or deactivates current variant</summary>
        /// <param name='Active'>If true, activates the variant, else if false deactivates the variant</param>
        /// <returns>Task to wait on</returns>
        void MakeActive(bool Active = true);

        /// <summary>StreamInfo constructor</summary>
        /// <param name='tagWithAttributes'>The EXT-X-STREAM-INF tag with the attributes</param>
        /// <param name='playlisturi'>The uri for the playlist for this variant</param> 
        /// <param name='parentPlaylist'>The parent (master) playlist</param>
        StreamInfo(std::wstring& tagWithAttributes, std::wstring& playlisturi, Playlist *parentPlaylist);

        /// <summary>Downloads and parses variant playlist</summary>
        /// <param name='tcePlaylistDownloaded'>Task completion event that is used to create a waitable task to be returned</param>
        /// <returns>Task to wait on</returns>
        task<HRESULT> DownloadPlaylistAsync(task_completion_event<HRESULT> tcePlaylistDownloaded = task_completion_event<HRESULT>());
        task<HRESULT> DownloadBatchPlaylist(shared_ptr<Playlist>& pPlaylist, wstring& uri, task_completion_event<HRESULT> tcePlaylistDownloaded = task_completion_event<HRESULT>());


        /// <summary>Prepare the stream for a switch to/from an alternate rendition</summary>
        /// <param name="RenditionType">The type of rendition we are switching to</param>
        /// <param name="pTargetRendition">The rendition instance we are switching to - can be nullptr if we are switching back to the main track</param>
        /// <returns>Returns the target playlist</returns>
        shared_ptr<Playlist> PrepareRenditionSwitch(std::wstring RenditionType, Rendition *pTargetRendition);

        Rendition *GetActiveAudioRendition()
        {
          std::lock_guard<std::recursive_mutex> lock(LockStream);
          return pActiveAudioRendition;
        }
        Rendition *GetActiveVideoRendition()
        {
          std::lock_guard<std::recursive_mutex> lock(LockStream);
          return pActiveVideoRendition;
        }

        void SetActiveAudioRendition(Rendition *ptr)
        {
          std::lock_guard<std::recursive_mutex> lock(LockStream);
          pActiveAudioRendition = ptr;
        }

        void SetActiveVideoRendition(Rendition *ptr)
        {
          std::lock_guard<std::recursive_mutex> lock(LockStream);
          pActiveVideoRendition = ptr;
        }

        void AddBackupPlaylistUri(wstring uri);
      };

      

    }
  }
} 