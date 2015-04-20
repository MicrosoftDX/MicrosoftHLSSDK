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
#include "pch.h"

#include <math.h>  
#include <numeric>
#include <assert.h>  
#include "Cookie.h"
#include "Rendition.h"

#include "VariableRate.h"
#include "HLSController.h"
#include "HLSControllerFactory.h"
#include "HLSPlaylist.h"
#include "HLSMediaSource.h"
#include "MFAudioStream.h"
#include "MFVideoStream.h"
#include "Playlist.h"  
#include "ContentDownloader.h"
#include "ContentDownloadRegistry.h"

using namespace Microsoft::HLSClient::Private;
using namespace std;
#pragma region Playlist  





Playlist::~Playlist()
{

    LOGIIF(IsVariant, "Root Playlist Destroyed", "Child Playlist Destroyed");

    if (!IsVariant && spswPlaylistRefresh != nullptr && spswPlaylistRefresh->IsTicking)
        spswPlaylistRefresh->StopTicking();

    OnPlaylistRefresh = nullptr;

    spDownloadRegistry->CancelAll(true);

    if (IsVariant && Variants.size() > 0)
    {
        for (auto itr : Variants)
            itr.second.reset();
    }
    else
    {
        for (auto itr : Segments)
            itr.reset();
    }



}

int Playlist::NeedsPreFetch()
{
    if (IsVariant)
        return 0;
    if (Configuration::GetCurrent()->PreFetchLengthInTicks == 0 && DerivedTargetDuration < 50000000)
    {
        Configuration::GetCurrent()->PreFetchLengthInTicks = 50000000;
    }
    unsigned long long totdur = 0;
    auto prefetchdur = Configuration::GetCurrent()->PreFetchLengthInTicks;
    auto part = std::partition(Segments.begin(), Segments.end(), [this, &totdur, prefetchdur](shared_ptr<MediaSegment> s)
    {
        totdur += s->Duration;
        return totdur <= prefetchdur;
    });
    auto ret = std::distance(Segments.begin(), part);
    if (ret > 0)
    {
        LOG("Prefetch needed - Prefetch " << ret + 1 << " segments");
        return (int)ret + 1;
    }
    else
        return (int)ret;
}
std::map<ContentType, unsigned short> Playlist::GetPIDFilter()
{
    if (this->spPIDFilter == nullptr && this->IsVariant == false) //has not been set and this may be a child
    {
        if (pParentRendition != nullptr)
            return pParentRendition->pParentPlaylist->GetPIDFilter();
        else if (pParentStream != nullptr)
            return pParentStream->pParentPlaylist->GetPIDFilter();
        else
            return std::map<ContentType, unsigned short>();
    }
    else
        return spPIDFilter != nullptr ? *spPIDFilter : std::map<ContentType, unsigned short>();
}

void Playlist::ResetPIDFilter(ContentType forType)
{

    if (this->IsVariant != false && this->ActiveVariant != nullptr)
    {
        auto target = this->ActiveVariant;
        if (target->spPlaylist != nullptr)
            target->spPlaylist->ResetPIDFilter(forType);
        auto audren = target->GetActiveAudioRendition();
        auto vidren = target->GetActiveVideoRendition();
        if (audren != nullptr && audren->spPlaylist != nullptr)
            audren->spPlaylist->ResetPIDFilter(forType);
        if (vidren != nullptr && vidren->spPlaylist != nullptr)
            vidren->spPlaylist->ResetPIDFilter(forType);
    }
    else
    {
        if (spPIDFilter->find(forType) == spPIDFilter->end())
            return;

        spPIDFilter->erase(forType);

        task<void>([this]()
        {
            std::vector<shared_ptr<MediaSegment>> downloaded;
            std::vector <shared_ptr<MediaSegment>> others;
            std::partition_copy(Segments.begin(), Segments.end(), back_inserter(downloaded), back_inserter(others), [this](shared_ptr<MediaSegment> m)
            {
                return m->GetCurrentState() == INMEMORYCACHE;
            });


            for (auto itm : downloaded)
            {
                itm->ApplyPIDFilter(*spPIDFilter);
            }
        }, task_options(task_continuation_context::use_arbitrary()));

    }
    return;
}

void Playlist::ResetPIDFilter()
{

    if (this->IsVariant != false && this->ActiveVariant != nullptr)
    {
        auto target = this->ActiveVariant;
        if (target->spPlaylist != nullptr)
            target->spPlaylist->ResetPIDFilter();
        auto audren = target->GetActiveAudioRendition();
        auto vidren = target->GetActiveVideoRendition();
        if (audren != nullptr && audren->spPlaylist != nullptr)
            audren->spPlaylist->ResetPIDFilter();
        if (vidren != nullptr && vidren->spPlaylist != nullptr)
            vidren->spPlaylist->ResetPIDFilter();
    }
    else
    {

        spPIDFilter->clear();

        task<void>([this]()
        {
            std::vector<shared_ptr<MediaSegment>> downloaded;
            std::vector <shared_ptr<MediaSegment>> others;
            std::partition_copy(Segments.begin(), Segments.end(), back_inserter(downloaded), back_inserter(others), [this](shared_ptr<MediaSegment> m)
            {
                return m->GetCurrentState() == INMEMORYCACHE;
            });

            for (auto itm : downloaded)
            {
                itm->ApplyPIDFilter(std::map<ContentType, unsigned short>());
            }
        }, task_options(task_continuation_context::use_arbitrary()));

    }
    return;
}

void Playlist::SetPIDFilter(shared_ptr<std::map<ContentType, unsigned short>> pidfilter)
{

    if (this->IsVariant != false && this->ActiveVariant != nullptr)
    {
        auto target = this->ActiveVariant;
        if (target->spPlaylist != nullptr)
            target->spPlaylist->SetPIDFilter(pidfilter);
        auto audren = target->GetActiveAudioRendition();
        auto vidren = target->GetActiveVideoRendition();
        if (audren != nullptr && audren->spPlaylist != nullptr)
            audren->spPlaylist->SetPIDFilter(pidfilter);
        if (vidren != nullptr && vidren->spPlaylist != nullptr)
            vidren->spPlaylist->SetPIDFilter(pidfilter);
    }
    else
    {
        cpMediaSource->protectionRegistry.Register(task<HRESULT>([this, pidfilter]()
        {
            std::vector<shared_ptr<MediaSegment>> downloaded;
            std::vector <shared_ptr<MediaSegment>> others;

            std::lock_guard<recursive_mutex> lock(this->LockMerge);
            std::partition_copy(Segments.begin(), Segments.end(), back_inserter(downloaded), back_inserter(others), [this](shared_ptr<MediaSegment> m)
            {
                return m->GetCurrentState() == INMEMORYCACHE;
            });

            bool Force = spPIDFilter != nullptr && pidfilter == nullptr;
            auto filter = pidfilter != nullptr ? *pidfilter : std::map<ContentType, unsigned short>();


            for (auto itm : downloaded)
            {
                itm->ApplyPIDFilter(filter, Force);
            }

            return S_OK;
        }, task_options(task_continuation_context::use_arbitrary())));
        spPIDFilter = pidfilter;
    }
    return;
}

shared_ptr<StreamInfo> Playlist::DownloadVariantStreamPlaylist(unsigned int desiredbitrate, bool failfast, short searchdirectionincaseoffailure, bool TestSegmentDownload)
{
    if (!IsVariant) return nullptr;

    //find the closest bitrate
    auto targetBitrate = cpMediaSource->spHeuristicsManager->FindClosestBitrate(desiredbitrate);
    //find the target stream
    auto streaminfo = Variants.find(targetBitrate)->second;
    //check to see if the playlist is downloaded
    if (streaminfo->spPlaylist == nullptr || IsLive)
    {
        HRESULT hr = streaminfo->DownloadPlaylistAsync().get();
        if (SUCCEEDED(hr) && TestSegmentDownload)
        {
            auto StartSeg = IsLive ?
                streaminfo->spPlaylist->GetSegment(streaminfo->spPlaylist->FindLiveStartSegmentSequenceNumber()) :
                streaminfo->spPlaylist->Segments.front();

            bool tempsetActiveVariant = false;
            //temprarily set the active variant - otherwise code downstream will fail
            if (this->ActiveVariant == nullptr)
            {
                tempsetActiveVariant = true;
                ActiveVariant = streaminfo.get();
            }
            auto ret = Playlist::StartStreamingAsync(streaminfo->spPlaylist.get(), StartSeg->GetSequenceNumber(), false, true, true).get();
            if (tempsetActiveVariant)
                this->ActiveVariant = nullptr;
            hr = std::get<0>(ret);
        }

        if (FAILED(hr))
        {
            if (failfast)
                return nullptr;

            auto startSearch = targetBitrate;

            if (searchdirectionincaseoffailure <= 0)
            {
                try
                {
                    do
                    {
                        if (targetBitrate != Variants.begin()->first)
                        {
                            targetBitrate = cpMediaSource->spHeuristicsManager->FindNextLowerBitrate(targetBitrate);
                            streaminfo = Variants.find(targetBitrate)->second;
                        }
                        else
                            break;
                    } while (FAILED(hr = streaminfo->DownloadPlaylistAsync().get()));
                }
                catch (task_canceled tc)
                {
                }

            }

            if (FAILED(hr) && searchdirectionincaseoffailure >= 0)
            {
                targetBitrate = startSearch;
                streaminfo = Variants.find(targetBitrate)->second;
                try
                {
                    while (FAILED(hr = streaminfo->DownloadPlaylistAsync().get()))
                    {
                        if (targetBitrate != Variants.rbegin()->first)
                        {
                            targetBitrate = cpMediaSource->spHeuristicsManager->FindNextHigherBitrate(targetBitrate);
                            streaminfo = Variants.find(targetBitrate)->second;
                        }
                        else
                            break;
                    }
                }
                catch (task_canceled tc)
                {
                }
            }

            if (FAILED(hr))
                return nullptr;
        }

    }

    return streaminfo;
}

shared_ptr<StreamInfo> Playlist::ActivateStream(unsigned int desiredbitrate, bool failfast, short searchdirectionincaseoffailure, bool TestSegmentDownload)
{
    auto streaminfo = DownloadVariantStreamPlaylist(desiredbitrate, failfast, searchdirectionincaseoffailure, TestSegmentDownload);
    if (streaminfo != nullptr)
    {
        streaminfo->MakeActive();
    }
    return streaminfo;
}

void Playlist::InitializeVideoStreamTickCallback()
{
    OnVideoStreamTick = [this]()
    {
        auto stream = cpMediaSource->cpVideoStream;
        auto curSegment = GetCurrentSegmentTracker(VIDEO);
        if (stream == nullptr)
            return;
        if (curSegment == nullptr)
            return;

        if (stream->StreamTickBase != nullptr)
        {
            stream->StreamTickBase->ValueInTicks +=
                stream->ApproximateFrameDistance == 0 ? DEFAULT_VIDEO_STREAMTICK_OFFSET : stream->ApproximateFrameDistance;

            unsigned long long ts;
            if (IsLive)
            {
                ts = stream->StreamTickBase->ValueInTicks;
            }
            else
            {
                ts = curSegment->TSAbsoluteToRelative(stream->StreamTickBase)->ValueInTicks;
            }


            stream->NotifyStreamTick(ts);
            LOG("STREAM TICK : VIDEO @ " << ts);
        }
    };
}

void Playlist::StartVideoStreamTick()
{
    StopVideoStreamTick();
    auto stream = cpMediaSource->cpVideoStream;
    if (stream != nullptr && cpMediaSource->GetCurrentState() == MSS_STARTED)//only if we are playing
    {

        spswVideoStreamTick = make_shared<StopWatch>();
        spswVideoStreamTick->TickEventFrequency = stream->ApproximateFrameDistance == 0 ? DEFAULT_VIDEO_STREAMTICK_OFFSET : stream->ApproximateFrameDistance;
        spswVideoStreamTick->StartTicking(OnVideoStreamTick);
    }
}


void Playlist::StopVideoStreamTick()
{
    if (spswVideoStreamTick != nullptr)
    {
        spswVideoStreamTick->StopTicking();
        spswVideoStreamTick.reset();
    }
}


void Playlist::InitializePlaylistRefreshCallback()
{
    OnPlaylistRefresh = [this]()
    {
        LOG("Refreshing Playlist...");

        std::unique_lock<std::recursive_mutex> LockSW(lockPlaylistRefreshStopWatch, std::defer_lock);

        LockSW.try_lock();
        if (LockSW.owns_lock() == false)
        {
            LOG("Refreshing Playlist..Could not acquire initial lock.");
            return;
        }

        if (this->IsLive && (this->IsVariant == false || (this->pParentStream != nullptr && !this->pParentStream->IsActive)) && spswPlaylistRefresh != nullptr)
        {
            spswPlaylistRefresh->StopTicking();
            spswPlaylistRefresh.reset();
        }

        if (this->IsVariant || (this->pParentStream != nullptr && !this->pParentStream->IsActive)) return; //variant master or non active variant


        HRESULT hr = S_OK;
        try
        {
            //stop ticking - we will reset stop watch with next tick interval once we merge the download
            if (spswPlaylistRefresh != nullptr)
            {
                spswPlaylistRefresh->StopTicking();
                spswPlaylistRefresh.reset();
            }

            if (cpMediaSource->GetCurrentState() == MSS_ERROR || cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
                return;


            if (this->pParentStream != nullptr)//variant child
            {
                auto si = cpMediaSource->spRootPlaylist->DownloadVariantStreamPlaylist(this->pParentStream->Bandwidth);
                if (si == nullptr) //serious
                {
                    this->cpMediaSource->NotifyError(hr);
                    return;
                }
                else if (si->Bandwidth != pParentStream->Bandwidth)
                {
                    pParentStream->spPlaylistRefresh = si->spPlaylistRefresh; //copy the refresh version over
                }

                {
                    //download the target playlist if a bitrate switch is pending
                    if (std::try_lock(this->cpMediaSource->cpVideoStream->LockSwitch, this->cpMediaSource->cpAudioStream->LockSwitch) < 0)
                    {
                        std::lock_guard<std::recursive_mutex> lockV(this->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
                        std::lock_guard<std::recursive_mutex> lockA(this->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

                        auto videoswitch = this->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
                        if (videoswitch != nullptr && videoswitch->VideoSwitchedTo == nullptr) //have not identified segment for video to switch to
                        {
                            if (FAILED(hr = videoswitch->targetPlaylist->pParentStream->DownloadPlaylistAsync().get()))
                            {
                                cpMediaSource->TryCancelPendingBitrateSwitch(true);
                            }
                        }
                    }
                }
            }
            else //live master
            {
                DownloadPlaylistAsync(this->cpMediaSource, Helpers::JoinUri(this->BaseUri, this->FileName), cpMediaSource->spRootPlaylist).wait();
            }

            if (
                (
                    (this->pParentStream != nullptr && this->pParentStream->spPlaylistRefresh != nullptr) ||
                    (!this->IsVariant && this->spswPlaylistRefresh != nullptr)
                    ) &&
                this->cpMediaSource->IsBuffering()) //if buffering - sample request will not be invoked and we need to merge playlists here
            {
                if (this->pParentStream != nullptr && this->pParentStream->spPlaylistRefresh != nullptr)
                    this->MergeLivePlaylistChild();
                else
                    this->MergeLivePlaylistMaster();


                //attempt to break buffering by inducing downloads - we do a little bit of checking before we call this
                auto minseg = MinCurrentSegment();//this is where the playhead is right now 
                auto maxseg = MaxCurrentSegment();
                auto nextseg = minseg->GetSequenceNumber() == maxseg->GetSequenceNumber() ? GetNextSegment(minseg->SequenceNumber, cpMediaSource->GetCurrentDirection()) : maxseg;

                if (minseg != nullptr)
                {
                    auto state = minseg->GetCurrentState();
                    if (state == INMEMORYCACHE)
                    {
                        if (minseg->GetSegmentLookahead(this->cpMediaSource->GetCurrentDirection()) > 0)//we should have LAB
                        {
                            LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                            this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                        }
                        else if (nextseg != nullptr)
                        {
                            if (nextseg->GetCurrentState() == INMEMORYCACHE && (nextseg->GetSegmentLookahead(this->cpMediaSource->GetCurrentDirection()) > 0 || nextseg->SequenceNumber == Segments.back()->SequenceNumber))//we should have LAB
                            {
                                LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                                this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                            }
                            else if (nextseg->GetCurrentState() != INMEMORYCACHE)
                            {
                                LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                                StartStreamingAsync(this, nextseg->SequenceNumber, true, true);
                                this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                            }
                        }
                    }
                    else if (state != DOWNLOADING) //not downloading
                    {
                        LOG("Playlist::OnPlaylistRefresh() - Inducing Streaming to break buffering in merge");
                        StartStreamingAsync(this, minseg->SequenceNumber, true, true);
                    }
                    //else if DOWNLOADING - we wait and try again in the next iteration because the download may break the buffering
                }

                if (sptceWaitPlaylistRefreshPlaybackResume != nullptr)//waiting on playlist refresh - new segment arrived
                {
                    if (!cpMediaSource->IsBuffering())
                    {
                        sptceWaitPlaylistRefreshPlaybackResume->set(S_OK);
                    }
                    else if (cpMediaSource->IsBuffering() &&
                        GetNextSegment(maxseg->SequenceNumber, cpMediaSource->GetCurrentDirection()) != nullptr)
                    {
                        LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge - new segment arrived in playlist refresh");
                        cpMediaSource->EndBuffering();
                        sptceWaitPlaylistRefreshPlaybackResume->set(S_OK);
                    }
                }

            }

        }
        catch (concurrency::task_canceled tc)
        {
        }
        catch (...)
        {
        }

        if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
            cpMediaSource->GetCurrentState() != MSS_STOPPED && cpMediaSource->GetCurrentState() != MSS_PAUSED)
        {
            if (spswPlaylistRefresh == nullptr && !this->LastLiveRefreshProcessed)//set it for the next refresh
                SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2));
        }
        return;
    };
}


void Playlist::CancelDownloadsAndWaitForCompletion()
{
    if (IsVariant)
    {
        for (auto itr : this->Variants)
        {
            if (itr.second != nullptr)
            {
                if (itr.second->spPlaylist != nullptr)
                    itr.second->spPlaylist->CancelDownloadsAndWaitForCompletion();
            }
        }
    }
    else
    {

        //LOGIF(this->pParentStream != nullptr, "Playlist::CancelDownloadsAndWaitForCompletion() : Cancelling Downloads for " << this->pParentStream->Bandwidth);

        try
        {
            std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
            if (IsLive) listlock.lock();

            if (Segments.size() != 0)
            {
                for (auto itr : Segments)
                {
                    if (itr->GetCurrentState() == DOWNLOADING)
                        itr->SetCurrentState(UNAVAILABLE);
                    itr->CancelDownloads(true);
                }
            }

            spDownloadRegistry->CancelAll(true);

            this->taskRegistry.CancelAll(true);

            this->taskRegistry.WhenAll().wait();
        }
        catch (...)
        {
        }
    }
}

void Playlist::CancelDownloads()
{
    try
    {
        //  ScopedLock lock(&csRoot);
        if (IsVariant)
        {
            for (auto itr : this->Variants)
            {
                if (itr.second != nullptr)
                {
                    if (itr.second->spPlaylist != nullptr)
                        itr.second->spPlaylist->CancelDownloads();
                }
            }
        }
        else
        {

            //LOGIF(this->pParentStream != nullptr, "Playlist::CancelDownloads() : Cancelling Downloads for " << this->pParentStream->Bandwidth);
            spDownloadRegistry->CancelAll(true);

            std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
            if (IsLive) listlock.lock();

            if (Segments.size() != 0)
            {
                for (auto itr : Segments)
                {
                    if (itr->GetCurrentState() == DOWNLOADING)
                        itr->SetCurrentState(UNAVAILABLE);
                    itr->CancelDownloads();
                }
            }

            this->taskRegistry.CancelAll();

        }
    }
    catch (task_canceled tc)
    {
    }
}

task<HRESULT> Playlist::DownloadPlaylistAsync(
    HLSControllerFactory ^pFactory,
    DefaultContentDownloader^ downloader,
    const std::wstring& URL,
    std::shared_ptr<Playlist>& spPlaylist,
    task_completion_event<HRESULT> tcePlaylistDownloaded)
{

    wstring url = URL;
    std::map<wstring, wstring> headers;
    std::vector<shared_ptr<Cookie>> cookies;
    Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;

    if (pFactory != nullptr)
        pFactory->RaisePrepareResourceRequest(ResourceType::PLAYLIST, url, cookies, headers, &external);

    downloader->Initialize(ref new Platform::String(url.data()));

    if (external == nullptr)
        downloader->SetParameters(nullptr, L"GET", cookies, headers);
    else
        downloader->SetParameters(nullptr, external);



    downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
        [&spPlaylist, url, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
    {
        DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);

        if (args->Content != nullptr && args->IsSuccessStatusCode)
        {
            std::vector<BYTE> MemoryCache = DefaultContentDownloader::BufferToVector(args->Content);
            if (MemoryCache.size() == 0)
                tcePlaylistDownloaded.set(E_FAIL);
            else
                Playlist::OnPlaylistDownloadCompleted(args->ContentUri->AbsoluteUri->Data(), MemoryCache, spPlaylist, tcePlaylistDownloaded);
        }
        else
            tcePlaylistDownloaded.set(E_FAIL);

    });



    downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
        [tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
    {
        tcePlaylistDownloaded.set(E_FAIL);
    });

    downloader->DownloadAsync();
    return task<HRESULT>(tcePlaylistDownloaded);
}


///<summary>Downloads a root playlist</summary>  
///<param name='URL'>The playlist URL</param>
///<param name='spPlaylist'>The downloaded playlist</param>
///<param name='tcePlaylistDownloaded'>TCE to signal completion </param>
///<returns>Task to wait on</returns>
task<HRESULT> Playlist::DownloadPlaylistAsync(
    CHLSMediaSource *ms,
    const std::wstring& URL,
    std::shared_ptr<Playlist>& spPlaylist,
    task_completion_event<HRESULT> tcePlaylistDownloaded)
{
    //create downloader


    if (ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED)
    {
        tcePlaylistDownloaded.set(E_FAIL);
        return task<HRESULT>(tcePlaylistDownloaded);
    }

    wstring url = URL;
    std::map<wstring, wstring> headers;
    std::vector<shared_ptr<Cookie>> cookies;
    Microsoft::HLSClient::IHLSContentDownloader^ external = nullptr;
    ms->cpControllerFactory->RaisePrepareResourceRequest(ResourceType::PLAYLIST, url, cookies, headers, &external);

    DefaultContentDownloader^ downloader = ref new DefaultContentDownloader();

    downloader->Initialize(ref new Platform::String(url.data()));
    if (external == nullptr)
        downloader->SetParameters(nullptr, L"GET", cookies, headers);
    else
        downloader->SetParameters(nullptr, external);


    downloader->Completed += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^>(
        [ms, &spPlaylist, url, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadCompletedArgs ^args)
    {
        DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);


        if ((ms->GetCurrentState() == MSS_ERROR || ms->GetCurrentState() == MSS_UNINITIALIZED))
        {
            tcePlaylistDownloaded.set(E_FAIL);
        }
        else
        {
            if (args->Content != nullptr && args->IsSuccessStatusCode)
            {
                std::vector<BYTE> MemoryCache = DefaultContentDownloader::BufferToVector(args->Content);
                if (MemoryCache.size() == 0)
                    tcePlaylistDownloaded.set(E_FAIL);
                else
                    Playlist::OnPlaylistDownloadCompleted(args->ContentUri->AbsoluteUri->Data(), MemoryCache, spPlaylist, tcePlaylistDownloaded);
            }
            else
                tcePlaylistDownloaded.set(E_FAIL);
        }

        //    ms->spDownloadRegistry->Unregister(downloader);

    });
    downloader->Error += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSContentDownloader ^, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^>(
        [ms, tcePlaylistDownloaded](Microsoft::HLSClient::IHLSContentDownloader ^sender, Microsoft::HLSClient::IHLSContentDownloadErrorArgs ^args)
    {
        DefaultContentDownloader^ downloader = static_cast<DefaultContentDownloader^>(sender);
        //    ms->spDownloadRegistry->Unregister(downloader);
        tcePlaylistDownloaded.set(E_FAIL);
    });


    ms->spDownloadRegistry->Register(downloader);

    downloader->DownloadAsync();
    return task<HRESULT>(tcePlaylistDownloaded);
}

HRESULT Playlist::OnPlaylistDownloadCompleted(const std::wstring& Url,
    std::vector<BYTE> MemoryCache,
    std::shared_ptr<Playlist>& spPlaylist,
    task_completion_event<HRESULT> tcePlaylistDownloaded)
{
    try
    {
        std::wstring baseuri, filename;
        Helpers::SplitUri(Url, baseuri, filename);

        if (spPlaylist == nullptr)
        {
            spPlaylist = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename);
            LOG("Playlist Download");
            LOG(spPlaylist->szData);
        }
        else if (spPlaylist->IsLive && !spPlaylist->IsVariant)
        {
            std::lock_guard<std::recursive_mutex> lockmerge(spPlaylist->LockMerge);
            spPlaylist->spPlaylistRefresh = std::make_shared<Playlist>(std::wstring(MemoryCache.begin(), MemoryCache.end()), baseuri, filename);
            if (spPlaylist->szData != spPlaylist->spPlaylistRefresh->szData)
                spPlaylist->spPlaylistRefresh->Parse();

            LOG("Playlist Refresh Download");
            LOG(spPlaylist->spPlaylistRefresh->szData);
        }

    }
    catch (...)
    {
        tcePlaylistDownloaded.set(E_FAIL);
        return E_FAIL;
    }
    tcePlaylistDownloaded.set(S_OK);
    return S_OK;
}

void Playlist::SetStopwatchForNextPlaylistRefresh(unsigned long long refreshIntervalInTicks, bool lockmerge)
{

    auto  StopwatchEvent = [this]()
    {
        LOG("Refreshing Playlist...");

        std::unique_lock<std::recursive_mutex> LockSW(lockPlaylistRefreshStopWatch, std::defer_lock);

        LockSW.try_lock();
        if (LockSW.owns_lock() == false)
        {
            LOG("Refreshing Playlist..Could not acquire initial lock.");
            return;
        }

        if (this->IsLive && (this->IsVariant == false || (this->pParentStream != nullptr && !this->pParentStream->IsActive)) && spswPlaylistRefresh != nullptr)
        {
            spswPlaylistRefresh->StopTicking();
            spswPlaylistRefresh.reset();
        }

        if (this->IsVariant || (this->pParentStream != nullptr && !this->pParentStream->IsActive)) return; //variant master or non active variant


        HRESULT hr = S_OK;
        try
        {

            //stop ticking - we will reset stop watch with next tick interval once we merge the download
            if (spswPlaylistRefresh != nullptr)
            {
                spswPlaylistRefresh->StopTicking();
                spswPlaylistRefresh.reset();
            }


            if (cpMediaSource->GetCurrentState() == MSS_ERROR || cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
                return;


            if (this->pParentStream != nullptr)//variant child
            {
                auto si = cpMediaSource->spRootPlaylist->DownloadVariantStreamPlaylist(this->pParentStream->Bandwidth);
                if (si == nullptr) //serious
                {
                    this->cpMediaSource->NotifyError(hr);
                    return;
                }
                else if (si->Bandwidth != pParentStream->Bandwidth)
                {
                    pParentStream->spPlaylistRefresh = si->spPlaylistRefresh; //copy the refresh version over
                }

                {
                    //download the target playlist if a bitrate switch is pending
                    if (std::try_lock(this->cpMediaSource->cpVideoStream->LockSwitch, this->cpMediaSource->cpAudioStream->LockSwitch) < 0)
                    {
                        std::lock_guard<std::recursive_mutex> lockV(this->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
                        std::lock_guard<std::recursive_mutex> lockA(this->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

                        auto videoswitch = this->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
                        if (videoswitch != nullptr && videoswitch->VideoSwitchedTo == nullptr) //have not identified segment for video to switch to
                        {
                            if (FAILED(hr = videoswitch->targetPlaylist->pParentStream->DownloadPlaylistAsync().get()))
                            {
                                cpMediaSource->TryCancelPendingBitrateSwitch(true);
                            }
                        }
                    }
                }
            }
            else //live master
            {
                DownloadPlaylistAsync(this->cpMediaSource, Helpers::JoinUri(this->BaseUri, this->FileName), cpMediaSource->spRootPlaylist).wait();
            }

            if (
                (
                    (this->pParentStream != nullptr && this->pParentStream->spPlaylistRefresh != nullptr) ||
                    (!this->IsVariant && this->spswPlaylistRefresh != nullptr)
                    ) &&
                this->cpMediaSource->IsBuffering()) //if buffering - sample request will not be invoked and we need to merge playlists here
            {
                if (this->pParentStream != nullptr && this->pParentStream->spPlaylistRefresh != nullptr)
                    this->MergeLivePlaylistChild();
                else
                    this->MergeLivePlaylistMaster();

                //attempt to break buffering by inducing downloads - we do a little bit of checking before we call this
                auto minseg = MinCurrentSegment();//this is where the playhead is right now 
                auto maxseg = MaxCurrentSegment();
                auto nextseg = minseg->GetSequenceNumber() == maxseg->GetSequenceNumber() ? GetNextSegment(minseg->SequenceNumber, cpMediaSource->GetCurrentDirection()) : maxseg;

                if (minseg != nullptr)
                {
                    auto state = minseg->GetCurrentState();
                    if (state == INMEMORYCACHE)
                    {
                        if (minseg->GetSegmentLookahead(this->cpMediaSource->GetCurrentDirection()) > 0)//we should have LAB
                        {
                            LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                            this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                        }
                        else if (nextseg != nullptr)
                        {
                            if (nextseg->GetCurrentState() == INMEMORYCACHE && (nextseg->GetSegmentLookahead(this->cpMediaSource->GetCurrentDirection()) > 0 || nextseg->SequenceNumber == Segments.back()->SequenceNumber))//we should have LAB
                            {
                                LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                                this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                            }
                            else if (nextseg->GetCurrentState() != INMEMORYCACHE)
                            {
                                LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge");
                                StartStreamingAsync(this, nextseg->SequenceNumber, true, true);
                                this->cpMediaSource->EndBuffering(); //stop buffering and let the source start requesting samples again
                            }
                        }
                    }
                    else if (state != DOWNLOADING) //not downloading
                    {
                        LOG("Playlist::OnPlaylistRefresh() - Inducing Streaming to break buffering in merge");
                        StartStreamingAsync(this, minseg->SequenceNumber, true, true);
                    }
                    //else if DOWNLOADING - we wait and try again in the next iteration because the download may break the buffering
                }

                if (sptceWaitPlaylistRefreshPlaybackResume != nullptr)//waiting on playlist refresh - new segment arrived
                {
                    if (!cpMediaSource->IsBuffering())
                    {
                        sptceWaitPlaylistRefreshPlaybackResume->set(S_OK);
                    }
                    else if (cpMediaSource->IsBuffering() &&
                        GetNextSegment(maxseg->SequenceNumber, cpMediaSource->GetCurrentDirection()) != nullptr)
                    {
                        LOG("Playlist::OnPlaylistRefresh() - Breaking Buffering in merge - new segment arrived in playlist refresh");
                        cpMediaSource->EndBuffering();
                        sptceWaitPlaylistRefreshPlaybackResume->set(S_OK);
                    }
                }

            }

        }
        catch (concurrency::task_canceled tc)
        {
        }
        catch (...)
        {
        }

        if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
            cpMediaSource->GetCurrentState() != MSS_STOPPED && cpMediaSource->GetCurrentState() != MSS_PAUSED)
        {
            if (spswPlaylistRefresh == nullptr && !this->LastLiveRefreshProcessed)//set it for the next refresh
                SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2));
        }
        return;
    };


    if (IsLive && cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)//only if we are playing live
    {
        std::unique_lock<std::recursive_mutex> lock(LockMerge, defer_lock);

        if (lockmerge)
        {
            lock.lock();
        }
        if (spswPlaylistRefresh != nullptr)
        {
            spswPlaylistRefresh->StopTicking();
            spswPlaylistRefresh.reset();
        }
        spswPlaylistRefresh = make_shared<StopWatch>();
        spswPlaylistRefresh->TickEventFrequency = refreshIntervalInTicks;
        spswPlaylistRefresh->StartTickingOnce(StopwatchEvent);
    }
}

shared_ptr<Timestamp> Playlist::GetPlaylistStartTimestamp()
{
    if (this->IsLive == false)
    {
        if (this->Segments.size() > 0)
        {
            StartStreamingAsync(this, this->Segments.at(0)->GetSequenceNumber(), false, true, true).wait();
        }
    }
    else
    {

        std::lock_guard<std::recursive_mutex> lockm(LockMerge);
        if (this->Segments.size() > 0)
        {
            StartStreamingAsync(this, this->Segments.at(0)->GetSequenceNumber(), false, true, true).wait();
        }
    }
    return this->StartPTSOriginal;
}


bool Playlist::MergeLivePlaylistChild()
{
    bool Changed = false;
    bool StillLive = false;
    bool SlidingWindowChanged = false;

    if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
    {


        if (pParentStream->IsActive)//if this is the active playlist
        {
            //merge the target playlist if a bitrate switch is pending
            if (std::try_lock(this->cpMediaSource->cpVideoStream->LockSwitch, this->cpMediaSource->cpAudioStream->LockSwitch) < 0)
            {
                std::lock_guard<std::recursive_mutex> lockV(this->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
                std::lock_guard<std::recursive_mutex> lockA(this->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

                auto videoswitch = this->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
                if (videoswitch != nullptr && videoswitch->VideoSwitchedTo == nullptr) //have not identified segment for video to switch to
                {
                    videoswitch->targetPlaylist->MergeLivePlaylistChild();
                }
            }
        }

        if (pParentStream->GetActiveAudioRendition() != nullptr &&
            pParentStream->GetActiveAudioRendition()->spPlaylist != nullptr &&
            pParentStream->GetActiveAudioRendition()->spPlaylist->IsLive)
        {
            if (!pParentStream->GetActiveAudioRendition()->spPlaylist->MergeAlternateRenditionPlaylist())
            {
                if (spswPlaylistRefresh == nullptr)
                    SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2), false);
                return false;
            }
        }


        std::unique_lock<std::recursive_mutex> lockmerge(LockMerge, std::defer_lock);
        std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);


        lockmerge.try_lock();
        if (lockmerge.owns_lock() == false)
        {
            if (spswPlaylistRefresh == nullptr)
                SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2), false);
            return false;
        }

        listlock.try_lock();
        if (listlock.owns_lock() == false)
        {
            if (spswPlaylistRefresh == nullptr)
                SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2), false);
            return false;
        }

        LOG("Attempting Media Playlist Merge...");

        try
        {

            if (pParentStream->spPlaylistRefresh != nullptr && pParentStream->spPlaylistRefresh->IsValid)
            {
                if (pParentStream->spPlaylistRefresh->Segments.size() > 0 &&
                    pParentStream->spPlaylistRefresh->Segments.back()->GetSequenceNumber() > Segments.back()->GetSequenceNumber())
                {
                    if (pParentStream->spPlaylistRefresh->DerivedTargetDuration < pParentStream->spPlaylist->DerivedTargetDuration)
                        pParentStream->spPlaylist->DerivedTargetDuration = pParentStream->spPlaylistRefresh->DerivedTargetDuration;


                    LOG("*** PLAYLIST MERGE (" << (pParentStream->IsActive ? L"Active" : L"Inactive") << ") for speed " << pParentStream->Bandwidth << " ***");
                    std::shared_ptr<EncryptionKey> lastEncKey = nullptr;
                    //get the min current segment
                    auto mincur = MinCurrentSegment();

                    //refresh has segments older than where we are playing now - we retain upto that
                    if ((mincur != nullptr && pParentStream->spPlaylistRefresh->Segments.size() > 0
                        && mincur->GetSequenceNumber() > pParentStream->spPlaylistRefresh->Segments.front()->GetSequenceNumber()) || mincur == nullptr)
                        mincur = GetSegment(pParentStream->spPlaylistRefresh->Segments.front()->GetSequenceNumber());


                    bool ApplyDiscontinuity = false;

                    if (PlaylistType != Microsoft::HLSClient::HLSPlaylistType::EVENT)
                    {

                        if (mincur != nullptr)
                        {
                            //we drop everything before mincur that is not currently in downloading state and is in the current playlist but not in the refreshed version
                            while (Segments.size() > 0 && Segments.front()->SequenceNumber != mincur->SequenceNumber && Segments.front()->GetCurrentState() != DOWNLOADING)
                            {
                                if (!Changed) Changed = true;

                                LOG("Dropping " << Segments.front()->SequenceNumber);

                                //shift the SlidingWindowStart 
                                if (SlidingWindowStart != nullptr)
                                {
                                    if (Segments.front()->Timeline.empty() == false)
                                        SlidingWindowStart = make_shared<Timestamp>(__max(Segments.front()->Timeline.back()->ValueInTicks, SlidingWindowStart->ValueInTicks + Segments.front()->CumulativeDuration));
                                    else
                                        SlidingWindowStart = make_shared<Timestamp>(SlidingWindowStart->ValueInTicks + Segments.front()->CumulativeDuration);

                                    SlidingWindowChanged = true;
                                }
                                if (ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                    ApplyDiscontinuity = true;

                                Segments.erase(Segments.begin());

                            }
                            lastEncKey = mincur->EncKey;


                        }
                        else
                        {
                            if (Segments.size() > 0 && ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                ApplyDiscontinuity = true;

                            Segments.clear();
                            if (!Changed) Changed = true;
                            LOG("Dropping all segments");
                        }
                    }

                    if (pParentStream->spPlaylistRefresh->Segments.size() > 0)
                    {
                        //we skip everything in the new playlist until we find an entry in the 
                        auto startaddingat = Segments.size() == 0 ? pParentStream->spPlaylistRefresh->Segments.begin() :
                            std::find_if(pParentStream->spPlaylistRefresh->Segments.begin(), pParentStream->spPlaylistRefresh->Segments.end(), [this](shared_ptr<MediaSegment> seg)
                        {
                            return seg->SequenceNumber > Segments.back()->SequenceNumber;
                        });
                        if (startaddingat != pParentStream->spPlaylistRefresh->Segments.end())
                        {
                            if (!Changed) Changed = true;
                            for (auto rpitr = startaddingat; rpitr != pParentStream->spPlaylistRefresh->Segments.end(); ++rpitr)
                            {
                                (*rpitr)->pParentPlaylist = this;

                                if ((*rpitr)->EncKey != nullptr && lastEncKey != nullptr && (*rpitr)->EncKey->IsEqual(lastEncKey) && lastEncKey->cpCryptoKey != nullptr)
                                    (*rpitr)->EncKey = lastEncKey;
                                else
                                    lastEncKey = (*rpitr)->EncKey;
                                LOG("Adding " << (*rpitr)->SequenceNumber);
                                if (ApplyDiscontinuity)
                                    (*rpitr)->Discontinous = true;
                                Segments.push_back(*rpitr);
                            }
                        }
                        BaseSequenceNumber = mincur == nullptr ? Segments.front()->SequenceNumber : mincur->SequenceNumber;
                    }

                    this->LastLiveRefreshProcessed = !pParentStream->spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                    if (Changed)
                    {
                        auto cdur = 0ULL; //recalc durations
                        for (auto itr : Segments)
                        {

                            itr->CumulativeDuration = cdur + itr->Duration;
                            cdur = itr->CumulativeDuration;
                            if (itr->EncKey != nullptr)
                                itr->EncKey->pParentPlaylist = this;
                        }


                        LOG("*** END MERGE ***");
                    }

                    if (SlidingWindowStart != nullptr && Segments.size() > 0)
                    {
                        SlidingWindowEnd = std::make_shared<Timestamp>(SlidingWindowStart->ValueInTicks + Segments.back()->CumulativeDuration);
                        SlidingWindowChanged = true;
                    }
                }
                else
                {
                    this->LastLiveRefreshProcessed = !pParentStream->spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                }

                this->szData = std::move(pParentStream->spPlaylistRefresh->szData);
                this->LastModified = std::move(pParentStream->spPlaylistRefresh->LastModified);
                this->ETag = std::move(pParentStream->spPlaylistRefresh->ETag);
                this->pParentStream->spPlaylistRefresh.reset();

            }
            else
            {
                if (pParentStream->spPlaylistRefresh != nullptr)
                {
                    this->LastModified = std::move(pParentStream->spPlaylistRefresh->LastModified);
                    this->ETag = std::move(pParentStream->spPlaylistRefresh->ETag);
                    this->pParentStream->spPlaylistRefresh.reset();
                }
            }

            if (SlidingWindowChanged)
            {

                this->pParentStream->pParentPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([this]()
                {
                    if (this->pParentStream->pParentPlaylist->cpMediaSource != nullptr  && //do not raise for alternate renditions
                        this->pParentStream->pParentPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && this->pParentStream->pParentPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
                        this->pParentStream->pParentPlaylist->cpMediaSource->cpController != nullptr && this->pParentStream->pParentPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                        this->pParentStream->pParentPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSlidingWindowChanged(this);

                    return S_OK;
                }, task_options(task_continuation_context::use_arbitrary())));

            }

        }
        catch (...)
        {


        }
    }
    if (!this->LastLiveRefreshProcessed && //if this is the last playlist in the program this will be marked non-Live since we should find the EXE-X-ENDLIST - in that case we stop the stopwatch
        pParentStream->IsActive &&
        cpMediaSource->GetCurrentState() != MSS_STOPPED && cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED && cpMediaSource->GetCurrentState() != MSS_PAUSED) //this is the active playlist
    {

        if (Changed)
            SetStopwatchForNextPlaylistRefresh(this->Segments.back()->Duration);
        else
            SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2));

    }

    return Changed;
}


bool Playlist::MergeLivePlaylistMaster()
{
    bool Changed = false;
    bool StillLive = false;
    bool SlidingWindowChanged = false;

    if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
    {
        std::lock_guard<std::recursive_mutex> lock(LockMerge);
        LOG("Attempting Master Playlist Merge...");

        try
        {

            if (spPlaylistRefresh != nullptr && spPlaylistRefresh->IsValid)
            {
                if (spPlaylistRefresh->Segments.size() > 0 &&
                    spPlaylistRefresh->Segments.back()->GetSequenceNumber() > Segments.back()->GetSequenceNumber())
                {
                    //spPlaylistRefresh->Parse();
                    if (spPlaylistRefresh->DerivedTargetDuration < DerivedTargetDuration)
                        DerivedTargetDuration = pParentStream->spPlaylistRefresh->DerivedTargetDuration;
                    {
                        std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
                        if (IsLive) listlock.lock();
                        LOG("*** PLAYLIST MERGE ***");
                        std::shared_ptr<EncryptionKey> lastEncKey = nullptr;
                        //get the min current segment
                        auto mincur = MinCurrentSegment();

                        //refresh has segments older than where we are playing now - we retain upto that
                        if ((mincur != nullptr && spPlaylistRefresh->Segments.size() > 0 && mincur->GetSequenceNumber() > spPlaylistRefresh->Segments.front()->GetSequenceNumber()) || mincur == nullptr)
                            mincur = GetSegment(spPlaylistRefresh->Segments.front()->GetSequenceNumber());


                        bool ApplyDiscontinuity = false;

                        if (PlaylistType != Microsoft::HLSClient::HLSPlaylistType::EVENT)
                        {
                            if (mincur != nullptr)
                            {
                                //we drop everything before mincur that is not currently in downloading state and is in the current playlist but not in the refreshed version
                                while (Segments.size() > 0 && Segments.front()->SequenceNumber != mincur->SequenceNumber && Segments.front()->GetCurrentState() != DOWNLOADING)
                                {
                                    if (!Changed) Changed = true;

                                    LOG("Dropping " << Segments.front()->SequenceNumber);

                                    //shift the SlidingWindowStart 
                                    if (SlidingWindowStart != nullptr)
                                    {
                                        if (Segments.front()->Timeline.empty() == false)
                                            SlidingWindowStart = make_shared<Timestamp>(__max(Segments.front()->Timeline.back()->ValueInTicks, SlidingWindowStart->ValueInTicks + Segments.front()->CumulativeDuration));
                                        else
                                            SlidingWindowStart = make_shared<Timestamp>(SlidingWindowStart->ValueInTicks + Segments.front()->CumulativeDuration);

                                        SlidingWindowChanged = true;
                                    }
                                    if (ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                        ApplyDiscontinuity = true;
                                    Segments.erase(Segments.begin());

                                }
                                lastEncKey = mincur->EncKey;
                            }
                            else
                            {
                                if (Segments.size() > 0 && ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                    ApplyDiscontinuity = true;

                                Segments.clear();
                                if (!Changed) Changed = true;
                                LOG("Dropping all segments");
                            }
                        }


                        if (spPlaylistRefresh->Segments.size() > 0)
                        {
                            //we skip everything in the new playlist until we find an entry in the 
                            auto startaddingat = Segments.size() == 0 ? spPlaylistRefresh->Segments.begin() :
                                std::find_if(spPlaylistRefresh->Segments.begin(), spPlaylistRefresh->Segments.end(), [this](shared_ptr<MediaSegment> seg)
                            {
                                return seg->SequenceNumber > Segments.back()->SequenceNumber;
                            });
                            if (startaddingat != spPlaylistRefresh->Segments.end())
                            {
                                if (!Changed) Changed = true;
                                for (auto rpitr = startaddingat; rpitr != spPlaylistRefresh->Segments.end(); ++rpitr)
                                {
                                    (*rpitr)->pParentPlaylist = this;

                                    if ((*rpitr)->EncKey != nullptr && lastEncKey != nullptr && (*rpitr)->EncKey->IsEqual(lastEncKey) && lastEncKey->cpCryptoKey != nullptr)
                                        (*rpitr)->EncKey = lastEncKey;
                                    else
                                        lastEncKey = (*rpitr)->EncKey;
                                    LOG("Adding " << (*rpitr)->SequenceNumber);
                                    if (ApplyDiscontinuity)
                                        (*rpitr)->Discontinous = true;
                                    Segments.push_back(*rpitr);
                                }
                            }
                            BaseSequenceNumber = mincur == nullptr ? Segments.front()->SequenceNumber : mincur->SequenceNumber;
                        }

                        this->LastLiveRefreshProcessed = !spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                        if (Changed)
                        {
                            auto cdur = 0ULL; //recalc durations
                            for (auto itr : Segments)
                            {

                                itr->CumulativeDuration = cdur + itr->Duration;
                                cdur = itr->CumulativeDuration;
                                if (itr->EncKey != nullptr)
                                    itr->EncKey->pParentPlaylist = this;
                            }


                            LOG("*** END MERGE ***");
                        }

                        if (SlidingWindowStart != nullptr && Segments.size() > 0)
                        {
                            SlidingWindowEnd = std::make_shared<Timestamp>(SlidingWindowStart->ValueInTicks + Segments.back()->CumulativeDuration);
                            SlidingWindowChanged = true;
                        }
                    }
                }
                else
                {
                    this->LastLiveRefreshProcessed = !spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                }

                this->szData = std::move(spPlaylistRefresh->szData);
                this->LastModified = std::move(spPlaylistRefresh->LastModified);
                this->ETag = std::move(spPlaylistRefresh->ETag);
                spPlaylistRefresh.reset();

            }
            else
            {
                if (spPlaylistRefresh != nullptr)
                {
                    this->LastModified = std::move(spPlaylistRefresh->LastModified);
                    this->ETag = std::move(spPlaylistRefresh->ETag);
                    spPlaylistRefresh.reset();
                }
            }

            if (SlidingWindowChanged)
            {

                this->cpMediaSource->protectionRegistry.Register(task<HRESULT>([this]()
                {
                    if (this->cpMediaSource != nullptr && //do not raise for alternate renditions
                        this->cpMediaSource->GetCurrentState() != MSS_ERROR && this->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
                        this->cpMediaSource->cpController != nullptr && this->cpMediaSource->cpController->GetPlaylist() != nullptr)
                        this->cpMediaSource->cpController->GetPlaylist()->RaiseSlidingWindowChanged(this);

                    return S_OK;
                }, task_options(task_continuation_context::use_arbitrary())));

            }

        }
        catch (...)
        {


        }
    }
    if (!this->LastLiveRefreshProcessed && //if this is the last playlist in the program this will be marked non-Live since we should find the EXE-X-ENDLIST - in that case we stop the stopwatch    
        cpMediaSource->GetCurrentState() != MSS_STOPPED && cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED && cpMediaSource->GetCurrentState() != MSS_PAUSED) //this is the active playlist
    {

        if (Changed)
            SetStopwatchForNextPlaylistRefresh(this->Segments.back()->Duration);
        else
            SetStopwatchForNextPlaylistRefresh((unsigned long long)(this->DerivedTargetDuration / 2));
    }


    return Changed;
}

bool Playlist::MergeAlternateRenditionPlaylist()
{
    bool Changed = false;
    if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
    {
        std::unique_lock<std::recursive_mutex> lockmerge(LockMerge, std::defer_lock);
        if (IsLive)
        {
            lockmerge.try_lock();
            if (lockmerge.owns_lock() == false)
                return false;
        }


        try
        {

            if (pParentRendition->spPlaylistRefresh != nullptr && pParentRendition->spPlaylistRefresh->IsValid)
            {
                if (pParentRendition->spPlaylistRefresh->Segments.size() > 0 &&
                    pParentRendition->spPlaylistRefresh->Segments.back()->GetSequenceNumber() > Segments.back()->GetSequenceNumber())
                {
                    std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
                    if (IsLive)
                    {
                        listlock.try_lock();
                        if (listlock.owns_lock() == false)
                            return false;
                    }
                    bool ApplyDiscontinuity = false;
                    std::shared_ptr<EncryptionKey> lastEncKey = nullptr;
                    //get the min current segment
                    auto mincur = MinCurrentSegment();

                    //refresh has segments older than where we are playing now - we retain upto that
                    if ((mincur != nullptr && pParentRendition->spPlaylistRefresh->Segments.size() > 0 &&
                        mincur->GetSequenceNumber() > pParentRendition->spPlaylistRefresh->Segments.front()->GetSequenceNumber()) || mincur == nullptr)
                        mincur = GetSegment(pParentRendition->spPlaylistRefresh->Segments.front()->GetSequenceNumber());

                    if (PlaylistType != Microsoft::HLSClient::HLSPlaylistType::EVENT && cpMediaSource->spRootPlaylist->ActiveVariant->spPlaylist->PlaylistType != Microsoft::HLSClient::HLSPlaylistType::EVENT)
                    {
                        if (mincur != nullptr)
                        {
                            //we drop everything before mincur that is not currently in downloading state
                            while (Segments.size() > 0 && Segments.front()->SequenceNumber != mincur->SequenceNumber && Segments.front()->GetCurrentState() != DOWNLOADING)
                            {
                                if (!Changed) Changed = true;
                                LOG("Dropping " << Segments.front()->SequenceNumber);


                                if (ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                    ApplyDiscontinuity = true;
                                Segments.erase(Segments.begin());
                            }
                            lastEncKey = mincur->EncKey;
                        }
                        else
                        {
                            if (Segments.size() > 0 && ApplyDiscontinuity == false && (Segments.front()->Discontinous || Segments.back()->Discontinous))
                                ApplyDiscontinuity = true;

                            Segments.clear();
                            if (!Changed) Changed = true;
                            LOG("Dropping all segments");
                        }
                    }
                    if (pParentRendition->spPlaylistRefresh->Segments.size() > 0)
                    {
                        //we skip everything in the new playlist until we find an entry in the 
                        auto startaddingat = Segments.size() == 0 ? pParentRendition->spPlaylistRefresh->Segments.begin() :
                            std::find_if(pParentRendition->spPlaylistRefresh->Segments.begin(), pParentRendition->spPlaylistRefresh->Segments.end(), [this](shared_ptr<MediaSegment> seg)
                        {
                            return seg->SequenceNumber > Segments.back()->SequenceNumber;
                        });
                        if (startaddingat != pParentRendition->spPlaylistRefresh->Segments.end())
                        {
                            if (!Changed) Changed = true;
                            for (auto rpitr = startaddingat; rpitr != pParentRendition->spPlaylistRefresh->Segments.end(); ++rpitr)
                            {
                                (*rpitr)->pParentPlaylist = this;

                                if ((*rpitr)->EncKey != nullptr && lastEncKey != nullptr && (*rpitr)->EncKey->IsEqual(lastEncKey) && lastEncKey->cpCryptoKey != nullptr)
                                    (*rpitr)->EncKey = lastEncKey;
                                else
                                    lastEncKey = (*rpitr)->EncKey;
                                LOG("Adding " << (*rpitr)->SequenceNumber);
                                if (ApplyDiscontinuity)
                                    (*rpitr)->Discontinous = true;
                                Segments.push_back(*rpitr);
                            }
                        }
                        BaseSequenceNumber = mincur == nullptr ? Segments.front()->SequenceNumber : mincur->SequenceNumber;
                    }

                    this->LastLiveRefreshProcessed = !pParentRendition->spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                    if (Changed)
                    {
                        auto cdur = 0ULL; //recalc durations
                        for (auto itr : Segments)
                        {
                            itr->CumulativeDuration = cdur + itr->Duration;
                            cdur = itr->CumulativeDuration;
                            if (itr->EncKey != nullptr)
                                itr->EncKey->pParentPlaylist = this;
                        }

                        LOG("*** END MERGE ***");
                    }

                }
                else
                {
                    this->LastLiveRefreshProcessed = !pParentRendition->spPlaylistRefresh->IsLive;//change live flag - is meanigful if this is the last playlist in the program - this will be marked non-Live since we should find the EXE-X-ENDLIST
                }

                this->szData = std::move(pParentRendition->spPlaylistRefresh->szData);
                this->LastModified = std::move(pParentRendition->spPlaylistRefresh->LastModified);
                this->ETag = std::move(pParentRendition->spPlaylistRefresh->ETag);
                this->pParentRendition->spPlaylistRefresh.reset();

            }
            else
            {
                if (pParentRendition->spPlaylistRefresh != nullptr)
                {
                    this->LastModified = std::move(pParentRendition->spPlaylistRefresh->LastModified);
                    this->ETag = std::move(pParentRendition->spPlaylistRefresh->ETag);
                    this->pParentRendition->spPlaylistRefresh.reset();
                }
            }
        }
        catch (...)
        {

        }
    }

    return Changed;
}

void Playlist::MakeDiscontinous()
{
    std::lock_guard<recursive_mutex> lock(LockSegmentList);
    for (auto itr : Segments)
        itr->Discontinous = true;
}

///<summary>Parse a playlist</summary>
void Playlist::Parse()
{

    std::vector<std::wstring> lines;
    std::wistringstream datastream(szData);

    do
    {
        std::wstring buff;
        //get a single line of text (terminated by either CR/LF or just LF)
        std::getline<wchar_t, char_traits<wchar_t>, allocator<wchar_t>>(datastream, buff, '\n');
        //empty line
        if (buff.empty() || buff.size() == 0)
            continue;
        //remove the carriage return and trim the string to remove and leading or trailing white spaces
        buff = Helpers::Trim(Helpers::RemoveCarriageReturn(buff));
        //empty string
        if (buff.empty())
            continue;
        else
            //store for processing
            lines.push_back(buff);
    } while (!datastream.fail() && !datastream.eof());

    if (lines.size() == 0) return;

    //parse the vector of lines
    ParseTags(lines);

    //if the playlist is a variant master, process renditions (if any) and set the default bitrate bounds
    if (IsVariant)
    {
        AssignRenditions();
        MinAllowedBitrate = Variants.begin()->first;
        MaxAllowedBitrate = Variants.rbegin()->first;
        StartBitrate = BitratesInPlaylistOrder.front();
    }
    //if not a variant master and there is a parent variant master
    if (!IsVariant && pParentStream != nullptr)
    {
        if (!IsLive)
            //set the total duration of the parent to be the same as this one - if not live
            pParentStream->pParentPlaylist->TotalDuration = this->TotalDuration;

        pParentStream->pParentPlaylist->PlaylistType = PlaylistType;

    }

    if (!IsVariant && this->Segments.size() > 0 && IsLive)
    {
        if (Configuration::GetCurrent()->MinimumLiveLatency == 0)
            Configuration::GetCurrent()->MinimumLiveLatency = 4 * PlaylistTargetDuration;
        else if (Configuration::GetCurrent()->MinimumLiveLatency < 3 * PlaylistTargetDuration) //clamp
            Configuration::GetCurrent()->MinimumLiveLatency = 3 * PlaylistTargetDuration;
    }
    //is the target duration missing ?
    if (this->DerivedTargetDuration == 0.0 && this->Segments.size() > 0)
    {
        if (this->IsLive == false)
        {
            this->DerivedTargetDuration = (unsigned long long)floor(this->TotalDuration / this->Segments.size());
        }
        else
        {
            unsigned int offsetfromtail = 0;
            unsigned long long livewindowduration = 0;
            auto livestart = this->FindLiveStartSegmentSequenceNumber(offsetfromtail, livewindowduration);
            this->DerivedTargetDuration = (unsigned long long)floor(livewindowduration / offsetfromtail);
        }
    }

    if (!IsLive)//clear the buffer - if live - it will be clared the next time playlist refreshes
        szData.clear();
    if (!IsVariant && this->Segments.size() > 0)
    {
        SetLABThreshold();
    }

    return;
}

void Playlist::SetLABThreshold()
{
    unsigned long long targetval = DerivedTargetDuration > 0 ? (unsigned long long)(DerivedTargetDuration) : Segments.front()->Duration;
    if (Configuration::GetCurrent()->LABLengthInTicks < targetval)
    {
        Configuration::GetCurrent()->SetLABLengthInTicks((unsigned long long)(targetval * 4));
    }


    auto prefetchticks = __max(Configuration::GetCurrent()->PreFetchLengthInTicks, DerivedTargetDuration < 50000000 ? 50000000 : 0);
    if (prefetchticks != 0)
        Configuration::GetCurrent()->SetLABLengthInTicks(Configuration::GetCurrent()->LABLengthInTicks + prefetchticks);

}

///<summary>Associates rendition information from the parent playlist to individual variant playlists</summary>
void Playlist::AssignRenditions()
{
    //no renditions
    if (this->AudioRenditions.empty() && this->VideoRenditions.empty() && this->SubtitleRenditions.empty())
        return;
    //loop through the variant children
    for (auto itr : this->Variants)
    {
        //if the variant has an audio renditon group ID and we find entries with that group ID in our list of audio renditions
        if (itr.second->HasAudioRenditionGroup && this->AudioRenditions.find(itr.second->AudioRenditionGroupID) != this->AudioRenditions.end())
        {
            //we associate the list of audio renditions with matching group ID to the variant child
            itr.second->AudioRenditions = this->AudioRenditions[itr.second->AudioRenditionGroupID];
            auto FindDefault = std::find_if(itr.second->AudioRenditions->begin(), itr.second->AudioRenditions->end(), [](shared_ptr<Rendition> spRen)
            {
                return spRen->Default == true && !spRen->PlaylistUri.empty();
            });
            if (FindDefault != itr.second->AudioRenditions->end())
                itr.second->SetActiveAudioRendition(FindDefault->get());
        }
        //if the variant has a video renditon group ID and we find entries with that group ID in our list of video renditions
        if (itr.second->HasVideoRenditionGroup && this->VideoRenditions.find(itr.second->VideoRenditionGroupID) != this->VideoRenditions.end())
        {
            //we associate the list of video renditions with matching group ID to the variant child
            itr.second->VideoRenditions = this->VideoRenditions[itr.second->VideoRenditionGroupID];
            auto FindDefault = std::find_if(itr.second->VideoRenditions->begin(), itr.second->VideoRenditions->end(), [](shared_ptr<Rendition> spRen)
            {
                return spRen->Default == true && !spRen->PlaylistUri.empty();
            });
            if (FindDefault != itr.second->VideoRenditions->end())
                itr.second->SetActiveVideoRendition(FindDefault->get());
        }
        //if the variant has an subtitle renditon group ID and we find entries with that group ID in our list of subtitle renditions
        if (itr.second->HasSubtitlesRenditionGroup && this->SubtitleRenditions.find(itr.second->SubtitlesRenditionGroupID) != this->SubtitleRenditions.end())
        {
            //we associate the list of subtitle renditions with matching group ID to the variant child
            itr.second->SubtitleRenditions = this->SubtitleRenditions[itr.second->SubtitlesRenditionGroupID];
        }
    }

    //clear the list of renditions on the master playlist
    this->AudioRenditions.clear();
    this->VideoRenditions.clear();
    this->SubtitleRenditions.clear();
}

///<summary>Playlist tag parser</summary>
///<param name='lines'>A vector of strings - where each string represents a non empty line in the playlist</param>
void Playlist::ParseTags(std::vector<std::wstring>& lines)
{
    std::shared_ptr<EncryptionKey> lastKey = nullptr;
    shared_ptr<Timestamp> lastPDT = nullptr;
    bool HitDisconinuity = false;
    bool StartDiscontinuity = false;
    shared_ptr<MediaSegment> pendingms = nullptr;
    std::vector<std::wstring> UnresolvedPreTagsForFirstSegment;
    PlaylistType = Microsoft::HLSClient::HLSPlaylistType::SLIDINGWINDOW;
    //loop through the lines
    for (std::vector<std::wstring>::iterator itr = begin(lines); itr != end(lines); itr++)
    {
        if ((*itr).empty()) continue;

        //read the tag name
        std::wstring tagName = Helpers::ReadTagName(*itr);

        if (tagName.empty() == true //not a tag
            && pendingms != nullptr) //we have a mediasegment waiting for a URL entry
        {
            //assume this is a URL entry 
            pendingms->SetUri(*itr);
            if (lastKey != nullptr) //associate the DRM key if any
                pendingms->EncKey = lastKey;
            //set sequence number
            pendingms->SetSequenceNumber(static_cast<unsigned int>(Segments.size()) + BaseSequenceNumber);
            //add up the duration to set cumulative duration
            pendingms->CumulativeDuration = Segments.size() > 0 ?
                GetNextSegment(pendingms->SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE)->CumulativeDuration + pendingms->Duration :
                pendingms->Duration;

            //if there is a segment before this and it has unresolved tags post segment - make them pre-segment for this one 
            if (Segments.size() > 0 && Segments.back()->UnresolvedTags.find(UnresolvedTagPlacement::PostSegment) != Segments.back()->UnresolvedTags.end())
            {
                pendingms->UnresolvedTags[UnresolvedTagPlacement::PreSegment].insert(pendingms->UnresolvedTags[UnresolvedTagPlacement::PreSegment].begin(), Segments.back()->UnresolvedTags[UnresolvedTagPlacement::PostSegment].begin(), Segments.back()->UnresolvedTags[UnresolvedTagPlacement::PostSegment].end());
            }
            else if (this->Segments.size() == 0 && UnresolvedPreTagsForFirstSegment.size() > 0)
            {
                pendingms->UnresolvedTags[UnresolvedTagPlacement::PreSegment].insert(pendingms->UnresolvedTags[UnresolvedTagPlacement::PreSegment].begin(), UnresolvedPreTagsForFirstSegment.begin(), UnresolvedPreTagsForFirstSegment.end());
                UnresolvedPreTagsForFirstSegment.clear();
            }
            //store
            this->Segments.push_back(pendingms);

            //find the timestamp boundaries
            pendingms->SetPTSBoundaries();
            if (StartDiscontinuity)
            {
                pendingms->StartsDiscontinuity = true;
                StartDiscontinuity = false;
            }
            if (HitDisconinuity)
                pendingms->Discontinous = true;

            if (lastPDT != nullptr)
            {
                pendingms->ProgramDateTime = lastPDT;
                lastPDT = make_shared<Timestamp>(lastPDT->ValueInTicks + pendingms->Duration);
            }

            TotalDuration += (pendingms->Duration);//increment total playlist duration
            pendingms = nullptr;
        }
        else if (tagName == TAGNAME::EXTM3U)
        {
            //valid playlist
            this->IsValid = true;
            continue;
        }
        else if (tagName == TAGNAME::EXT_X_PROGRAM_DATE_TIME)
        {
            std::wstring szPDT;
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, szPDT);
            if (szPDT.empty() == false)
            {
                unsigned long long pdtval = 0;
                auto valid = Helpers::ToProgramDateTime(szPDT, pdtval);
                if (valid)
                    lastPDT = make_shared<Timestamp>(pdtval);
            }

        }
        else if (tagName == TAGNAME::EXT_X_ENDLIST)
        {
            IsLive = false;
            if (!this->IsVariant && this->pParentStream != nullptr && this->pParentStream->spPlaylist.get() == this)//only if this is not a live playlist (in case of live this instance will be the refresh instance will not match the spPlaylist pointer in value)
                this->pParentStream->pParentPlaylist->IsLive = false;
            PlaylistType = Microsoft::HLSClient::HLSPlaylistType::VOD;
        }
        else if (tagName == TAGNAME::EXT_X_DISCONTINUITY)
        {
            HitDisconinuity = true;
            StartDiscontinuity = true;
        }
        else if (tagName == TAGNAME::EXT_X_VERSION)
        {
            //read and store version
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, Version);
        }
        else if (tagName == TAGNAME::EXT_X_TARGETDURATION)
        {
            //read and store duration 
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, PlaylistTargetDuration);
            PlaylistTargetDuration = (unsigned long long)(PlaylistTargetDuration * 10000000);
        }
        else if (tagName == TAGNAME::EXT_X_ALLOW_CACHE)
        {
            //read and store the allow cache directive
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, AllowCache);
        }
        else if (tagName == TAGNAME::EXT_X_PLAYLIST_TYPE)
        {
            wstring pltype;
            //read and store the allow cache directive
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, pltype);
            auto pltypeu = Helpers::ToUpper(pltype);
            PlaylistType = (pltypeu == L"EVENT" ? Microsoft::HLSClient::HLSPlaylistType::EVENT : (pltypeu == L"VOD" ? Microsoft::HLSClient::HLSPlaylistType::VOD : Microsoft::HLSClient::HLSPlaylistType::UNKNOWN));

        }
        else if (tagName == TAGNAME::EXT_X_MEDIA_SEQUENCE)
        {
            //read and store the base sequence number
            Helpers::ReadAttributeValueFromPosition(Helpers::ReadAttributeList(*itr), 0, BaseSequenceNumber);
        }
        else if (tagName == TAGNAME::EXT_X_MEDIA)//alternate renditions
        {
            //create a Rendition
            auto ren = std::make_shared<Rendition>(*itr, this);
            //if audio
            if (ren->Type == Rendition::TYPEAUDIO)
            {
                //if we have not found this group ID before
                if (AudioRenditions.find(ren->GroupID) == AudioRenditions.end())
                {
                    //make the vector to store renditions for this group
                    AudioRenditions[ren->GroupID] = std::make_shared<std::vector<std::shared_ptr<Rendition>>>();
                }
                //store the rendition in the vector for its group
                AudioRenditions[ren->GroupID]->push_back(ren);
            }
            //if video
            else if (ren->Type == Rendition::TYPEVIDEO)
            {
                if (VideoRenditions.find(ren->GroupID) == VideoRenditions.end())
                {
                    VideoRenditions[ren->GroupID] = std::make_shared<std::vector<std::shared_ptr<Rendition>>>();
                }
                VideoRenditions[ren->GroupID]->push_back(ren);
            }
            //if subtitle
            else if (ren->Type == Rendition::TYPESUBTITLES)
            {
                if (SubtitleRenditions.find(ren->GroupID) == SubtitleRenditions.end())
                {
                    SubtitleRenditions[ren->GroupID] = std::make_shared<std::vector<std::shared_ptr<Rendition>>>();
                }
                SubtitleRenditions[ren->GroupID]->push_back(ren);
            }

        }
        else if (tagName == TAGNAME::EXT_X_KEY) //AES-128 key
        {
            //store it temporarily
            lastKey = std::make_shared<EncryptionKey>((*itr), this);
        }
        else if (tagName == TAGNAME::EXT_X_STREAM_INF) //variant playlist entry
        {
            //make StreamInfo instance
            auto si = std::make_shared<StreamInfo>((*itr), *(itr + 1), this);
            auto found = Variants.find(si->Bandwidth);
            //if we have already encountered this bandwidth - this is a backup playlist
            if (found != Variants.end())
            {
                found->second->AddBackupPlaylistUri(*(itr + 1));
            }
            else
            {
                //store it keyed by bitrate
                BitratesInPlaylistOrder.push_back(si->Bandwidth);
                Variants.insert(std::pair<unsigned int, std::shared_ptr<StreamInfo>>(si->Bandwidth, si));
                //ASSUMPTION : EXT-X-STREAM-INF is immediately followed by a playlist URI 
                itr += 1; //consumed next line as well
                if (!IsVariant) //make playlist as variant master
                {
                    IsVariant = true;
                }
            }
        }
        else if (tagName == TAGNAME::EXT_X_BYTERANGE && pendingms != nullptr)
        {
            pendingms->SetByteRangeInfo(*itr);
        }
        else if (tagName == TAGNAME::EXTINF) //entry for a media segment
        {
            pendingms = std::make_shared<MediaSegment>(*itr, this);
        }
        else if (tagName == TAGNAME::EXT_X_I_FRAMES_ONLY) //TBD
        {

        }
        else if (tagName == TAGNAME::EXT_X_I_FRAMES_STREAM_INF) //TBD
        {

        }
        else if (!IsVariant) //this is a #EXT tag - but we do not interpret it - we do this only for child playlists
        {
            if (pendingms != nullptr)
            {
                pendingms->UnresolvedTags[UnresolvedTagPlacement::WithSegment].push_back(*itr);
            }
            else //no segment is being processed now
            {
                if (Segments.size() > 0)
                {
                    Segments.back()->UnresolvedTags[UnresolvedTagPlacement::PostSegment].push_back(*itr);
                }
                else
                {
                    UnresolvedPreTagsForFirstSegment.push_back(*itr);
                }
            }
        }
    }
}


shared_ptr<Timestamp> Playlist::FindApproximateLivePosition()
{
    if (!IsLive || IsVariant) return 0;

    if (Segments.size() == 0) return nullptr;

    auto startseq = FindLiveStartSegmentSequenceNumber();
    auto startseg = GetSegment(startseq);
    //safety net
    if (startseg == nullptr)
        startseg = Segments.front();

    if (startseg->GetCurrentState() == INMEMORYCACHE)
        return startseg->Discontinous ? make_shared<Timestamp>(startseg->TSAbsoluteToDiscontinousAbsolute(startseg->Timeline.front()->ValueInTicks)) : startseg->Timeline.front();
    else
    { //offset quarter sec 
        auto livestartsegminusone = GetNextSegment(startseq, MFRATE_REVERSE);

        return livestartsegminusone != nullptr ? make_shared<Timestamp>((SlidingWindowStart != nullptr ? SlidingWindowStart->ValueInTicks : 0) + livestartsegminusone->CumulativeDuration + 2500000) : (SlidingWindowStart != nullptr ? make_shared<Timestamp>(SlidingWindowStart->ValueInTicks) : nullptr); //offset quarter sec 
    }
}

unsigned int Playlist::FindLiveStartSegmentSequenceNumber()
{
    if (!IsLive || IsVariant) return 0;
    //per HLS spec we should not start playback any later than at least 3 DerivedTargetDuration lengths away from the end of the playlist. 
    //We simplify it a little and say 3 segment lengths away. However if there is a pre fetch settings then we take the max (3 segment length, prefetch duration).
    auto segcount = Segments.size();
    unsigned int OffsetFromTail = (Configuration::GetCurrent()->PreFetchLengthInTicks > 0) ? (unsigned int)ceil((Configuration::GetCurrent()->PreFetchLengthInTicks + Configuration::GetCurrent()->MinimumLiveLatency) / PlaylistTargetDuration) : (unsigned int)ceil(Configuration::GetCurrent()->MinimumLiveLatency / PlaylistTargetDuration);
    //auto startSeg = segcount > OffsetFromTail ? *(Segments.rbegin() + (OffsetFromTail - 1)) : Segments.front();
    auto startSeg = segcount > OffsetFromTail ? Segments.at(segcount - OffsetFromTail) : Segments.front();
    auto ret = startSeg->SequenceNumber;
    return ret;
}

unsigned int Playlist::FindLiveStartSegmentSequenceNumber(unsigned int& offsetFromTail, unsigned long long& liveWindowDuration)
{
    if (!IsLive || IsVariant) return 0;
    //per HLS spec we should not start playback any later than at least 3 DerivedTargetDuration lengths away from the end of the playlist. 
    //We simplify it a little and say 3 segment lengths away. However if there is a pre fetch settings then we take the max (3 segment length, prefetch duration).
    auto segcount = Segments.size();
    offsetFromTail = (Configuration::GetCurrent()->PreFetchLengthInTicks > 0) ? (unsigned int)ceil((Configuration::GetCurrent()->PreFetchLengthInTicks + Configuration::GetCurrent()->MinimumLiveLatency) / PlaylistTargetDuration) : (unsigned int)ceil(Configuration::GetCurrent()->MinimumLiveLatency / PlaylistTargetDuration);
    auto startSeg = segcount > offsetFromTail ? *(Segments.rbegin() + (offsetFromTail - 1)) : Segments.front();
    offsetFromTail = segcount > offsetFromTail ? offsetFromTail : (unsigned int)Segments.size();
    auto ret = startSeg->SequenceNumber;
    liveWindowDuration = segcount > offsetFromTail ? Segments.back()->CumulativeDuration - startSeg->CumulativeDuration + startSeg->Duration : Segments.back()->CumulativeDuration;
    return ret;
}

unsigned int Playlist::FindAltRenditionMatchingSegment(Playlist* mainPlaylist, unsigned int SequenceNumber)
{

    Playlist::StartStreamingAsync(this, SequenceNumber, false, true, true).get();
    return SequenceNumber;
}

unsigned int Playlist::FindAltRenditionMatchingSegment(Playlist* mainPlaylist, unsigned int SequenceNumber, bool& EndsBeforeMain)
{
    auto mainseg = mainPlaylist->GetSegment(SequenceNumber);

    unsigned int targetseq = 0;


    if (mainseg->ProgramDateTime != nullptr && Segments.front()->ProgramDateTime != nullptr)
    {
        auto matchingseg = std::find_if(begin(this->Segments), end(this->Segments), [mainseg](shared_ptr<MediaSegment> ms)
        {
            return ms->ProgramDateTime->ValueInTicks + ms->Duration >= mainseg->ProgramDateTime->ValueInTicks;
        });
        if (matchingseg == end(this->Segments)) //something is wrong
            targetseq = SequenceNumber;
        else
        {
            targetseq = (*matchingseg)->SequenceNumber;
        }
    }
    else
    {
        if (SequenceNumber == mainPlaylist->Segments.front()->GetSequenceNumber())
            targetseq = this->Segments.front()->SequenceNumber;

        auto maintracklowerbound = mainPlaylist->GetNextSegment(SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE)->CumulativeDuration;
        auto nextseg = mainPlaylist->GetNextSegment(SequenceNumber, MFRATE_DIRECTION::MFRATE_FORWARD);
        auto maintrackupperbound = nextseg != nullptr ? nextseg->CumulativeDuration : MAXULONGLONG;

        {
            std::lock_guard<recursive_mutex> lock(LockSegmentList);
            auto matchingseg = std::find_if(begin(this->Segments), end(this->Segments), [maintracklowerbound, maintrackupperbound](shared_ptr<MediaSegment> ms)
            {
                return ms->CumulativeDuration >= maintracklowerbound && ms->CumulativeDuration <= maintrackupperbound;
            });
            if (matchingseg == end(this->Segments)) //something is wrong
                targetseq = SequenceNumber;
            else
                targetseq = (*matchingseg)->SequenceNumber;
        }
    }



    LOG("FindAltRenditionMatchingSegment: Main : " << SequenceNumber << " matched to " << targetseq);

    EndsBeforeMain = GetSegment(targetseq)->CumulativeDuration < mainseg->CumulativeDuration;

    return targetseq;
}

///<summary>Set the current position on all playing streams on the given playlist. Position is set to the nearest timestamp.</summary>
///<param name='pPlaylist'>The target playlist</param>
///<param name='PositionInTicks'>The desired position in ticks</param>
///<returns>Tuple. First value is the success indicator. Second value is a map of the final positions set for each content type keyed by content type.</returns>
tuple<HRESULT, std::map<ContentType, unsigned long long>> Playlist::SetCurrentPositionVOD(Playlist *pPlaylist, unsigned long long RelativePositionInTicks)
{


    //cancel any pending bitrate notification - we do not want to muddle our seek logic
    pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);
    //lock to prevent any incoming bitrate changes
    std::lock(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
    std::lock_guard<std::recursive_mutex> lockV(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
    std::lock_guard<std::recursive_mutex> lockA(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

    tuple<HRESULT, std::map<ContentType, unsigned long long>> retval;
    unsigned long long finalpos = 0;
    shared_ptr<MediaSegment> TargetSeg = nullptr;

    //if the requested position is beyond the presentation - return end of stream indicator
    if (RelativePositionInTicks >= pPlaylist->TotalDuration)
    {
        finalpos = pPlaylist->TotalDuration;
        TargetSeg = pPlaylist->Segments.back();
    }
    else
    {
        finalpos = RelativePositionInTicks;
        TargetSeg = pPlaylist->GetSegmentAtTime(RelativePositionInTicks);
    }


    //set the segment index trackers - this may not set correctly if the playlist did not have any trackers to begin with - we correct that after the segment is downloaded
    for (auto itr = pPlaylist->CurrentSegmentTracker.begin(); itr != pPlaylist->CurrentSegmentTracker.end(); itr++)
        pPlaylist->SetCurrentSegmentTracker(itr->first, TargetSeg);


    if (pPlaylist->pParentRendition == nullptr) //main track
    {
        if (TargetSeg->GetCurrentState() != MediaSegmentState::INMEMORYCACHE) //only do this if this is the main track
        {
            StartStreamingAsync(pPlaylist, TargetSeg->SequenceNumber, true, true).wait();//wait() or get() ?

            if (TargetSeg->Discontinous)
            {
                auto prevseg = pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), MFRATE_DIRECTION::MFRATE_REVERSE);
                if (prevseg != nullptr)
                    TargetSeg->UpdateSampleDiscontinuityTimestamps(TargetSeg->TSRelativeToAbsolute(prevseg->CumulativeDuration)->ValueInTicks);
            }
        }
        else
            //rewind the segment
            TargetSeg->RewindInCacheSegment();

        auto nextSeg = pPlaylist->GetNextSegment(TargetSeg->SequenceNumber, pPlaylist->cpMediaSource->GetCurrentDirection());
        if (nextSeg != nullptr)//not first or last segment
        {

            auto prefetchcount = pPlaylist->NeedsPreFetch();

            if (prefetchcount > 0)
            {
                pPlaylist->BulkFetch(nextSeg, prefetchcount - 1, pPlaylist->cpMediaSource->GetCurrentDirection());
            }
            else
            {
                //get the playback time we will have left in this segment one we set the requested position
                auto remainder = TargetSeg->GetSegmentLookahead(finalpos, pPlaylist->cpMediaSource->GetCurrentDirection());

                float appxtimeestimatefornextsegdownload = pPlaylist->cpMediaSource->spHeuristicsManager->GetLastSuggestedBandwidth() != 0 ?
                    (10000000 * ((float)TargetSeg->LengthInBytes / (float)(pPlaylist->cpMediaSource->spHeuristicsManager->GetLastSuggestedBandwidth()))) * ((float)nextSeg->Duration / (float)TargetSeg->Duration) + 10000000 : //1 sec safety buffer;
                    TargetSeg->Duration / 2;

                //if the remainder left will be at lease a second more than what we estimate the next segment willl take to download
                if (remainder > appxtimeestimatefornextsegdownload)
                {
                    //we start chaining from the next segment, but do not wait
                    StartStreamingAsync(pPlaylist, pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), pPlaylist->cpMediaSource->GetCurrentDirection())->GetSequenceNumber(), true, false);
                }
                else
                {
                    //we start chaining from the next segment and wait for it to be downloaded
                    StartStreamingAsync(pPlaylist, pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), pPlaylist->cpMediaSource->GetCurrentDirection())->GetSequenceNumber(), true, true).wait();//wait() or get() ?
                }
            }
        }
    }
    else
    {
        auto TargetMainSeg = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetSegmentAtTime(RelativePositionInTicks);
        //only do this if the alternate segment has not been downloaded yet
        if (TargetSeg->GetCurrentState() != MediaSegmentState::INMEMORYCACHE)
        {
            auto prefetchcount = pPlaylist->NeedsPreFetch();

            if (prefetchcount > 0)
            {
                pPlaylist->BulkFetch(TargetSeg, prefetchcount, pPlaylist->cpMediaSource->GetCurrentDirection());
            }
            else
            {
                StartStreamingAsync(pPlaylist, TargetSeg->SequenceNumber, TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration, true, true).wait();//wait() or get() ?

                if (TargetSeg->Discontinous)
                {
                    auto prevseg = pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), MFRATE_DIRECTION::MFRATE_REVERSE);
                    if (prevseg != nullptr)
                        TargetSeg->UpdateSampleDiscontinuityTimestamps(TargetSeg->TSRelativeToAbsolute(prevseg->CumulativeDuration)->ValueInTicks);
                }
            }
        }
        else
        {
            auto prefetchcount = pPlaylist->NeedsPreFetch();
            auto nextseg = pPlaylist->GetNextSegment(TargetSeg->SequenceNumber, pPlaylist->cpMediaSource->curDirection);
            if (nextseg != nullptr)
            {
                if (prefetchcount > 0)
                {
                    pPlaylist->BulkFetch(nextseg, prefetchcount - 1, pPlaylist->cpMediaSource->GetCurrentDirection());
                }
                else if (TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration)
                {
                    StartStreamingAsync(pPlaylist, nextseg->GetSequenceNumber(), TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration, true, true);//wait() or get() ?
                }
            }

            TargetSeg->RewindInCacheSegment();
        }
    }


    if (Configuration::GetCurrent()->ForceKeyFrameMatchOnSeek &&
        finalpos != pPlaylist->TotalDuration && TargetSeg->MediaTypePIDMap.find(VIDEO) != TargetSeg->MediaTypePIDMap.end())
    {

        auto sd = TargetSeg->FindNearestIDRSample(pPlaylist->TSRelativeToAbsolute(RelativePositionInTicks)->ValueInTicks,
            TargetSeg->GetPIDForMediaType(VIDEO),
            pPlaylist->cpMediaSource->GetCurrentDirection());


        if (sd != nullptr)
            finalpos = (TargetSeg->Discontinous) ? pPlaylist->TSAbsoluteToRelative(sd->DiscontinousTS)->ValueInTicks : pPlaylist->TSAbsoluteToRelative(sd->SamplePTS)->ValueInTicks;

    }


    //finally set the current position on the target segment
    std::map<ContentType, unsigned long long> retActualPosition;

    if (finalpos < pPlaylist->TotalDuration &&
        (
            (TargetSeg->Discontinous == false && TargetSeg->EndPTSNormalized != nullptr && finalpos < TargetSeg->EndPTSNormalized->ValueInTicks) ||
            (TargetSeg->Discontinous && finalpos <= TargetSeg->GetLastSample()->DiscontinousTS->ValueInTicks)
            ))
    {
        retActualPosition = TargetSeg->SetCurrentPosition(finalpos, pPlaylist->cpMediaSource->GetCurrentDirection());
    }
    else //seeked past end
    {
        for (auto itr = TargetSeg->MediaTypePIDMap.begin(); itr != TargetSeg->MediaTypePIDMap.end(); itr++)
        {
            TargetSeg->AdvanceUnreadQueue(itr->second, nullptr);
            retActualPosition.emplace(std::pair<ContentType, unsigned long long>(itr->first, finalpos));
        }
    }


    LOG("SetCurrentPosition() : Segment = " << TargetSeg->SequenceNumber);
#if defined _VSLOG
    for (auto itm : retActualPosition)
    {
        LOG("SetCurrentPosition() : " << (itm.first == AUDIO ? "AUDIO = " : "VIDEO = ") << itm.second);
    }
#endif


    retval = tuple<HRESULT, std::map<ContentType, unsigned long long>>(S_OK, retActualPosition);

    //set stream tick counters if needed
    if (pPlaylist->cpMediaSource->cpVideoStream->Selected() && pPlaylist->cpMediaSource->cpAudioStream->Selected())
    {
        if (TargetSeg->HasMediaType(AUDIO) == false && std::get<1>(retval).find(VIDEO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[VIDEO]);
        if (TargetSeg->HasMediaType(VIDEO) == false && std::get<1>(retval).find(AUDIO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[AUDIO]);
    }

    return retval;
}

tuple<HRESULT, std::map<ContentType, unsigned long long>> Playlist::SetCurrentPositionLive(Playlist *pPlaylist, unsigned long long AbsolutePositionInTicks)
{
    std::unique_lock<std::recursive_mutex> lockV;
    std::unique_lock<std::recursive_mutex> lockA;


    if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING)
    {
        //cancel any pending bitrate notification - we do not want to muddle our seek logic
        pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);
        //lock to prevent any incoming bitrate changes
        std::lock(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
        lockV = std::unique_lock<std::recursive_mutex>(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
        lockA = std::unique_lock<std::recursive_mutex>(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);
    }



    tuple<HRESULT, std::map<ContentType, unsigned long long>> retval;
    shared_ptr<Timestamp> finalpos = nullptr;
    shared_ptr<MediaSegment> TargetSeg = nullptr;


    auto livestartsegminusone = pPlaylist->GetNextSegment(pPlaylist->FindLiveStartSegmentSequenceNumber(), MFRATE_REVERSE);
    auto lastseg = pPlaylist->Segments.back();

    LOG(L"SetCurrentPositionLive : Requested: " << AbsolutePositionInTicks << L",Sliding Window Start: " << pPlaylist->SlidingWindowStart->ValueInTicks << L", 1st Segment Start: " << pPlaylist->Segments.front()->StartPTSNormalized->ValueInTicks);
    if (livestartsegminusone != nullptr)
    {
        if (AbsolutePositionInTicks < pPlaylist->SlidingWindowStart->ValueInTicks) //cannot move beyond sliding window start 
        {
            finalpos = nullptr;
            TargetSeg = pPlaylist->Segments.front();
        }
        else if (AbsolutePositionInTicks > pPlaylist->SlidingWindowStart->ValueInTicks + livestartsegminusone->CumulativeDuration) //cannot move beyond the live start position
        {
            finalpos = make_shared<Timestamp>(pPlaylist->SlidingWindowStart->ValueInTicks + livestartsegminusone->CumulativeDuration);
            TargetSeg = pPlaylist->GetSegment(pPlaylist->FindLiveStartSegmentSequenceNumber());
        }
        else
        {
            finalpos = make_shared<Timestamp>(AbsolutePositionInTicks);

            TargetSeg = pPlaylist->GetSegmentAtTime(AbsolutePositionInTicks);
            if (TargetSeg == nullptr)
            {
                finalpos = nullptr;
                TargetSeg = pPlaylist->GetSegment(pPlaylist->FindLiveStartSegmentSequenceNumber());
            }
        }
    }
    else
    {
        finalpos = nullptr;
        TargetSeg = pPlaylist->GetSegment(pPlaylist->FindLiveStartSegmentSequenceNumber());
    }



    //set the segment index trackers - this may not set correctly if the playlist did not have any trackers to begin with - we correct that after the segment is downloaded
    for (auto itr = pPlaylist->CurrentSegmentTracker.begin(); itr != pPlaylist->CurrentSegmentTracker.end(); itr++)
        pPlaylist->SetCurrentSegmentTracker(itr->first, TargetSeg);



    if (TargetSeg->GetCurrentState() != MediaSegmentState::INMEMORYCACHE) //only do this if this is the main track
        StartStreamingAsync(pPlaylist, TargetSeg->SequenceNumber, true, true).wait();//wait() or get() ?
    else
        //rewind the segment
        TargetSeg->RewindInCacheSegment();

    if (finalpos == nullptr)
        finalpos = TargetSeg->Timeline.front();

    auto nextseg = pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), pPlaylist->cpMediaSource->GetCurrentDirection());
    if (nextseg != nullptr)//not first or last segment
    {

        auto prefetchcount = pPlaylist->NeedsPreFetch();

        if (prefetchcount > 0)
        {
            pPlaylist->BulkFetch(nextseg, prefetchcount - 1, pPlaylist->cpMediaSource->GetCurrentDirection());
        }
        else
        {
            //get the playback time we will have left in this segment once we set the requested position
            auto remainder = TargetSeg->GetSegmentLookahead(finalpos->ValueInTicks, pPlaylist->cpMediaSource->GetCurrentDirection());

            float appxtimeestimatefornextsegdownload = pPlaylist->cpMediaSource->spHeuristicsManager->GetLastSuggestedBandwidth() != 0 ?
                (10000000 * ((float)TargetSeg->LengthInBytes / (float)(pPlaylist->cpMediaSource->spHeuristicsManager->GetLastSuggestedBandwidth()))) * ((float)nextseg->Duration / (float)TargetSeg->Duration) + 10000000 : //1 sec safety buffer;
                TargetSeg->Duration / 2;

            //if the remainder left will be at lease a second more than what we estimate the next segment willl take to download
            if (remainder > appxtimeestimatefornextsegdownload)
            {
                //we start chaining from the next segment, but do not wait
                StartStreamingAsync(pPlaylist, pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), pPlaylist->cpMediaSource->GetCurrentDirection())->GetSequenceNumber(), true, false);
            }
            else
            {
                //we start chaining from the next segment and wait for it to be downloaded
                StartStreamingAsync(pPlaylist, pPlaylist->GetNextSegment(TargetSeg->GetSequenceNumber(), pPlaylist->cpMediaSource->GetCurrentDirection())->GetSequenceNumber(), true, true).wait();//wait() or get() ?
            }
        }
    }


    if (Configuration::GetCurrent()->ForceKeyFrameMatchOnSeek &&
        //finalpos != pPlaylist->TotalDuration && 
        TargetSeg->MediaTypePIDMap.find(VIDEO) != TargetSeg->MediaTypePIDMap.end())
    {

        auto sd = TargetSeg->FindNearestIDRSample(finalpos->ValueInTicks,
            TargetSeg->GetPIDForMediaType(VIDEO),
            pPlaylist->cpMediaSource->GetCurrentDirection());
        if (sd != nullptr)
            finalpos = make_shared<Timestamp>(TargetSeg->Discontinous ? TargetSeg->TSAbsoluteToDiscontinousAbsolute(sd->SamplePTS->ValueInTicks) : sd->SamplePTS->ValueInTicks);
    }

    //finally set the current position on the target segment
    std::map<ContentType, unsigned long long> retActualPosition;

    retActualPosition = TargetSeg->SetCurrentPosition(finalpos->ValueInTicks, pPlaylist->cpMediaSource->GetCurrentDirection(), true);

    LOG("SetCurrentPosition() : Segment = " << TargetSeg->SequenceNumber);
#if defined _VSLOG
    for (auto itm : retActualPosition)
    {
        LOG("SetCurrentPosition() : " << (itm.first == AUDIO ? "AUDIO = " : "VIDEO = ") << itm.second);
    }
#endif


    retval = tuple<HRESULT, std::map<ContentType, unsigned long long>>(S_OK, retActualPosition);

    //set stream tick counters if needed
    if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING && pPlaylist->cpMediaSource->cpVideoStream->Selected() && pPlaylist->cpMediaSource->cpAudioStream->Selected())
    {
        if (TargetSeg->HasMediaType(AUDIO) == false && std::get<1>(retval).find(VIDEO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[VIDEO]);
        if (TargetSeg->HasMediaType(VIDEO) == false && std::get<1>(retval).find(AUDIO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[AUDIO]);
    }


    return retval;
}

tuple<HRESULT, std::map<ContentType, unsigned long long>> Playlist::SetCurrentPositionLiveAlternateAudio(Playlist *pPlaylist, unsigned long long AbsolutePositionInTicks, unsigned int MainPlaylistSegmentSeq)
{
    std::unique_lock<std::recursive_mutex> lockV;
    std::unique_lock<std::recursive_mutex> lockA;


    if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING)
    {
        //cancel any pending bitrate notification - we do not want to muddle our seek logic
        pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);
        //lock to prevent any incoming bitrate changes
        std::lock(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
        lockV = std::unique_lock<std::recursive_mutex>(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
        lockA = std::unique_lock<std::recursive_mutex>(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);
    }



    tuple<HRESULT, std::map<ContentType, unsigned long long>> retval;
    shared_ptr<MediaSegment> TargetSeg = nullptr;

    auto TargetMainSeg = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetSegment(MainPlaylistSegmentSeq);
    TargetSeg = pPlaylist->GetSegment(pPlaylist->FindAltRenditionMatchingSegment(pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist.get(), MainPlaylistSegmentSeq));

    //set the segment index trackers - this may not set correctly if the playlist did not have any trackers to begin with - we correct that after the segment is downloaded
    for (auto itr = pPlaylist->CurrentSegmentTracker.begin(); itr != pPlaylist->CurrentSegmentTracker.end(); itr++)
        pPlaylist->SetCurrentSegmentTracker(itr->first, TargetSeg);

    if (TargetSeg->GetCurrentState() != MediaSegmentState::INMEMORYCACHE)
    {
        StartStreamingAsync(pPlaylist, TargetSeg->SequenceNumber, TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration, true, true).get();//wait() or get() ?
    }
    else
    {
        if (TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration)
        {
            auto nextseg = pPlaylist->GetNextSegment(TargetSeg->SequenceNumber, pPlaylist->cpMediaSource->curDirection);
            if (nextseg != nullptr)
                StartStreamingAsync(pPlaylist, nextseg->GetSequenceNumber(), TargetSeg->CumulativeDuration < TargetMainSeg->CumulativeDuration, true, true);//wait() or get() ?
        }
        TargetSeg->RewindInCacheSegment();
    }




    //finally set the current position on the target segment
    std::map<ContentType, unsigned long long> retActualPosition;

    retActualPosition = TargetSeg->SetCurrentPosition(AbsolutePositionInTicks, pPlaylist->cpMediaSource->GetCurrentDirection(), true);

    LOG("SetCurrentPosition() : Segment = " << TargetSeg->SequenceNumber);
#if defined _VSLOG
    for (auto itm : retActualPosition)
    {
        LOG("SetCurrentPosition() : " << (itm.first == AUDIO ? "AUDIO = " : "VIDEO = ") << itm.second);
    }
#endif


    retval = tuple<HRESULT, std::map<ContentType, unsigned long long>>(S_OK, retActualPosition);

    //set stream tick counters if needed
    if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING && pPlaylist->cpMediaSource->cpVideoStream->Selected() && pPlaylist->cpMediaSource->cpAudioStream->Selected())
    {
        if (TargetSeg->HasMediaType(AUDIO) == false && std::get<1>(retval).find(VIDEO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[VIDEO]);
        if (TargetSeg->HasMediaType(VIDEO) == false && std::get<1>(retval).find(AUDIO) != std::get<1>(retval).end())
            pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase = make_shared<Timestamp>(std::get<1>(retval)[AUDIO]);
    }




    return retval;
}


///<summary>Returns the segment that contains the specified timepoint</summary>
///<param name='timeinticks'>The timepoint in ticks measured from the start of the presentation</param>
///<param name='retrycount'>The number of times the method retries. Each retry reduces the timestamp by an eighth of a second. Value maxes out at 4 and is zero based i.e. 4 = 5 retries.</param>
///<returns>The matching segment</returns>
shared_ptr<MediaSegment> Playlist::GetSegmentAtTime(unsigned long long timeinticks, unsigned short retrycount)
{
    std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
    std::unique_lock<std::recursive_mutex> mergelock(LockMerge, std::defer_lock);
    if (IsLive)
    {
        listlock.lock();
        mergelock.lock();
    }

    std::shared_ptr<MediaSegment> ret = nullptr;
    Playlist::SEGMENTVECTOR::iterator match = Segments.end();

    if (IsLive)
    {
        auto firstinmem = std::find_if(Segments.begin(), Segments.end(), [this](shared_ptr<MediaSegment> spms)
        {
            return spms->GetCurrentState() == INMEMORYCACHE;
        });

        if (firstinmem != Segments.end())
        {
            auto startoffset = (*firstinmem)->StartPTSNormalized->ValueInTicks;// -(*firstinmem)->CumulativeDuration - (*firstinmem)->Duration;
            //find the first segment for which the given time point is less than the start PTS - the target segment is the one prior to it then
            match = std::find_if(Segments.begin(), Segments.end(), [timeinticks, startoffset, this](Playlist::SEGMENTVECTOR::value_type segdata)
            {
                return segdata->CumulativeDuration > (timeinticks - startoffset);
            });
        }
    }
    else
    {
        //find the first segment for which the given time point is less than the start PTS - the target segment is the one prior to it then
        match = std::find_if(Segments.begin(), Segments.end(), [timeinticks, this](Playlist::SEGMENTVECTOR::value_type segdata)
        {
            return segdata->IncludesTimePoint(timeinticks, MFRATE_FORWARD);//always search forward
        });
    }

    if (match != Segments.end())
    {
        ret = *(match);
    }
    else
    {
        //BAD! BAD! //ret = nullptr; //return null if the byte position is out of range
        //should not happen- but the "retry" code is to try and prevent disaster

        //let's try adding/subtracting a quarter second away from the timeinticks  
        if (retrycount == 0)
        {
            ret = GetSegmentAtTime(timeinticks - __min(1000000, timeinticks), 1);//careful - do not want to cause unsigned overflow - henc the __min;
        }
        else if (retrycount == 1) //take away another quarter of a sec
        {
            ret = GetSegmentAtTime(timeinticks - __min(1000000, timeinticks), 2);
        }
        else if (retrycount == 2) //take away another quarter of a sec
        {
            ret = GetSegmentAtTime(timeinticks - __min(1000000, timeinticks), 3);
        }
        else if (retrycount == 3) //add  another quarter of a sec to the original (which has been reduced by three quarters)
        {
            ret = GetSegmentAtTime(timeinticks + 3000000, 4);
        }
        else if (retrycount == 4) //take away another quarter of a sec
        {
            ret = GetSegmentAtTime(timeinticks + 1000000, 5);
        }
        else if (retrycount == 5) //take away another quarter a sec
        {
            ret = GetSegmentAtTime(timeinticks + 1000000, 6);
        }
    }
    return ret;
}

///<summary>Gets the length of the look ahead buffer starting at a specific segment</summary>
///<param name='CurSegIdx'>The segment to start calculating from</param>
///<returns>The look ahead buffer in ticks</returns>
unsigned long long Playlist::GetCurrentLABLength(unsigned int CurSegSeqNum, bool IncludeDownloading, unsigned long long StopIfBeyond)
{

    unsigned long long ret = 0;

    std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
    if (IsLive) listlock.lock();

    auto seg = GetSegment(CurSegSeqNum);



    while (seg != nullptr)
    {
        //LOG("GetCurrentLABLength:Evaluating Seg " << seg->GetSequenceNumber() << ", State = " << seg->GetCurrentState());
        //for segments that have data

        if ((!IncludeDownloading && seg->GetCurrentState() == MediaSegmentState::INMEMORYCACHE) ||
            (IncludeDownloading && (seg->GetCurrentState() == MediaSegmentState::INMEMORYCACHE || seg->GetCurrentState() == MediaSegmentState::DOWNLOADING)))
        {
            //add up segment duration
            ret += seg->GetSegmentLookahead(cpMediaSource->GetCurrentDirection());
            if (StopIfBeyond > 0 && ret >= StopIfBeyond)
                break;
        }
        //break at the first segment that is not in memory
        else
            break;

        seg = GetNextSegment(seg->SequenceNumber, cpMediaSource->GetCurrentDirection());

    }

    //LOG("Evaluated LAB = " << ret << " , Min LAB " << Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate));
    return ret;
}

void  Playlist::CheckAndSetUpStreamTickCountersOnBRSwitch(Playlist* pPlaylist,
    shared_ptr<MediaSegment> targetseg,
    shared_ptr<MediaSegment> srcseg)
{
    auto prevsrcseg = srcseg->SequenceNumber >= targetseg->SequenceNumber ?
        pPlaylist->GetNextSegment(targetseg->SequenceNumber, MFRATE_DIRECTION::MFRATE_REVERSE) : srcseg;
    //if we switched from A/V to A Only or V Only we need to supply stream ticks
    TrackType srcTrackType = srcseg->HasMediaType(AUDIO) && srcseg->HasMediaType(VIDEO) ?
        TrackType::BOTH : (srcseg->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);
    TrackType targetTrackType = targetseg->HasMediaType(AUDIO) && targetseg->HasMediaType(VIDEO) ?
        TrackType::BOTH : (targetseg->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);

    if (srcTrackType == TrackType::BOTH && targetTrackType == TrackType::AUDIO)
    {
        auto vidpid = srcseg->GetPIDForMediaType(VIDEO);
        if (prevsrcseg != nullptr)
        {
            if (prevsrcseg->UnreadQueues[vidpid].empty() == false)
                pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase =
                make_shared<Timestamp>(prevsrcseg->Discontinous &&
                    prevsrcseg->UnreadQueues[vidpid].back()->DiscontinousTS != nullptr ?
                    prevsrcseg->UnreadQueues[vidpid].back()->DiscontinousTS->ValueInTicks :
                    prevsrcseg->UnreadQueues[vidpid].back()->SamplePTS->ValueInTicks);
            else
                pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase =
                make_shared<Timestamp>(prevsrcseg->Discontinous &&
                    prevsrcseg->ReadQueues[vidpid].back()->DiscontinousTS != nullptr ?
                    prevsrcseg->ReadQueues[vidpid].back()->DiscontinousTS->ValueInTicks :
                    prevsrcseg->ReadQueues[vidpid].back()->SamplePTS->ValueInTicks);
        }
    }
    else if (srcTrackType == TrackType::BOTH && targetTrackType == TrackType::VIDEO)
    {
        auto audpid = srcseg->GetPIDForMediaType(AUDIO);
        if (prevsrcseg != nullptr)
        {
            if (prevsrcseg->UnreadQueues[audpid].empty() == false)
                pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase =
                make_shared<Timestamp>(prevsrcseg->Discontinous &&
                    prevsrcseg->UnreadQueues[audpid].back()->DiscontinousTS != nullptr ?
                    prevsrcseg->UnreadQueues[audpid].back()->DiscontinousTS->ValueInTicks :
                    prevsrcseg->UnreadQueues[audpid].back()->SamplePTS->ValueInTicks);
            else
                pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase =
                make_shared<Timestamp>(prevsrcseg->Discontinous &&
                    prevsrcseg->ReadQueues[audpid].back()->DiscontinousTS != nullptr ?
                    prevsrcseg->ReadQueues[audpid].back()->DiscontinousTS->ValueInTicks :
                    prevsrcseg->ReadQueues[audpid].back()->SamplePTS->ValueInTicks);
        }
    }
    else if (srcTrackType != TrackType::BOTH && targetTrackType == TrackType::BOTH)
    {
        pPlaylist->cpMediaSource->cpVideoStream->StreamTickBase = nullptr;
        pPlaylist->cpMediaSource->cpAudioStream->StreamTickBase = nullptr;
    }

}



bool Playlist::CheckAndSwitchBitrate(Playlist *&pPlaylist, ContentType type, unsigned short srcPID)
{
    Playlist * target = nullptr;
    unsigned short targetPID = 0;
    bool vbrswitch = false;
    bool abrswitch = false;

    if (cpMediaSource->GetCurrentState() != MediaSourceState::MSS_STARTED || cpMediaSource->spRootPlaylist == nullptr)
        return vbrswitch;

    if (cpMediaSource->spRootPlaylist->IsVariant == false)
        return vbrswitch;

    std::shared_ptr<PlaylistSwitchRequest> videoswitch = nullptr;
    std::shared_ptr<PlaylistSwitchRequest> audioswitch = nullptr;

    unique_lock<recursive_mutex> lockVideo(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, defer_lock);
    unique_lock<recursive_mutex> lockAudio(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch, defer_lock);
    unique_lock<recursive_mutex> lockSrcMerge(pPlaylist->LockMerge, defer_lock);
    unique_lock<recursive_mutex> lockClient(pPlaylist->pParentStream == nullptr ?
        pPlaylist->cpMediaSource->spRootPlaylist->LockClient : pPlaylist->pParentStream->pParentPlaylist->LockClient, defer_lock);
    unique_lock<recursive_mutex> lockTargetMerge;


    lockVideo.lock();
    lockAudio.lock();
    lockClient.lock();

    videoswitch = pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
    audioswitch = pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch();

    //this should only happen if this gets called on a alternate rendition stream
    if ((type == AUDIO && audioswitch == nullptr) || (type == VIDEO && videoswitch == nullptr))
        return false;

    if (type == VIDEO && videoswitch != nullptr && videoswitch->targetPlaylist != nullptr)
    {
        lockSrcMerge.lock();
        lockTargetMerge = unique_lock<recursive_mutex>(videoswitch->targetPlaylist->LockMerge);
    }
    else if (type == AUDIO && audioswitch != nullptr && audioswitch->targetPlaylist != nullptr)
    {
        lockSrcMerge.lock();
        lockTargetMerge = unique_lock<recursive_mutex>(audioswitch->targetPlaylist->LockMerge);
    }



    if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0 && (
        (videoswitch != nullptr && videoswitch->SegmentTryCount > Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch) ||
        (audioswitch != nullptr && audioswitch->SegmentTryCount > Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch)
        ))
    {
        LOG("CheckAndSwitchBitrate: Cancelling BR Switch - Segment Try count reached");
        pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);
        return false;
    }

    auto currentAchievable = pPlaylist->cpMediaSource->spHeuristicsManager->FindBitrateToSwitchTo(pPlaylist->cpMediaSource->spHeuristicsManager->GetLastMeasuredBandwidth());
    if (type == AUDIO && audioswitch != nullptr
        && audioswitch->targetPlaylist->pParentStream->Bandwidth > pPlaylist->pParentStream->Bandwidth
        && currentAchievable < audioswitch->targetPlaylist->pParentStream->Bandwidth)
    {
        LOG("CheckAndSwitchBitrate: Cancelling BR Upshift - Target bitrate no longer achievable");
        if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
        {
            if (audioswitch != nullptr)
                audioswitch->SegmentTryCount++;
        }
        pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);

        return false;
    }

    if (type == VIDEO && videoswitch != nullptr
        && videoswitch->targetPlaylist->pParentStream->Bandwidth > pPlaylist->pParentStream->Bandwidth
        && currentAchievable < videoswitch->targetPlaylist->pParentStream->Bandwidth)
    {
        LOG("CheckAndSwitchBitrate: Cancelling BR Upshift - Target bitrate no longer achievable");
        if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
        {
            if (videoswitch != nullptr)
                videoswitch->SegmentTryCount++;
        }
        pPlaylist->cpMediaSource->TryCancelPendingBitrateSwitch(true);

        return false;
    }

    shared_ptr<MediaSegment> vidsrcseg = nullptr;
    shared_ptr<MediaSegment> audsrcseg = nullptr;

    if (type == VIDEO)
    {
        if (videoswitch == nullptr || pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate != 1.0)
            return vbrswitch;

        shared_ptr<MediaSegment> targetseg = nullptr;


        //get the target playlist
        target = videoswitch->targetPlaylist;

        targetseg = target->HasCurrentSegmentTracker(VIDEO) ?
            target->GetCurrentSegmentTracker(VIDEO) :
            target->GetBitrateSwitchTarget(this, videoswitch->IgnoreBuffer);

        if (targetseg == nullptr)
        {
            LOG("CheckAndSwitchBitrate: Skipping VIDEO BR Switch attempt - No target segment");
            if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
            {
                if (videoswitch != nullptr)
                    videoswitch->SegmentTryCount++;
            }
            return vbrswitch;
        }

        vidsrcseg = pPlaylist->GetCurrentSegmentTracker(VIDEO);
        auto nextvidsrcseg = pPlaylist->GetNextSegment(vidsrcseg->GetSequenceNumber(), MFRATE_DIRECTION::MFRATE_FORWARD);
        if (pPlaylist->pParentStream->Bandwidth > target->pParentStream->Bandwidth &&
            targetseg->GetSequenceNumber() > nextvidsrcseg->GetSequenceNumber() && nextvidsrcseg->GetCurrentState() == INMEMORYCACHE)
        {
            //shifting down and we still have higher bitrate buffer to playout
            LOG("CheckAndSwitchBitrate: Buffer left - postponing down shift - current seg = " << vidsrcseg->SequenceNumber << ", targetseg = " << targetseg->SequenceNumber);
            return vbrswitch;
        }



        LOG("CheckAndSwitchBitrate:  Trying shift(From " << pPlaylist->pParentStream->Bandwidth << "(Seq " << vidsrcseg->GetSequenceNumber() << ") to " << target->pParentStream->Bandwidth << "(Seq " << targetseg->GetSequenceNumber() << ")");

        try
        {
            auto hr = CheckAndBufferForBRSwitch(vidsrcseg->SequenceNumber);
            if (FAILED(hr))
                throw hr;
        }
        catch (...)
        {
            LOG("CheckAndSwitchBitrate: Skipping VIDEO BR Switch attempt - Error loading target segment");
            if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
            {
                if (videoswitch != nullptr)
                    videoswitch->SegmentTryCount++;
            }
            return vbrswitch;
        }

        //find nearest keyframe 

        if (targetseg->GetCurrentState() == INMEMORYCACHE)
        {
            shared_ptr<SampleData> match = nullptr;

            if (targetseg->HasMediaType(VIDEO))
            {

                auto targetvidpid = targetseg->GetPIDForMediaType(VIDEO);

                auto oldvidframedist = pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance;

                if (vidsrcseg != nullptr && vidsrcseg->Discontinous && targetseg->Discontinous == false)
                {
                    videoswitch->targetPlaylist->MakeDiscontinous();
                }
                if (targetseg->Discontinous && vidsrcseg != nullptr && vidsrcseg->GetCurrentState() == INMEMORYCACHE)
                {
                    targetseg->UpdateSampleDiscontinuityTimestamps(vidsrcseg, true);
                }

                pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance =
                    targetseg->GetApproximateFrameDistance(VIDEO, targetvidpid);

                auto nextsample = vidsrcseg->HasMediaType(VIDEO) && vidsrcseg->ReadQueues[vidsrcseg->GetPIDForMediaType(VIDEO)].size() > 0
                    ? vidsrcseg->ReadQueues[vidsrcseg->GetPIDForMediaType(VIDEO)].back()
                    : (vidsrcseg->HasMediaType(AUDIO) && vidsrcseg->ReadQueues[vidsrcseg->GetPIDForMediaType(AUDIO)].size() > 0 ?
                        vidsrcseg->ReadQueues[vidsrcseg->GetPIDForMediaType(AUDIO)].back() :
                        targetseg->PeekNextSample(targetvidpid, pPlaylist->cpMediaSource->GetCurrentDirection()));

                if (nextsample != nullptr)
                {
                    match = targetseg->FindNearestIDRSample(nextsample->SamplePTS->ValueInTicks, targetvidpid, MFRATE_FORWARD, false, 1);//find closest video key frame
                    //try immediate next segment in the target
                    if (match == nullptr)
                    {
                        auto nextseg = target->GetNextSegment(targetseg->GetSequenceNumber(), MFRATE_FORWARD);
                        if (nextseg != nullptr)
                        {
                            target->SetCurrentSegmentTracker(VIDEO, nextseg);
                            targetseg = nextseg;
                            if (targetseg->GetCurrentState() != INMEMORYCACHE)
                            {
                                pPlaylist->cpMediaSource->StartBuffering();
                                try
                                {
                                    Playlist::StartStreamingAsync(target, targetseg->GetSequenceNumber(), false, true, true).get();
                                }
                                catch (...)
                                {

                                }
                                pPlaylist->cpMediaSource->EndBuffering();
                            }
                            if (targetseg->GetCurrentState() == INMEMORYCACHE)
                                match = targetseg->FindNearestIDRSample(nextsample->SamplePTS->ValueInTicks, targetvidpid, MFRATE_FORWARD, false, 1);//find closest video key frame
                        }
                    }

                    if (match == nullptr) //no keyframe
                        return vbrswitch;

                    auto nextsrcseg = pPlaylist->GetNextSegment(vidsrcseg->GetSequenceNumber(), MFRATE_FORWARD);

                    //there's a "hole"
                    if (nextsrcseg != nullptr && (match->SamplePTS->ValueInTicks -
                        nextsample->SamplePTS->ValueInTicks) > 2 * oldvidframedist &&
                        targetseg->Discontinous == false &&
                        vidsrcseg->Discontinous == false &&
                        nextsrcseg->Discontinous == false)
                    {
                        LOG("Bitrate Switch: Filling up gap between" << nextsample->SamplePTS->ValueInTicks << " and " << match->SamplePTS->ValueInTicks);
                        if (nextsrcseg->GetCurrentState() != INMEMORYCACHE)
                        {
                            pPlaylist->cpMediaSource->StartBuffering();
                            try
                            {
                                Playlist::StartStreamingAsync(pPlaylist, nextsrcseg->GetSequenceNumber(), false, true).get();
                            }
                            catch (...)
                            {
                            }
                            pPlaylist->cpMediaSource->EndBuffering();
                        }

                        if (nextsrcseg->GetCurrentState() == INMEMORYCACHE && nextsrcseg->HasMediaType(VIDEO))
                        {
                            //transfer video samples from source bitrate to fill the "hole"
                            while (true)
                            {
                                if (targetseg->UnreadQueues[targetvidpid].front()->SamplePTS->ValueInTicks != match->SamplePTS->ValueInTicks)
                                    targetseg->UnreadQueues[targetvidpid].pop_front();
                                else
                                    break;
                            }

                            auto srcvidpid = nextsrcseg->GetPIDForMediaType(VIDEO);

                            auto srcvidmatchitr = std::find_if(nextsrcseg->UnreadQueues[srcvidpid].begin(),
                                nextsrcseg->UnreadQueues[srcvidpid].end(), [this, match](shared_ptr<SampleData> sd)
                            {
                                return sd->SamplePTS->ValueInTicks >= match->SamplePTS->ValueInTicks;
                            });

                            if (srcvidmatchitr != nextsrcseg->UnreadQueues[srcvidpid].begin()) {
                                for (auto itr = --srcvidmatchitr;; itr--)
                                {
                                    targetseg->UnreadQueues[targetvidpid].push_front(*itr);
                                    if (nextsrcseg->UnreadQueues[srcvidpid].front()->SamplePTS->ValueInTicks == (*itr)->SamplePTS->ValueInTicks)
                                        break;
                                }
                            }
                            //transfer audio samples from source bitrate to fill the "hole"
                            if (targetseg->HasMediaType(AUDIO) && nextsrcseg->HasMediaType(AUDIO))
                            {
                                auto targetaudpid = targetseg->GetPIDForMediaType(AUDIO);
                                while (true)
                                {
                                    if (targetseg->UnreadQueues[targetaudpid].front()->SamplePTS->ValueInTicks < match->SamplePTS->ValueInTicks)
                                        targetseg->UnreadQueues[targetaudpid].pop_front();
                                    else
                                        break;
                                }

                                auto srcaudpid = nextsrcseg->GetPIDForMediaType(AUDIO);

                                auto srcaudmatchitr = std::find_if(nextsrcseg->UnreadQueues[srcaudpid].begin(),
                                    nextsrcseg->UnreadQueues[srcaudpid].end(), [this, match](shared_ptr<SampleData> sd)
                                {
                                    return sd->SamplePTS->ValueInTicks >= match->SamplePTS->ValueInTicks;
                                });
                                if (srcaudmatchitr != nextsrcseg->UnreadQueues[srcaudpid].begin()) {
                                    for (auto itr = --srcaudmatchitr;; itr--)
                                    {
                                        targetseg->UnreadQueues[targetaudpid].push_front(*itr);
                                        if (nextsrcseg->UnreadQueues[srcaudpid].front()->SamplePTS->ValueInTicks == (*itr)->SamplePTS->ValueInTicks)
                                            break;
                                    }
                                }
                            }
                        }

                    }
                    else
                    {
                        targetseg->SetCurrentPosition(targetvidpid, match->SamplePTS->ValueInTicks, pPlaylist->cpMediaSource->GetCurrentDirection(), true);
                        LOG("CheckAndSwitchBitrate: Video Bitrate Switch - First Sample PTS = " << nextsample->SamplePTS->ValueInTicks << ", First IDR PTS = " << match->SamplePTS->ValueInTicks);
                    }
                }
                if (!target->HasCurrentSegmentTracker(VIDEO) ||
                    target->GetCurrentSegmentTracker(VIDEO)->SequenceNumber != targetseg->SequenceNumber)
                    target->SetCurrentSegmentTracker(VIDEO, targetseg);
            }

            if (audioswitch != nullptr)
                audioswitch->VideoSwitchedTo = targetseg;

            //simply switch bitrate - it falls on first sample of new segment
            pPlaylist->cpMediaSource->cpVideoStream->SwitchBitrate();

            CheckAndSetUpStreamTickCountersOnBRSwitch(pPlaylist, targetseg, vidsrcseg);

            if (pPlaylist->IsLive == false && vidsrcseg->GetCloaking() == nullptr)
                vidsrcseg->Scavenge();

            vbrswitch = true;
        }

        if (vbrswitch &&
            pPlaylist->cpMediaSource != nullptr &&
            pPlaylist->cpMediaSource->cpController != nullptr &&
            pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr &&
            pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch() == nullptr) //switch happened
        {

            LOG("CheckAndSwitchBitrate:  VIDEO Switched(From " << pPlaylist->pParentStream->Bandwidth << "(Seq " << pPlaylist->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber() << ") to " << target->pParentStream->Bandwidth << "(Seq " << target->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber() << ")");
            auto curVidSegSeNum = pPlaylist->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber();
            auto targetVidSegSeqNum = target->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber();
            pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([pPlaylist, target, curVidSegSeNum, targetVidSegSeqNum]()
            {
                if (pPlaylist->cpMediaSource != nullptr &&
                    pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
                    pPlaylist->cpMediaSource->cpController != nullptr && pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSegmentSwitched(pPlaylist, target, curVidSegSeNum, targetVidSegSeqNum);

                return S_OK;
            }, task_options(task_continuation_context::use_arbitrary())));
        }

        if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
        {
            if (videoswitch != nullptr)
                videoswitch->SegmentTryCount++;
        }
    }


    if (type == AUDIO || vbrswitch)//either we are switching from audio only or we have just switched video
    {
        //get the target playlist

        if (audioswitch != nullptr &&  pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate == 1.0 &&
            pPlaylist->pParentRendition == nullptr)
        {
            shared_ptr<MediaSegment> targetseg = nullptr;

            audsrcseg = pPlaylist->GetCurrentSegmentTracker(AUDIO);
            if (audsrcseg->HasMediaType(VIDEO) && audioswitch->VideoSwitchedTo == nullptr &&
                !(Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO))//source has video - only switch after video has switched
            {
                LOGIF(vbrswitch, "CheckAndSwitchBitrate: Video has not switched yet : audsrcseg->HasMediaType(VIDEO) && audioswitch->VideoSwitchedTo == nullptr");
                return type == VIDEO ? vbrswitch : false;
            }

            target = audioswitch->targetPlaylist;

            targetseg = audioswitch->VideoSwitchedTo != nullptr ?
                audioswitch->VideoSwitchedTo : (target->HasCurrentSegmentTracker(AUDIO) ?
                    target->GetCurrentSegmentTracker(AUDIO) :
                    target->GetBitrateSwitchTarget(this, audioswitch->IgnoreBuffer));


            if (targetseg == nullptr)
            {
                LOGIF(vbrswitch, "CheckAndSwitchBitrate: Video switched but audio did not : targetseg == nullptr");
                return type == VIDEO ? vbrswitch : false;
            }

            if (!vbrswitch)
            {
                try
                {
                    auto hr = CheckAndBufferForBRSwitch(audsrcseg->SequenceNumber);
                    if (FAILED(hr))
                        throw hr;
                }
                catch (...)
                {
                    LOG("CheckAndSwitchBitrate: Skipping AUDIO BR Switch attempt - Error loading target segment");
                    return type == VIDEO ? vbrswitch : false;
                }

            }
            if (targetseg->GetCurrentState() == INMEMORYCACHE)
            {
                if (targetseg->HasMediaType(AUDIO))
                {
                    bool samplestransferred = false;

                    //transfer unread audio samples to the target segment
                    if (audsrcseg->HasMediaType(AUDIO) && !(IsLive && (targetseg->HasMediaType(VIDEO) == false /* going from A/V to A Only*/ || audsrcseg->HasMediaType(VIDEO) == false /* going from A Only to A/V*/)))
                        //  && audsrcseg->Discontinous == false && targetseg->Discontinous == false)
                    {
                        auto srcaudPID = audsrcseg->GetPIDForMediaType(AUDIO);
                        auto targetaudPID = targetseg->GetPIDForMediaType(AUDIO);
                        if (audsrcseg->SequenceNumber < targetseg->SequenceNumber && audsrcseg->IsReadEOS(srcaudPID) == false)
                        {
                            auto firstaudiosample = targetseg->PeekNextSample(targetaudPID, MFRATE_DIRECTION::MFRATE_FORWARD);

                            for (auto itr = audsrcseg->UnreadQueues[srcaudPID].rbegin(); itr != audsrcseg->UnreadQueues[srcaudPID].rend(); itr++)
                            {
                                if (audsrcseg->Discontinous == false &&
                                    targetseg->Discontinous == false &&
                                    (*itr)->SamplePTS->ValueInTicks >= firstaudiosample->SamplePTS->ValueInTicks)
                                    continue;

                                targetseg->UnreadQueues[targetaudPID].push_front(*itr);
                            }
                            samplestransferred = true;
                            LOG("Bitrate Switch: Transferred unread audio samples");
                        }
                        else if (audsrcseg->SequenceNumber == targetseg->SequenceNumber && audsrcseg->ReadQueues.find(srcaudPID) != audsrcseg->ReadQueues.end() && audsrcseg->ReadQueues[srcaudPID].size() > 0)
                        {
                            auto firstaudiosample = targetseg->PeekNextSample(targetaudPID, MFRATE_DIRECTION::MFRATE_FORWARD);
                            auto lastaudiosrcsample = audsrcseg->ReadQueues[srcaudPID].back();
                            if (audsrcseg->Discontinous == false &&
                                targetseg->Discontinous == false && firstaudiosample->SamplePTS->ValueInTicks <= lastaudiosrcsample->SamplePTS->ValueInTicks)
                            {
                                while (targetseg->UnreadQueues[targetaudPID].front()->SamplePTS->ValueInTicks <= lastaudiosrcsample->SamplePTS->ValueInTicks)
                                {
                                    targetseg->ReadQueues[targetaudPID].push_back(targetseg->UnreadQueues[targetaudPID].front());
                                    targetseg->UnreadQueues[targetaudPID].pop_front();
                                }
                            }
                            samplestransferred = true;
                            LOG("Bitrate Switch: Removed duplicate audio samples");
                        }
                    }

                    if (audsrcseg != nullptr && audsrcseg->Discontinous && targetseg->Discontinous == false)
                    {
                        audioswitch->targetPlaylist->MakeDiscontinous();
                    }


                    if (targetseg->Discontinous && audsrcseg != nullptr && audsrcseg->GetCurrentState() == INMEMORYCACHE
                        && ((!audsrcseg->HasMediaType(VIDEO) && targetseg->HasMediaType(VIDEO)) ||
                            !targetseg->HasMediaType(VIDEO) ||
                            samplestransferred))
                    {
                        targetseg->UpdateSampleDiscontinuityTimestamps(audsrcseg, true);
                    }

                    pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance =
                        targetseg->GetApproximateFrameDistance(AUDIO, targetseg->GetPIDForMediaType(AUDIO));
                }

                if (!audsrcseg->HasMediaType(VIDEO) && targetseg->HasMediaType(VIDEO))
                {

                    pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance =
                        targetseg->GetApproximateFrameDistance(VIDEO, targetseg->GetPIDForMediaType(VIDEO));

                    if (IsLive)
                    {
                        targetseg->pParentPlaylist->MakeDiscontinous();

                        if (targetseg->Discontinous && audsrcseg != nullptr && audsrcseg->GetCurrentState() == INMEMORYCACHE
                            && ((!audsrcseg->HasMediaType(VIDEO) && targetseg->HasMediaType(VIDEO)) ||
                                !targetseg->HasMediaType(VIDEO)))
                        {
                            targetseg->UpdateSampleDiscontinuityTimestamps(audsrcseg, true);
                        }
                    }
                    auto nextsample = targetseg->PeekNextSample(targetseg->GetPIDForMediaType(VIDEO), pPlaylist->cpMediaSource->GetCurrentDirection());
                    if (nextsample != nullptr /* && Configuration::GetCurrent()->ForceKeyFrameMatchOnBitrateSwitch*/)
                    {
                        auto match = targetseg->FindNearestIDRSample(nextsample->SamplePTS->ValueInTicks, targetseg->GetPIDForMediaType(VIDEO));//find closest video key frame
                        if (match != nullptr)
                        {

#if WINVER < 0x0A00 && WINAPI_FAMILY==WINAPI_FAMILY_PC_APP
                            pPlaylist->cpMediaSource->cpVideoStream->NotifyFormatChanged(nullptr); //draining the decoder on the phone  on Windows 8.1 causes a D3D access violation
#elif WINVER >= 0x0A00 && WINAPI_FAMILY!=WINAPI_FAMILY_PHONE_APP
                            pPlaylist->cpMediaSource->cpVideoStream->NotifyFormatChanged(nullptr); //draining the decoder on the phone  on Windows 8.1 causes a D3D access violation
#endif 
                            targetseg->SetCurrentPosition(targetseg->GetPIDForMediaType(VIDEO), match->SamplePTS->ValueInTicks, pPlaylist->cpMediaSource->GetCurrentDirection(), true);
                            LOG("CheckAndSwitchBitrate: Video Bitrate Switch : First Sample PTS = " << nextsample->SamplePTS->ValueInTicks << ", First IDR PTS = " << match->SamplePTS->ValueInTicks);
                        }
                    }
                    if (!target->HasCurrentSegmentTracker(VIDEO) ||
                        target->GetCurrentSegmentTracker(VIDEO)->SequenceNumber != targetseg->SequenceNumber)
                        target->SetCurrentSegmentTracker(VIDEO, targetseg);


                    if (audioswitch != nullptr)
                        audioswitch->VideoSwitchedTo = targetseg;

                    //simply switch bitrate - it falls on first sample of new segment
                    pPlaylist->cpMediaSource->cpVideoStream->SwitchBitrate();
                    vbrswitch = true;
                }
                else if (((!audsrcseg->HasMediaType(VIDEO) && !targetseg->HasMediaType(VIDEO)) || Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO) && pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch() != nullptr)
                {
                    if (!target->HasCurrentSegmentTracker(VIDEO) ||
                        target->GetCurrentSegmentTracker(VIDEO)->SequenceNumber != targetseg->SequenceNumber)
                        target->SetCurrentSegmentTracker(VIDEO, targetseg);


                    if (audioswitch != nullptr)
                        audioswitch->VideoSwitchedTo = targetseg;

                    //simply switch bitrate - it falls on first sample of new segment
                    pPlaylist->cpMediaSource->cpVideoStream->SwitchBitrate();
                    vbrswitch = true;
                }



                if (!target->HasCurrentSegmentTracker(AUDIO) ||
                    target->GetCurrentSegmentTracker(AUDIO)->SequenceNumber != targetseg->SequenceNumber)
                    target->SetCurrentSegmentTracker(AUDIO, targetseg);

                //simply switch bitrate - it falls on first sample of new segment
                pPlaylist->cpMediaSource->cpAudioStream->SwitchBitrate();

                abrswitch = true;

            }
            else if (vbrswitch && pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch() != nullptr &&
                ((!targetseg->HasMediaType(AUDIO) && !audsrcseg->HasMediaType(AUDIO)) || Configuration::GetCurrent()->ContentTypeFilter == ContentType::VIDEO))
            {
                if (!target->HasCurrentSegmentTracker(AUDIO) ||
                    target->GetCurrentSegmentTracker(AUDIO)->SequenceNumber != targetseg->SequenceNumber)
                    target->SetCurrentSegmentTracker(AUDIO, targetseg);
                pPlaylist->cpMediaSource->cpAudioStream->SwitchBitrate();
                abrswitch = true;
            }
            else
            {
                LOGIF(vbrswitch, "CheckAndSwitchBitrate: Video switched but audio did not : targetseg not in memory");
                abrswitch = false;
            }

            if (abrswitch &&
                pPlaylist->cpMediaSource != nullptr &&
                pPlaylist->cpMediaSource->cpController != nullptr &&
                pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr &&
                pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch() == nullptr)//switch actually happened
            {

                LOG("CheckAndSwitchBitrate:  : AUDIO Switched(From " << pPlaylist->pParentStream->Bandwidth << "(Seq " << pPlaylist->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber() << ") to " << target->pParentStream->Bandwidth << "(Seq " << target->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber() << ")");

                auto curAudSegSeqNum = pPlaylist->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber();
                auto targetAudSegSeqNum = target->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber();
                pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([pPlaylist, target, curAudSegSeqNum, targetAudSegSeqNum]()
                {
                    if (pPlaylist->cpMediaSource != nullptr &&
                        pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED && pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR
                        && pPlaylist->cpMediaSource->cpController != nullptr &&
                        pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                        pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSegmentSwitched(pPlaylist, target, curAudSegSeqNum, targetAudSegSeqNum);

                    return S_OK;
                }, task_options(task_continuation_context::use_arbitrary())));
            }

            if (Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch > 0)
            {
                if (audioswitch != nullptr)
                    audioswitch->SegmentTryCount++;
            }
        }
    }

    if (vbrswitch || abrswitch)
    {
        //handle rendition switches that may be required because of the bitrate switch
        //if we are playing alternate audio and no rendition switch is currently scheduled
        if (pPlaylist->pParentStream->GetActiveAudioRendition() != nullptr && pPlaylist->cpMediaSource->cpAudioStream->GetPendingRenditionSwitch() == nullptr)
        {
            //find a matching rendition for the target bitrate
            auto matchingRen = Rendition::FindMatching(pPlaylist->pParentStream->GetActiveAudioRendition(), *(target->pParentStream->AudioRenditions.get()));
            if (matchingRen != nullptr)
            {
                if (matchingRen->IsActive == false || target->pParentStream->GetActiveAudioRendition() != matchingRen.get()) //founding matching rendition that is not currently active
                {
                    //make the currenr rendition inactive and the target one active
                    pPlaylist->pParentStream->GetActiveAudioRendition()->IsActive = false;
                    pPlaylist->pParentStream->SetActiveAudioRendition(nullptr);
                    target->pParentStream->SetActiveAudioRendition(matchingRen.get());
                    matchingRen->IsActive = true;

                }
            }
            else
            {
                //deactivate the alternate rendition
                pPlaylist->pParentStream->GetActiveAudioRendition()->IsActive = false;
                pPlaylist->PauseBufferBuilding = false;
                pPlaylist->pParentStream->SetActiveAudioRendition(nullptr);
                //schedule a switch to the main track
                cpMediaSource->cpAudioStream->ScheduleSwitch(target, PlaylistSwitchRequest::SwitchType::RENDITION);
            }
        }
    }
    if (pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch() == nullptr &&
        pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch() == nullptr &&
        (vbrswitch || abrswitch))//switch complete
    {

        if (vbrswitch && pPlaylist->IsLive == false && vidsrcseg != nullptr && vidsrcseg->GetCloaking() == nullptr)
            vidsrcseg->Scavenge();
        if (abrswitch && pPlaylist->IsLive == false && audsrcseg != nullptr && audsrcseg->GetCloaking() == nullptr)
            audsrcseg->Scavenge();

        //make current variant inactive
        pPlaylist->pParentStream->MakeActive(false);
        if (pPlaylist->spswPlaylistRefresh != nullptr)
        {
            pPlaylist->spswPlaylistRefresh->StopTicking();
            pPlaylist->spswPlaylistRefresh.reset();
        }
        pPlaylist->StopVideoStreamTick();
        pPlaylist->CancelDownloads();
        target->PauseBufferBuilding = false;
        //make target variant active
        target->pParentStream->MakeActive(true);
        if (pPlaylist->IsLive)
        {

            target->MergeLivePlaylistChild();
            target->StartPTSOriginal = pPlaylist->StartPTSOriginal;

            //transfer the sliding window start for event playlists
            if (pPlaylist->PlaylistType == Microsoft::HLSClient::HLSPlaylistType::EVENT && pPlaylist->SlidingWindowStart != nullptr)
            {
                target->SlidingWindowStart = pPlaylist->SlidingWindowStart;
                target->SlidingWindowEnd = std::make_shared<Timestamp>(target->SlidingWindowStart->ValueInTicks + target->Segments.back()->CumulativeDuration);
            }

            if (target->spswPlaylistRefresh == nullptr)
            {
                target->SetStopwatchForNextPlaylistRefresh((unsigned long long)(target->DerivedTargetDuration / 2));
            }
        }

        //start streaming the target chained
        target->CheckAndBufferIfNeeded(target->MaxCurrentSegment()->GetSequenceNumber(), false);

        LOG("CheckAndSwitchBitrate: Switch completed (From " << pPlaylist->pParentStream->Bandwidth << " to " << target->pParentStream->Bandwidth << ",Current Start PTS : " << (pPlaylist->StartPTSOriginal != nullptr ? pPlaylist->StartPTSOriginal->ValueInTicks : 0) << " , Target Start PTS : " << (target->StartPTSOriginal != nullptr ? target->StartPTSOriginal->ValueInTicks : 0) << ")");

        auto oldbandwidth = pPlaylist->pParentStream->Bandwidth;
        pPlaylist->SetCurrentSegmentTracker(VIDEO, nullptr);
        pPlaylist->SetCurrentSegmentTracker(AUDIO, nullptr);
        pPlaylist = target;

        if (Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift != 0.0 && cpMediaSource->spHeuristicsManager != nullptr &&
            cpMediaSource->spHeuristicsManager->GetIgnoreDownshiftTolerance())
            cpMediaSource->spHeuristicsManager->SetIgnoreDownshiftTolerance(false);

        if (pPlaylist->cpMediaSource->spHeuristicsManager != nullptr)
            pPlaylist->cpMediaSource->spHeuristicsManager->SetLastSuggestedBandwidth(pPlaylist->pParentStream->Bandwidth);//just to be safe - we have seen sometimes that with overlapping bitrate switch requests this does not get set properly
        if (vbrswitch && pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch() == nullptr && cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
            cpMediaSource->cpVideoStream->RaiseBitrateSwitched(oldbandwidth, pPlaylist->pParentStream->Bandwidth);
        if (abrswitch && pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch() == nullptr && cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
            cpMediaSource->cpAudioStream->RaiseBitrateSwitched(oldbandwidth, pPlaylist->pParentStream->Bandwidth);

    }

    return type == VIDEO ? vbrswitch : abrswitch;
}


bool Playlist::CheckAndSwitchAudioRendition(Playlist *&pPlaylist, bool FollowingBitrateSwitch)
{
    Playlist *target = nullptr;
    std::lock_guard<std::recursive_mutex> lockAudio(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
    auto audioswitch = pPlaylist->cpMediaSource->cpAudioStream->GetPendingRenditionSwitch();

    if (audioswitch == nullptr || pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate != 1.0)
        return false;

    LOG("Attempting Rendition Switch...");
    target = audioswitch->targetPlaylist;

    //are we switching to the main track ?  
    if (target->pParentRendition == nullptr &&  pPlaylist->HasCurrentSegmentTracker(AUDIO))
    {
        std::lock_guard<std::recursive_mutex> targetLock(target->LockSegmentTracking);
        if (!target->HasCurrentSegmentTracker(AUDIO) || target->GetCurrentSegmentTracker(AUDIO)->SequenceNumber != pPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber)  //switching to main track
          //update the main track segment tracker
            target->SetCurrentSegmentTracker(AUDIO, target->GetSegment(pPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber));//change on the next one
    }

    if (audioswitch != nullptr &&
        pPlaylist->HasCurrentSegmentTracker(AUDIO) &&
        target->GetCurrentSegmentTracker(AUDIO)->SequenceNumber == pPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber &&
        target->GetCurrentSegmentTracker(AUDIO)->GetCurrentState() == INMEMORYCACHE)
    {

        if (pPlaylist->pParentStream != nullptr)
        {
            LOG("Rendition Switch From Main Audio Track");
        }
        else if (target->pParentStream != nullptr)
        {
            LOG("Rendition Switch To Main Audio Track");
        }
        pPlaylist->cpMediaSource->cpAudioStream->SwitchRendition();
        pPlaylist = target;
        //in case a bitrate switch happened in between the request and the execution of the rendition switch
        pPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->SetActiveAudioRendition(target->pParentRendition);
        return true;
    }
    return false;
}

void Playlist::RaiseStreamSelectionChanged(TrackType from, TrackType to)
{
    //raise the bitrate switch completion event on a background thread - do not block stream
    if (cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
    {
        cpMediaSource->protectionRegistry.Register(task<HRESULT>([from, to, this]()
        {
            if (cpMediaSource->GetCurrentState() != MSS_ERROR && cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED &&
                cpMediaSource->cpController != nullptr && cpMediaSource->cpController->GetPlaylist() != nullptr)
                cpMediaSource->cpController->GetPlaylist()->RaiseStreamSelectionChanged(from, to);
            return S_OK;
        }));
    }
}


bool Playlist::IsEOS()
{
    bool ret = true;
    std::lock_guard<std::recursive_mutex> lock(LockSegmentTracking);
    //check EOS for each stream
    for (auto itr : CurrentSegmentTracker)
    {
        ret = IsEOS(itr.first);
        if (ret == false)
            break;
    }

    return ret;

}

bool Playlist::IsEOS(ContentType mediaType)
{
    bool ret = true;
    std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
    if (IsLive) listlock.lock();

    auto curseg = GetCurrentSegmentTracker(mediaType);
    auto lastseg = (cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD ? Segments.back() : Segments.front());
    if (curseg->HasMediaType(mediaType) == false)//we do not have this media type;
        ret = true;
    //if we are not at the last segment OR if we are at the last segment, but that segment still has data - we return false,or if the playlist is still marked live (it won't be for the last refresh of a live stream since the last refresh should contain an EXT-X-ENDLIST)
    else if (/*this->IsLive ||*/
        (curseg->SequenceNumber != lastseg->SequenceNumber) ||
        (curseg->SequenceNumber == lastseg->SequenceNumber && curseg->IsReadEOS(curseg->GetPIDForMediaType(mediaType)) == false))
    {
        ret = false;
    }

    return ret;
}

///<summary>Invoked by the MF streams to request a sample during playback</summary>
///<param name='pPlaylist'>The target playlist</param>
///<param name='type'>The content type for the sample (audio or video)</param>
///<param name='ppSample'>The returned sample</param>
///<returns>HRESULT indicating success or failure</returns>
HRESULT Playlist::RequestSample(Playlist *pPlaylist, ContentType type, IMFSample** ppSample)
{
    if (pPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED || pPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR)
        return MF_E_MEDIA_SOURCE_WRONGSTATE;
    else
    {
        //LOGIIF(type == VIDEO,"Requesting Video Sample","Requesting Audio Sample");
        return type == VIDEO ? RequestVideoSample(pPlaylist, ppSample) : RequestAudioSample(pPlaylist, ppSample);
    }
}

void Playlist::AdjustSegmentForVideoThinning(Playlist *pPlaylist, shared_ptr<MediaSegment> curSegment, unsigned short PID, bool& SegmentPIDEOS)
{
    if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned && curSegment->GetCurrentState() == INMEMORYCACHE)
    {
        shared_ptr<Timestamp> ts = nullptr;
        auto nextsd = curSegment->PeekNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection());
        if (nextsd != nullptr && nextsd->IsSampleIDR == false)
            ts = curSegment->PositionQueueAtNextIDR(PID, pPlaylist->cpMediaSource->GetCurrentDirection(), pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->IDRSkipCount);
        //get the EOS flag again for this PID
        SegmentPIDEOS = curSegment->IsReadEOS(PID);

        if (ts == nullptr)
        {
            if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition() == nullptr) //main audio
            {
                if (pPlaylist->CurrentSegmentTracker.find(AUDIO) != pPlaylist->CurrentSegmentTracker.end() && pPlaylist->CurrentSegmentTracker[AUDIO] != nullptr)
                    pPlaylist->CurrentSegmentTracker[AUDIO]->AdvanceUnreadQueue(pPlaylist->CurrentSegmentTracker[AUDIO]->GetPIDForMediaType(AUDIO), nullptr);

            }
            else if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition() != nullptr) //alt audio
            {
                if (pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker.find(AUDIO) != pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker.end() && pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO] != nullptr)
                    pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO]->AdvanceUnreadQueue(pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO]->GetPIDForMediaType(AUDIO), nullptr);
            }
            else if (pPlaylist->pParentStream == nullptr && pPlaylist->pParentRendition != nullptr) //this is alt video
            {
                if (pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition() == nullptr)
                {
                    if (pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CurrentSegmentTracker.find(AUDIO) != pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CurrentSegmentTracker.end() && pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CurrentSegmentTracker[AUDIO] != nullptr)
                        pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CurrentSegmentTracker[AUDIO]->AdvanceUnreadQueue(pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CurrentSegmentTracker[AUDIO]->GetPIDForMediaType(AUDIO), nullptr);
                }
                else //also playing alt audio
                {
                    if (pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker.find(AUDIO) != pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker.end() && pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO] != nullptr)
                        pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO]->AdvanceUnreadQueue(pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->GetActiveAudioRendition()->spPlaylist->CurrentSegmentTracker[AUDIO]->GetPIDForMediaType(AUDIO), nullptr);
                }
            }
        }
    }
}

void Playlist::ApplyStreamTick(ContentType type, shared_ptr<MediaSegment> curSegment, shared_ptr<MediaSegment> oldSrcSeg, IMFSample* pSample)
{
    auto stream = (type == VIDEO ? dynamic_cast<CMFStreamCommonImpl*>(cpMediaSource->cpVideoStream.Get()) : dynamic_cast<CMFStreamCommonImpl*>(cpMediaSource->cpAudioStream.Get()));

    if (stream == nullptr)
        return;

    bool HasType = curSegment->HasMediaType(type);
    auto pid = HasType ? curSegment->GetPIDForMediaType(type) : 0;
    auto cumDur = (type == VIDEO ? cpMediaSource->spRootPlaylist->LiveVideoPlaybackCumulativeDuration :
        cpMediaSource->spRootPlaylist->LiveAudioPlaybackCumulativeDuration);

    if (stream->StreamTickBase == nullptr)
    {
        if (HasType && curSegment->ReadQueues.find(pid) != curSegment->ReadQueues.end() &&
            curSegment->ReadQueues[pid].size() > 0)
        {
            stream->StreamTickBase =
                make_shared<Timestamp>(curSegment->Discontinous &&
                    curSegment->ReadQueues[pid].back()->DiscontinousTS != nullptr ?
                    curSegment->ReadQueues[pid].back()->DiscontinousTS->ValueInTicks
                    : curSegment->ReadQueues[pid].back()->SamplePTS->ValueInTicks);
        }
        else if (oldSrcSeg != nullptr && oldSrcSeg->GetCurrentState() == INMEMORYCACHE)
        {
            if (oldSrcSeg->HasMediaType(type))
            {
                auto pid = oldSrcSeg->GetPIDForMediaType(type);
                if (oldSrcSeg->ReadQueues.find(pid) != oldSrcSeg->ReadQueues.end() &&
                    oldSrcSeg->ReadQueues[pid].size() > 0)
                {
                    stream->StreamTickBase =
                        make_shared<Timestamp>(oldSrcSeg->Discontinous &&
                            oldSrcSeg->ReadQueues[pid].back()->DiscontinousTS != nullptr ?
                            oldSrcSeg->ReadQueues[pid].back()->DiscontinousTS->ValueInTicks
                            : oldSrcSeg->ReadQueues[pid].back()->SamplePTS->ValueInTicks);
                }
            }
        }
        else
        {
            if (IsLive)
            {
                stream->StreamTickBase =
                    make_shared<Timestamp>(StartPTSOriginal->ValueInTicks + cumDur);
            }
            else
            {
                stream->StreamTickBase =
                    make_shared<Timestamp>(oldSrcSeg != nullptr ? oldSrcSeg->CumulativeDuration : StartPTSOriginal->ValueInTicks);
            }
        }
    }

    if (stream->StreamTickBase != nullptr)
    {
        stream->StreamTickBase->ValueInTicks +=
            stream->ApproximateFrameDistance == 0 ?
            (type == VIDEO ? DEFAULT_VIDEO_STREAMTICK_OFFSET : DEFAULT_AUDIO_STREAMTICK_OFFSET) :
            stream->ApproximateFrameDistance;

        unsigned long long ts;
        if (IsLive)
        {
            ts = stream->StreamTickBase->ValueInTicks;
        }
        else
        {
            ts = curSegment->TSAbsoluteToRelative(stream->StreamTickBase)->ValueInTicks;
        }


        stream->NotifyStreamTick(ts);
        pSample->SetSampleTime(ts);
        LOG("Empty sample : " << (type == VIDEO ? L"VIDEO" : L"AUDIO") << " Stream Tick TS = " << ts);
    }
}

void Playlist::SetupStreamTick(ContentType type, shared_ptr<MediaSegment> curSegment, shared_ptr<MediaSegment> oldSrcSeg)
{
    auto stream = (type == VIDEO ? dynamic_cast<CMFStreamCommonImpl*>(cpMediaSource->cpVideoStream.Get()) : dynamic_cast<CMFStreamCommonImpl*>(cpMediaSource->cpAudioStream.Get()));

    if (stream == nullptr)
        return;

    bool HasType = curSegment->HasMediaType(type);
    auto pid = HasType ? curSegment->GetPIDForMediaType(type) : 0;
    auto cumDur = (type == VIDEO ? cpMediaSource->spRootPlaylist->LiveVideoPlaybackCumulativeDuration :
        cpMediaSource->spRootPlaylist->LiveAudioPlaybackCumulativeDuration);

    if (stream->StreamTickBase == nullptr)
    {
        if (HasType && curSegment->ReadQueues.find(pid) != curSegment->ReadQueues.end() &&
            curSegment->ReadQueues[pid].size() > 0)
        {
            stream->StreamTickBase =
                make_shared<Timestamp>(curSegment->Discontinous &&
                    curSegment->ReadQueues[pid].back()->DiscontinousTS != nullptr ?
                    curSegment->ReadQueues[pid].back()->DiscontinousTS->ValueInTicks
                    : curSegment->ReadQueues[pid].back()->SamplePTS->ValueInTicks);
        }
        else if (oldSrcSeg != nullptr && oldSrcSeg->GetCurrentState() == INMEMORYCACHE)
        {
            if (oldSrcSeg->HasMediaType(type))
            {
                auto pid = oldSrcSeg->GetPIDForMediaType(type);
                if (oldSrcSeg->ReadQueues.find(pid) != oldSrcSeg->ReadQueues.end() &&
                    oldSrcSeg->ReadQueues[pid].size() > 0)
                {
                    stream->StreamTickBase =
                        make_shared<Timestamp>(oldSrcSeg->Discontinous &&
                            oldSrcSeg->ReadQueues[pid].back()->DiscontinousTS != nullptr ?
                            oldSrcSeg->ReadQueues[pid].back()->DiscontinousTS->ValueInTicks
                            : oldSrcSeg->ReadQueues[pid].back()->SamplePTS->ValueInTicks);
                }
            }
        }
        else
        {
            if (IsLive)
            {
                stream->StreamTickBase =
                    make_shared<Timestamp>(StartPTSOriginal->ValueInTicks + cumDur);
            }
            else
            {
                stream->StreamTickBase =
                    make_shared<Timestamp>(oldSrcSeg != nullptr ? oldSrcSeg->CumulativeDuration : StartPTSOriginal->ValueInTicks);
            }
        }
    }


}

void Playlist::WaitPlaylistRefreshPlaybackResume()
{
    cpMediaSource->StartBuffering();
    LOG("Buffering - waiting for playlist to refresh");
    if (sptceWaitPlaylistRefreshPlaybackResume == nullptr)
    {
        sptceWaitPlaylistRefreshPlaybackResume = make_shared<task_completion_event<HRESULT>>();
    }


    task<HRESULT>(*(sptceWaitPlaylistRefreshPlaybackResume.get())).wait();
    sptceWaitPlaylistRefreshPlaybackResume.reset();
    if (cpMediaSource->IsBuffering())
        cpMediaSource->EndBuffering();

    return;
}

///<summary>Returns a video sample</summary>
///<param name='pPlaylist'>The target playlist</param> 
///<param name='ppSample'>The returned sample</param>
///<returns>HRESULT indicating success or failure</returns> 
HRESULT Playlist::RequestVideoSample(Playlist *pPlaylist, IMFSample** ppSample)
{

    unsigned short PID = 0;
    MediaSegmentState state = MediaSegmentState::UNAVAILABLE;
    bool SegmentPIDEOS = false;
    bool SegmentEOS = false;
    bool StreamEOS = false;
    DWORD buffsize = 0;
    shared_ptr<MediaSegment> curSegment = nullptr;
    shared_ptr<MediaSegment> oldsrcseg = nullptr;
    bool CheckBuffer = false;
    bool brswitch = false;
    bool segswitch = false;
    bool renswitch = false;
    TrackType lasttracktype = TrackType::BOTH;
    unsigned long long lastsamplets = 0;
    Playlist * oldplaylist = nullptr;
    unsigned int oldsegseq = 0;
    unsigned int SkipCounter = 0;


    //get the current segment index
    curSegment = pPlaylist->GetCurrentSegmentTracker(VIDEO);
    oldsrcseg = curSegment;
    if (curSegment->HasMediaType(VIDEO))
    {
        //get the PID for the requested sample's media type
        PID = curSegment->GetPIDForMediaType(VIDEO);
        //ge the EOS flag for this PID
        SegmentPIDEOS = curSegment->IsReadEOS(PID);
        //get the stream EOS flag
        StreamEOS = pPlaylist->IsEOS(VIDEO);
    }
    //get the overall EOS flag for this segment
    SegmentEOS = curSegment->IsReadEOS();
    //get the current state of the segment
    state = curSegment->GetCurrentState();
    lasttracktype = curSegment->HasMediaType(AUDIO) && curSegment->HasMediaType(VIDEO) ? TrackType::BOTH : (curSegment->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);

    if (curSegment->HasMediaType(VIDEO) &&
        curSegment->ReadQueues.find(PID) != curSegment->ReadQueues.end() &&
        curSegment->ReadQueues[PID].size() > 0)
        lastsamplets = curSegment->ReadQueues[PID].back()->SamplePTS->ValueInTicks;

    oldplaylist = pPlaylist;


    /** handle end of stream **/

    /** Handle playlist merge if we are playing live **/
    if (pPlaylist->IsLive)
    {
        if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->spPlaylistRefresh != nullptr)
            CheckBuffer = CheckBuffer || pPlaylist->MergeLivePlaylistChild();
        else if (pPlaylist->IsVariant == false && pPlaylist->spPlaylistRefresh != nullptr)
            CheckBuffer = CheckBuffer || pPlaylist->MergeLivePlaylistMaster();
    }

    //if end of stream
    if (StreamEOS && (!pPlaylist->IsLive || (pPlaylist->IsLive && pPlaylist->LastLiveRefreshProcessed)))
    {
        LOG("Video Scavenge 1");
        //scavenge last segment
        curSegment->Scavenge();

        LOG("RequestVideoSample() : Stream ended")
            //notify end of stream
            return MF_E_END_OF_STREAM;
    }


    if (curSegment->HasMediaType(VIDEO)) //do all of this only if we are playing video
    {

        if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned)
            AdjustSegmentForVideoThinning(pPlaylist, curSegment, PID, SegmentPIDEOS);

        //if end of segment for this PID
        if (SegmentPIDEOS)
        {

            shared_ptr<PlaylistSwitchRequest> videoswitch = nullptr;

            while (true)
            {
                if (curSegment == nullptr)
                {
                    LOG("RequestVideoSample() : Stream ended");
                    //notify end of stream
                    return MF_E_END_OF_STREAM;
                }

                shared_ptr<MediaSegment> nextseg = nullptr;

                brswitch = pPlaylist->CheckAndSwitchBitrate(pPlaylist, ContentType::VIDEO, PID);
                if (brswitch)
                {
                    segswitch = true;

                    nextseg = pPlaylist->GetCurrentSegmentTracker(VIDEO);//reset ;
                    if (nextseg->GetCurrentState() == INMEMORYCACHE && nextseg->HasMediaType(VIDEO))
                        PID = nextseg->GetPIDForMediaType(VIDEO);

                    if (!nextseg->HasMediaType(VIDEO)) //break out if no video or segment loaded successfully
                        break;
                }
                else
                {
                    //forward segment
                    nextseg = pPlaylist->GetNextSegment(curSegment->SequenceNumber, pPlaylist->cpMediaSource->GetCurrentDirection());
                }

                //can this be null ? One scenario would a live stream where the playlist has not refreshed and we are at the last segment already. 
                //Another could be zero length segments from here on till the end of the stream
                //If former, in the next block of code when we check for buffering, we will land into buffering, and the next playlist refresh should bring us back into playback
                //If latter we check  for 
                if (nextseg != nullptr)
                {
                    //rewind segment - in case we seeked back and when we reencounter segment it is partially read - causing a jump forward otherwise
                    if (nextseg->GetCurrentState() == INMEMORYCACHE && pPlaylist->IsLive == false && nextseg->HasMediaType(VIDEO) && !brswitch) //an audio sample has already been read from this segment - so reqind must have happened there
                        nextseg->RewindInCacheSegment(nextseg->GetPIDForMediaType(VIDEO));

                    //if entire segment is EOS (i.e. no more samples of any media type left to read) - scavenge
                    if (SegmentEOS && curSegment->GetCurrentState() == INMEMORYCACHE)
                    {
                        if (pPlaylist->IsLive == false && curSegment->GetCloaking() == nullptr && curSegment->Discontinous == false)
                        {
                            LOG("Video Scavenge 2");
                            curSegment->Scavenge();
                        }
                    }
                    oldsegseq = curSegment->SequenceNumber;
                    if (pPlaylist->IsLive)
                    {
                        if (oldsrcseg->HasMediaType(VIDEO))
                        {
                            auto vidpid = oldsrcseg->GetPIDForMediaType(VIDEO);
                            pPlaylist->cpMediaSource->spRootPlaylist->LiveVideoPlaybackCumulativeDuration += (
                                pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance != 0 ?
                                (((unsigned long long)oldsrcseg->UnreadQueues[vidpid].size() + (oldsrcseg->ReadQueues.find(vidpid) != oldsrcseg->ReadQueues.end() ? (unsigned long long)oldsrcseg->ReadQueues[vidpid].size() : 0UL)) - 1) * pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance :
                                oldsrcseg->Duration
                                );
                        }
                        else
                        {
                            pPlaylist->cpMediaSource->spRootPlaylist->LiveVideoPlaybackCumulativeDuration += oldsrcseg->Duration;
                        }
                    }

                    pPlaylist->SetCurrentSegmentTracker(VIDEO, nextseg);//reset 
                    curSegment = nextseg;
                    if (curSegment->GetCurrentState() == INMEMORYCACHE && curSegment->HasMediaType(VIDEO))
                        PID = nextseg->GetPIDForMediaType(VIDEO);
                    segswitch = true;

                    LOG("Video segment changed from " << oldsegseq << " to " << curSegment->GetSequenceNumber());

                }
                else //check again for stream EOS
                {
                    StreamEOS = pPlaylist->IsEOS(VIDEO);
                    //if end of stream
                    if (StreamEOS)
                    {
                        if (!pPlaylist->IsLive)
                        {
                            LOG("Video Scavenge 3");
                            //scavenge last segment
                            curSegment->Scavenge();

                            LOG("RequestVideoSample() : Stream ended")
                                //notify end of stream
                                return MF_E_END_OF_STREAM;
                        }
                        else
                        {
                            pPlaylist->WaitPlaylistRefreshPlaybackResume();
                            continue;
                        }
                    }
                }


                //get the segment state 
                state = curSegment->GetCurrentState();
                //not in memory ?
                if (state != MediaSegmentState::INMEMORYCACHE)  //are we in a state where there are bits to read ?  
                {

                    LOG("RequestVideoSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");
                    try
                    {
                        if (pPlaylist->pParentRendition != nullptr) //if this is an alternate rendition
                        {
                            auto ret = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CheckAndBufferIfNeeded(curSegment->GetSequenceNumber(), true, segswitch).get();//wait() or get() ?
                            HRESULT hr = std::get<0>(ret);
                            if (SUCCEEDED(hr) && curSegment->LengthInBytes > 0)//got non zero length segment downloaded successfully
                            {

                                if (curSegment->HasMediaType(VIDEO) == false)
                                {
                                    SkipCounter++;
                                    continue;
                                }
                                if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned)
                                {
                                    AdjustSegmentForVideoThinning(pPlaylist, curSegment, PID, SegmentPIDEOS);
                                    if (!SegmentPIDEOS)
                                        break;
                                }
                                else
                                    break;
                            }
                            if (FAILED(hr))//segment failed
                            {

                                if (!Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure)
                                    return E_FAIL;
                            }

                        }
                        else
                        {
                            //will check LAB and rebuild as needed - we wait since we have no bits to supply
                            auto ret = pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ? 
                            HRESULT hr = std::get<0>(ret);
                            if (SUCCEEDED(hr) && curSegment->LengthInBytes > 0)//got non zero length segment downloaded successfully
                            {
                                if (curSegment->HasMediaType(VIDEO) == false)
                                {
                                    SkipCounter++;
                                    continue;
                                }
                                if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned)
                                {
                                    AdjustSegmentForVideoThinning(pPlaylist, curSegment, PID, SegmentPIDEOS);
                                    if (!SegmentPIDEOS)
                                        break;
                                }
                                else
                                    break;
                            }
                            if (FAILED(hr))//segment failed
                            {

                                if (!Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure)
                                    return E_FAIL;
                            }
                        }
                    }
                    catch (task_canceled tcex)
                    {
                        LOG("VIDEO : Task Cancelled");
                    }
                    catch (...)
                    {
                        LOG("VIDEO : Exception");
                        return E_FAIL;
                    }

                }
                else //current segment in memory ?
                {

                    if (curSegment->HasMediaType(VIDEO) == false)
                    {
                        SkipCounter++;
                        continue;
                    }
                    if (pPlaylist->pParentRendition == nullptr) //if this is not an alternate rendition
                    {
                        //if we are playing an alternate rendition and we have the segment in cache - 
                        //no need to check the maintrack since alt track segments are always downloaded 
                        //coupled with main track counter parts so one's existence guarantees the other.
                        try
                        {
                            LOG("RequestVideoSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");

                            pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, false, segswitch);
                        }
                        catch (...)
                        {
                        }
                    }

                    if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned)
                    {
                        AdjustSegmentForVideoThinning(pPlaylist, curSegment, PID, SegmentPIDEOS);
                        if (!SegmentPIDEOS)
                            break;
                    }
                    else
                        break;
                }

                SkipCounter++;
            }
        }
        //if we did something before that required checking for buffer
        else if (CheckBuffer)
        {
            //get the segment state 
            state = curSegment->GetCurrentState();
            //not in memory ?
            if (state != MediaSegmentState::INMEMORYCACHE)  //are we in a state where there are bits to read ?  
            {
                LOG("RequestVideoSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");
                try
                {
                    if (pPlaylist->pParentRendition != nullptr) //if this is an alternate rendition
                    {
                        auto ret = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CheckAndBufferIfNeeded(curSegment->GetSequenceNumber(), true, segswitch).get();//wait() or get() ?
                        if (FAILED(std::get<0>(ret)))
                            return E_FAIL;
                    }
                    else
                    {
                        //will check LAB and rebuild as needed - we wait since we have no bits to supply
                        auto ret = pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ? 
                        if (FAILED(std::get<0>(ret)))
                            return E_FAIL;
                    }
                }
                catch (task_canceled tcex)
                {
                    LOG("VIDEO : Task Cancelled");
                }
                catch (...)
                {
                    LOG("VIDEO : Exception");
                    return E_FAIL;
                }
                if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Thinned)
                    AdjustSegmentForVideoThinning(pPlaylist, curSegment, PID, SegmentPIDEOS);
            }
            else //current segment in memory ?
            {
                if (pPlaylist->pParentRendition == nullptr) //if this is not an alternate rendition
                {
                    //if we are playing an alternate rendition and we have the segment in cache - 
                    //no need to check the maintrack since alt track segments are always downloaded 
                    //coupled with main track counter parts so one's existence guarantees the other.
                    try
                    {
                        LOG("RequestVideoSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");
                        pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, false, segswitch);
                    }
                    catch (...)
                    {
                    }
                }
            }
        }

        if (segswitch && curSegment->HasMediaType(VIDEO)) //we have video
        {
            //get the PID for the requested sample's media type again - since segment switch might change it
            PID = curSegment->GetPIDForMediaType(VIDEO);
        }


#if WINVER < 0x0A00 && WINAPI_FAMILY==WINAPI_FAMILY_PC_APP //win 8.1 Non phone
        if (SkipCounter > 0)
            pPlaylist->cpMediaSource->cpVideoStream->NotifyFormatChanged(nullptr); //drain decoder if we have skipped segments
#elif WINVER >= 0x0A00 && WINAPI_FAMILY!=WINAPI_FAMILY_PHONE_APP //win 10 non phone
        if (SkipCounter > 0)
            pPlaylist->cpMediaSource->cpVideoStream->NotifyFormatChanged(nullptr); //drain decoder if we have skipped segments
#endif   

    }


    if (!brswitch && segswitch)
    {
        std::lock_guard<std::recursive_mutex> lockvid(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch);
        auto videoswitch = pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
        std::lock_guard<std::recursive_mutex> lockaud(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
        auto audioswitch = pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch();

        //we are switching on segment boundaries only and our current segment has moved beyond the originally targeted segment   
        //the current targeted segment will no longer be useful to us
        if (videoswitch != nullptr && videoswitch->targetPlaylist != nullptr)
        {
            if (videoswitch->targetPlaylist->HasCurrentSegmentTracker(VIDEO))
            {

                auto oldtargetseg = videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO);
                //if we are shifting up
                //or shifting down and the segmnt has moved beyond the originally targeted segment (with a down shift the target segment can be at a distance from the current segment since we try to playout any buffer first)
                if (
                    (videoswitch->targetPlaylist->pParentStream->Bandwidth > pPlaylist->pParentStream->Bandwidth) ||
                    (videoswitch->targetPlaylist->pParentStream->Bandwidth < pPlaylist->pParentStream->Bandwidth &&
                        curSegment->SequenceNumber >= videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber)
                    )
                {
                    videoswitch->targetPlaylist->SetCurrentSegmentTracker(VIDEO,
                        videoswitch->targetPlaylist->GetNextSegment(oldtargetseg->SequenceNumber, MFRATE_FORWARD));
                }

                if (oldtargetseg != nullptr && (videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO) == nullptr || oldtargetseg->SequenceNumber < videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber))
                {
                    LOG("Video Scavenge 4");
                    oldtargetseg->Scavenge();
                }

            }
            else
            {
                videoswitch->targetPlaylist->SetCurrentSegmentTracker(VIDEO,
                    videoswitch->targetPlaylist->GetBitrateSwitchTarget(pPlaylist, curSegment.get(), videoswitch->IgnoreBuffer));
            }

            if (audioswitch != nullptr && audioswitch->targetPlaylist != nullptr)
            {
                audioswitch->targetPlaylist->SetCurrentSegmentTracker(AUDIO,
                    videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO));//set to match video tracker
            }
        }

    }


    if (curSegment != nullptr && curSegment->HasMediaType(AUDIO) && brswitch) //we recently switched bitrate at segment boundary whioch would have switched audio as well - so segswitch 
        renswitch = pPlaylist->CheckAndSwitchAudioRendition(pPlaylist, brswitch);

    if (brswitch || curSegment == nullptr)
        curSegment = pPlaylist->GetCurrentSegmentTracker(VIDEO);



    //check to see if VIDEO is still a valid content type for this segment - we may have switched to an audio only bitrate
    if (curSegment->HasMediaType(VIDEO) && curSegment->GetCurrentState() == INMEMORYCACHE)
    {
        //get PID again in case the TS has a different program mapping (can happen when we switch rendition or bitrate)
        PID = curSegment->GetPIDForMediaType(VIDEO);


        if (segswitch || pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance == 0)
        {
            auto oldFrameDist = pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance;

            pPlaylist->cpMediaSource->cpVideoStream->ApproximateFrameDistance = curSegment->GetApproximateFrameDistance(VIDEO, PID);

        }

        if (!brswitch && segswitch) //segment switch with a BR switch is reported elsewhere
        {

            if (curSegment->Discontinous &&
                oldsrcseg != nullptr &&
                oldsrcseg->GetCurrentState() == INMEMORYCACHE &&
                curSegment->SampleReadCount() == 0) //do this only once
            {
                curSegment->UpdateSampleDiscontinuityTimestamps(oldsrcseg, false);
            }
        }

        if (!brswitch && segswitch) //segment switch with a BR switch is reported elsewhere
        {
            if (SkipCounter > 0)
            {
                curSegment->PositionQueueAtNextIDR(PID, pPlaylist->cpMediaSource->curDirection);
                auto vididr = curSegment->PeekNextIDR(pPlaylist->cpMediaSource->curDirection, 0);
                //if we are playing audio - bring that over as well
                if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition() != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->HasCurrentSegmentTracker(AUDIO)) //playing alternate audio
                {
                    auto curseg = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->GetSegmentAtTime(vididr->SamplePTS->ValueInTicks);
                    curseg->AdvanceUnreadQueue(curseg->GetPIDForMediaType(AUDIO), vididr->SamplePTS);
                    pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->SetCurrentSegmentTracker(AUDIO, curseg);
                    if (curseg->GetCurrentState() != INMEMORYCACHE)
                        curseg->DownloadSegmentDataAsync().wait();
                }
                else if (curSegment->HasMediaType(AUDIO))
                {
                    pPlaylist->SetCurrentSegmentTracker(AUDIO, curSegment);

                }
            }

            auto cursegseq = curSegment->SequenceNumber;
            if (pPlaylist->cpMediaSource != nullptr && pPlaylist->cpMediaSource->cpController != nullptr && pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([pPlaylist, oldsegseq, cursegseq]()
            {
                if (pPlaylist->cpMediaSource != nullptr && pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED
                    && pPlaylist->cpMediaSource->cpController != nullptr && pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSegmentSwitched(pPlaylist, pPlaylist, oldsegseq, cursegseq);

                return S_OK;
            }, task_options(task_continuation_context::use_arbitrary())));
        }

        //return the sample
        curSegment->GetNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection(), ppSample);

        if (*ppSample != nullptr)
        {
            if ((curSegment->ReadQueues[PID].size() == 1 && SkipCounter > 0 /*|| curSegment->Discontinous*/) ||//first sample
                (curSegment->ReadQueues[PID].size() > 1 && brswitch) || (curSegment->Discontinous && brswitch) || curSegment->ReadQueues[PID].back()->ForceSampleDiscontinuity)
            {

                LOG("VIDEO Discontinuity");
                (*ppSample)->SetUINT32(MFSampleExtension_Discontinuity, (UINT32)TRUE);
            }
            LOGIF(brswitch || SegmentPIDEOS, "Playlist::RequestVideoSample() - First VIDEO TS on Segment Switch : " << curSegment->ReadQueues[PID].front()->SamplePTS->ValueInTicks);

        }
        else//this should NEVER be - this is here just to protect against unforseen race conditions while we remove bugs
        {
            LOG("VIDEO : Unexpected empty sample");
            auto ptr = pPlaylist->cpMediaSource->cpVideoStream->CreateEmptySample();
            if (ptr != nullptr)
            {
                *ppSample = ptr.Detach();
            }

            pPlaylist->ApplyStreamTick(VIDEO, curSegment, segswitch ? oldsrcseg : nullptr, *ppSample);
        }
    }
    else
    {
        //if a video stream is not playing, but the audio stream index has changed, update the video stream index as well to stay in sync
        if (pPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber != pPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber)
            pPlaylist->SetCurrentSegmentTracker(VIDEO, pPlaylist->GetCurrentSegmentTracker(AUDIO));

        auto ptr = pPlaylist->cpMediaSource->cpVideoStream->CreateEmptySample();
        if (ptr != nullptr)
        {
            *ppSample = ptr.Detach();
        }
        LOG("VIDEO : empty sample");
        pPlaylist->SetupStreamTick(VIDEO, curSegment, segswitch ? oldsrcseg : nullptr);
        if (pPlaylist->spswVideoStreamTick == nullptr)
            pPlaylist->StartVideoStreamTick();
    }

    if (brswitch)
    {
        TrackType curTrackType = curSegment->HasMediaType(AUDIO) && curSegment->HasMediaType(VIDEO) ? TrackType::BOTH : (curSegment->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);

        if (lasttracktype != curTrackType)
        {
            if (lasttracktype == TrackType::BOTH && (curTrackType == TrackType::VIDEO || curTrackType == TrackType::AUDIO)) //a call to br switch from video sample request can switch both A/V to A/V or from A/V to A/Only
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->RaiseStreamSelectionChanged(lasttracktype, curTrackType);

            }
            else if (lasttracktype == TrackType::VIDEO && curTrackType == TrackType::BOTH)
            {
                //set the audio track to synchronize
                pPlaylist->SetCurrentSegmentTracker(AUDIO, pPlaylist->GetCurrentSegmentTracker(VIDEO));
                auto nextvidsample = curSegment->PeekNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection());
                curSegment->AdvanceUnreadQueue(curSegment->GetPIDForMediaType(AUDIO), nextvidsample->SamplePTS);
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->RaiseStreamSelectionChanged(lasttracktype, curTrackType);
            }
        }
    }

    if (segswitch && !brswitch && oldsegseq != curSegment->GetSequenceNumber() && curSegment->HasMediaType(VIDEO) && pPlaylist->pParentStream != nullptr && pPlaylist->cpMediaSource->spRootPlaylist->IsVariant)
    {
        auto oldseg = pPlaylist->GetSegment(oldsegseq);
        unsigned int From = oldseg->GetCloaking() != nullptr ? oldseg->GetCloaking()->pParentPlaylist->pParentStream->Bandwidth : oldseg->pParentPlaylist->pParentStream->Bandwidth;
        unsigned int To = curSegment->GetCloaking() != nullptr ? curSegment->GetCloaking()->pParentPlaylist->pParentStream->Bandwidth : curSegment->pParentPlaylist->pParentStream->Bandwidth;

        if (From != To)
        {
            pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, pPlaylist]()
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(VIDEO, From, To);

                return S_OK;
            }));

        }

    }
    if (pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate == 0.0 && curSegment->HasMediaType(VIDEO) && pPlaylist->pParentStream != nullptr && pPlaylist->cpMediaSource->spRootPlaylist->IsVariant)
    {
        unsigned int From = pPlaylist->cpMediaSource->spHeuristicsManager->GetLastSuggestedBandwidth();
        unsigned int To = curSegment->GetCloaking() != nullptr ? curSegment->GetCloaking()->pParentPlaylist->pParentStream->Bandwidth : curSegment->pParentPlaylist->pParentStream->Bandwidth;

        if (From != To)
        {
            pPlaylist->cpMediaSource->spHeuristicsManager->SetLastSuggestedBandwidth(To);
            pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, pPlaylist]()
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(VIDEO, From, To);
                return S_OK;
            }));

            if (curSegment->HasMediaType(AUDIO))
                pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, pPlaylist]()
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(AUDIO, From, To);

                return S_OK;
            }));
        }
    }

    if (oldsrcseg != nullptr && oldsrcseg->GetCurrentState() == INMEMORYCACHE && oldsrcseg->IsReadEOS() && segswitch
        && ((curSegment->HasMediaType(AUDIO) && pPlaylist->HasCurrentSegmentTracker(AUDIO) && pPlaylist->HasCurrentSegmentTracker(VIDEO) &&
            pPlaylist->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber() == pPlaylist->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber()) || !curSegment->HasMediaType(AUDIO)))
    {
        LOG("Video Scavenge 6");
        oldsrcseg->Scavenge();
    }


    return S_OK;
}



///<summary>Returns an audio sample</summary>
///<param name='pPlaylist'>The target playlist</param> 
///<param name='ppSample'>The returned sample</param>
///<returns>HRESULT indicating success or failure</returns>

HRESULT Playlist::RequestAudioSample(Playlist *pPlaylist, IMFSample** ppSample)
{

    //NOTE: Most of the code below is identical to the RequestVideoSample() method. Please refer to that method for code comments.
    unsigned short PID = 0;
    MediaSegmentState state = MediaSegmentState::UNAVAILABLE;
    bool SegmentPIDEOS = false;
    bool SegmentEOS = false;
    bool StreamEOS = false;
    DWORD buffsize = 0;
    shared_ptr<MediaSegment> curSegment = nullptr;

    shared_ptr<MediaSegment> oldsrcseg = nullptr;
    bool CheckBuffer = false;
    bool brswitch = false;
    bool renswitch = false;
    bool segswitch = false;
    TrackType lasttracktype = TrackType::BOTH;
    unsigned long long lastsamplets = 0;
    Playlist * oldplaylist = nullptr;
    unsigned int oldsegseq = 0;
    unsigned int SkipCounter = 0;


    //get the current segment index
    curSegment = pPlaylist->GetCurrentSegmentTracker(AUDIO);
    oldsrcseg = curSegment;
    if (curSegment->HasMediaType(AUDIO))
    {
        //get the PID for the requested sample's media type
        PID = curSegment->GetPIDForMediaType(AUDIO);
        //ge the EOS flag for this PID
        SegmentPIDEOS = curSegment->IsReadEOS(PID);
        //get the stream EOS flag
        StreamEOS = pPlaylist->IsEOS(AUDIO);
    }
    //get the overall EOS flag for this segment
    SegmentEOS = curSegment->IsReadEOS();
    //get the current state of the segment
    state = curSegment->GetCurrentState();

    lasttracktype = curSegment->HasMediaType(AUDIO) && curSegment->HasMediaType(VIDEO) ? TrackType::BOTH : (curSegment->HasMediaType(AUDIO) ? TrackType::AUDIO : TrackType::VIDEO);

    if (curSegment->HasMediaType(AUDIO) &&
        curSegment->ReadQueues.find(PID) != curSegment->ReadQueues.end() &&
        curSegment->ReadQueues[PID].size() > 0)
        lastsamplets = curSegment->ReadQueues[PID].back()->SamplePTS->ValueInTicks;

    oldplaylist = pPlaylist;


    /** handle end of stream **/

    /** Handle playlist merge if we are playing live **/
    if (pPlaylist->IsLive)
    {
        if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->spPlaylistRefresh != nullptr)
            CheckBuffer = CheckBuffer || pPlaylist->MergeLivePlaylistChild();
        else if (pPlaylist->IsVariant == false && pPlaylist->spPlaylistRefresh != nullptr)
            CheckBuffer = CheckBuffer || pPlaylist->MergeLivePlaylistMaster();
    }

    //if end of stream
    if (StreamEOS && (!pPlaylist->IsLive || (pPlaylist->IsLive && pPlaylist->LastLiveRefreshProcessed)))
    {
        LOG("Audio Scavenge 1");
        //scavenge last segment
        curSegment->Scavenge();
        LOG("RequestAudioSample() : Stream ended")
            //notify end of stream
            return MF_E_END_OF_STREAM;
    }


    //if end of segment for this PID
    if (SegmentPIDEOS)
    {
        //forward segment  
        shared_ptr<PlaylistSwitchRequest> audioswitch = nullptr;
        while (true)
        {
            if (curSegment == nullptr)
            {
                LOG("RequestAudioSample() : Stream ended");
                //notify end of stream
                return MF_E_END_OF_STREAM;
            }

            shared_ptr<MediaSegment> nextseg = nullptr;

            brswitch = pPlaylist->CheckAndSwitchBitrate(pPlaylist, ContentType::AUDIO, PID);
            if (brswitch)
            {
                segswitch = true;

                nextseg = pPlaylist->GetCurrentSegmentTracker(AUDIO);//reset ;
                if (nextseg->GetCurrentState() == INMEMORYCACHE && nextseg->HasMediaType(AUDIO))
                    PID = nextseg->GetPIDForMediaType(AUDIO);

                if (!nextseg->HasMediaType(AUDIO)) //break if no audio
                    break;
            }
            else
            {

                nextseg = pPlaylist->GetNextSegment(curSegment->SequenceNumber, pPlaylist->cpMediaSource->GetCurrentDirection());
            }
            //can this be null ? The only scenario would a live stream where the playlist has not refreshed and we are at the last segment already. 
            //If so in the next block of code when we check for buffering, we will land into buffering, and the next playlist refresh should bring us back into playback
            if (nextseg != nullptr)
            {

                //rewind segment - in case we seeked back and when we reencounter segment it is partially read - causing a jump forward otherwise
                if (nextseg->GetCurrentState() == INMEMORYCACHE && pPlaylist->IsLive == false && nextseg->HasMediaType(AUDIO) && !brswitch)
                    nextseg->RewindInCacheSegment(nextseg->GetPIDForMediaType(AUDIO));

                //if entire segment is EOS (i.e. no more samples of any media type left to read) - scavenge
                if (SegmentEOS && curSegment->GetCurrentState() == INMEMORYCACHE)
                {
                    if (pPlaylist->IsLive == false && curSegment->GetCloaking() == nullptr && curSegment->Discontinous == false)
                    {
                        LOG("Audio Scavenge 2");
                        curSegment->Scavenge();
                    }
                }
                oldsegseq = curSegment->SequenceNumber;
                if (pPlaylist->IsLive)
                {
                    if (oldsrcseg->HasMediaType(AUDIO))
                    {
                        auto audpid = oldsrcseg->GetPIDForMediaType(AUDIO);
                        pPlaylist->cpMediaSource->spRootPlaylist->LiveAudioPlaybackCumulativeDuration += (
                            pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance != 0 ?
                            (((unsigned long long)oldsrcseg->UnreadQueues[audpid].size() + (oldsrcseg->ReadQueues.find(audpid) != oldsrcseg->ReadQueues.end() ? (unsigned long long)oldsrcseg->ReadQueues[audpid].size() : 0UL)) - 1) * pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance :
                            oldsrcseg->Duration
                            );
                    }
                    else
                    {
                        pPlaylist->cpMediaSource->spRootPlaylist->LiveAudioPlaybackCumulativeDuration += oldsrcseg->Duration;
                    }
                }

                pPlaylist->SetCurrentSegmentTracker(AUDIO, nextseg);
                curSegment = nextseg;
                if (curSegment->GetCurrentState() == INMEMORYCACHE  && curSegment->HasMediaType(AUDIO))
                    PID = curSegment->GetPIDForMediaType(AUDIO);

                segswitch = true;
                LOG("Audio segment changed from " << oldsegseq << " to " << curSegment->GetSequenceNumber());
            }
            else //check again for stream EOS
            {

                StreamEOS = pPlaylist->IsEOS(AUDIO);
                //if end of stream
                if (StreamEOS)
                {
                    if (!pPlaylist->IsLive)
                    {
                        LOG("Audio Scavenge 3");
                        //scavenge last segment
                        curSegment->Scavenge();

                        LOG("RequestAudioSample() : Stream ended")
                            //notify end of stream
                            return MF_E_END_OF_STREAM;
                    }
                    else
                    {
                        pPlaylist->WaitPlaylistRefreshPlaybackResume();
                        continue;
                    }

                }
            }

            //if we are playing alternate audio - keep the audio segment tracker in the main segment in sync
            if (pPlaylist->pParentRendition != nullptr &&
                pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->HasCurrentSegmentTracker(AUDIO))
            {
                pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->SetCurrentSegmentTracker(AUDIO, pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetSegment(curSegment->SequenceNumber));
            }

            state = curSegment->GetCurrentState();

            if (state != MediaSegmentState::INMEMORYCACHE)  //are we in a state where there are bits to read ?  
            {

                LOG("RequestAudioSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");

                try
                {

                    if (pPlaylist->pParentRendition != nullptr) //check the main playlist 
                    {
                        auto ret = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ?;
                        HRESULT hr = std::get<0>(ret);
                        if (SUCCEEDED(hr) && curSegment->LengthInBytes > 0)//got non zero length segment downloaded successfully 
                        {
                            if (curSegment->HasMediaType(AUDIO) == false)
                            {
                                SkipCounter++;
                                continue;
                            }
                            else
                                break;
                        }

                        if (FAILED(hr))//segment failed
                        {
                            if (!Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure)
                                return E_FAIL;
                        }
                    }
                    else
                    {
                        auto ret = pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ?; 
                        HRESULT hr = std::get<0>(ret);
                        if (SUCCEEDED(hr) && curSegment->LengthInBytes > 0)//got non zero length segment downloaded successfully 
                        {
                            if (curSegment->HasMediaType(AUDIO) == false)
                            {
                                SkipCounter++;
                                continue;
                            }
                            else
                                break;
                        }
                        if (FAILED(hr))//segment failed
                        {


                            if (!Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure)
                                return E_FAIL;
                        }
                    }
                }
                catch (task_canceled)
                {
                    LOG("AUDIO : Task Cancelled");
                }
                catch (...)
                {
                    LOG("AUDIO : Exception");
                    return E_FAIL;
                }

            }
            else
            {
                if (curSegment->HasMediaType(AUDIO) == false)
                {
                    SkipCounter++;
                    continue;
                }
                //if this is the main track
                if (pPlaylist->pParentRendition == nullptr)
                {
                    //if we are playing an alternate rendition and we have the segment in cache - 
                    //no need to check the maintrack since alt track segments are always downloaded 
                    //coupled with main track counter parts so one's existence guarantees the other.
                    try
                    {
                        LOG("RequestAudioSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");
                        pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, false, segswitch);
                    }
                    catch (...)
                    {
                    }
                }

                break;
            }

            SkipCounter++;
        }
    }
    else if (CheckBuffer)
    {

        state = curSegment->GetCurrentState();

        if (state != MediaSegmentState::INMEMORYCACHE)  //are we in a state where there are bits to read ?  
        {
            LOG("RequestAudioSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");

            try
            {

                if (pPlaylist->pParentRendition != nullptr) //check the main playlist 
                {
                    auto ret = pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ?;
                    if (FAILED(std::get<0>(ret)))
                        return E_FAIL;
                }
                else
                {
                    auto ret = pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, true, segswitch).get();//wait() or get() ?; 
                    if (FAILED(std::get<0>(ret)))
                        return E_FAIL;
                }
            }
            catch (task_canceled)
            {
                LOG("AUDIO : Task Cancelled");
            }
            catch (...)
            {
                LOG("AUDIO : Exception");
                return E_FAIL;
            }

        }
        else
        {
            //if this is the main track
            if (pPlaylist->pParentRendition == nullptr)
            {
                //if we are playing an alternate rendition and we have the segment in cache - 
                //no need to check the maintrack since alt track segments are always downloaded 
                //coupled with main track counter parts so one's existence guarantees the other.
                try
                {
                    LOG("RequestAudioSample():Calling CheckAndBufferIfNeeded(" << curSegment->SequenceNumber << ")");
                    pPlaylist->CheckAndBufferIfNeeded(curSegment->SequenceNumber, false, segswitch);
                }
                catch (...)
                {
                }

            }
        }
        if (curSegment->HasMediaType(AUDIO)) //we have audio
        {
            //get the PID for the requested sample's media type again - since segment switch might change it
            PID = curSegment->GetPIDForMediaType(AUDIO);

        }
    }

    //ensure that the target playlist has a start PTS that will cause an increasing timestamp sequence. If not offset the target start PTS by the last sample TS to ensure linear continuity


    //we are switching on segment boundaries only and our current segment has moved across the originally targeted segment - only do this if we are playing just audio - otherwise this will be done in the video sample request
    if (!brswitch && segswitch &&
        !curSegment->HasMediaType(VIDEO) &&
        curSegment->SequenceNumber >= pPlaylist->MaxBufferedSegment()->SequenceNumber)//the current targeted segment will no longer be useful to us
    {
        std::lock_guard<std::recursive_mutex> lockswitch(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch);
        auto audioswitch = pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch();
        if (audioswitch != nullptr && audioswitch->targetPlaylist != nullptr)
        {
            if (audioswitch->targetPlaylist->HasCurrentSegmentTracker(AUDIO))
            {

                auto oldtargetseg = audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO);
                //if we are shifting up
                //or shifting down and the segmnt has moved beyond the originally targeted segment (with a down shift the target segment can be at a distance from the current segment since we try to playout any buffer first)
                if (
                    (audioswitch->targetPlaylist->pParentStream->Bandwidth > pPlaylist->pParentStream->Bandwidth) ||
                    (audioswitch->targetPlaylist->pParentStream->Bandwidth < pPlaylist->pParentStream->Bandwidth &&
                        curSegment->SequenceNumber >= audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber)
                    )
                {
                    audioswitch->targetPlaylist->SetCurrentSegmentTracker(AUDIO,
                        audioswitch->targetPlaylist->GetNextSegment(oldtargetseg->SequenceNumber, MFRATE_FORWARD));
                }

                if (oldtargetseg != nullptr && (audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO) == nullptr
                    || oldtargetseg->SequenceNumber < audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber))
                {

                    LOG("Audio Scavenge 4");
                    oldtargetseg->Scavenge();
                }
            }
            else
            {
                audioswitch->targetPlaylist->SetCurrentSegmentTracker(AUDIO,
                    audioswitch->targetPlaylist->GetBitrateSwitchTarget(pPlaylist, curSegment.get(), audioswitch->IgnoreBuffer));
            }
        }
    }
    if (segswitch)
        renswitch = pPlaylist->CheckAndSwitchAudioRendition(pPlaylist, brswitch);

    if (brswitch || curSegment == nullptr || renswitch)
        curSegment = pPlaylist->GetCurrentSegmentTracker(AUDIO);


    //if this is an alternate audio rendition and if we did not get to construct elementary audio samples earlier, do it now
    if (pPlaylist->pParentRendition != nullptr && curSegment->PeekNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection()) == nullptr)
    {
        curSegment->BuildAudioElementaryStreamSamples();
        if (curSegment->HasMediaType(AUDIO)) //we have audio
        {
            //get the PID for the requested sample's media type again - since segment switch might change it
            PID = curSegment->GetPIDForMediaType(AUDIO);
        }
    }


    //check to see if AUDIO is still a valid content type for this segment - we may have switched to an video only bitrate
    if (curSegment->HasMediaType(AUDIO) && curSegment->GetCurrentState() == INMEMORYCACHE)
    {

        //get PID again in case the TS has a different program mapping (can heppen when we switch rendition or bitrate)
        PID = curSegment->GetPIDForMediaType(AUDIO);


        if (segswitch || pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance == 0)
        {
            auto oldFrameDist = pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance;

            pPlaylist->cpMediaSource->cpAudioStream->ApproximateFrameDistance = curSegment->GetApproximateFrameDistance(AUDIO, PID);

        }

        if (!brswitch && segswitch) //segment switch - if only audio is playing - otherwise reported by video sample request
        {

            if (curSegment->Discontinous &&
                oldsrcseg != nullptr &&
                oldsrcseg->GetCurrentState() == INMEMORYCACHE &&
                curSegment->SampleReadCount() == 0) //do this only once
            {
                curSegment->UpdateSampleDiscontinuityTimestamps(oldsrcseg, false);
            }
        }

        if (!brswitch && segswitch) //segment switch - if only audio is playing - otherwise reported by video sample request
        {
            if (SkipCounter > 0 && curSegment->HasMediaType(VIDEO))
            {
                auto vididr = curSegment->PeekNextIDR(pPlaylist->cpMediaSource->curDirection, 0);
                if (vididr != nullptr)
                    curSegment->AdvanceUnreadQueue(PID, vididr->SamplePTS);
            }
        }

        if (!brswitch && segswitch && (!curSegment->HasMediaType(VIDEO) || Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO)) //segment switch - if only audio is playing - otherwise reported by video sample request
        {

            auto cursegseq = curSegment->SequenceNumber;
            if (pPlaylist->cpMediaSource != nullptr && pPlaylist->cpMediaSource->cpController != nullptr && pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr
                && pPlaylist->pParentStream != nullptr) //not for alternate renditions
                pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([pPlaylist, oldsegseq, cursegseq]()
            {
                if (pPlaylist->cpMediaSource != nullptr && pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED
                    && pPlaylist->cpMediaSource->cpController != nullptr && pPlaylist->cpMediaSource->cpController->GetPlaylist() != nullptr)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseSegmentSwitched(pPlaylist, pPlaylist, oldsegseq, cursegseq);

                return S_OK;
            }, task_options(task_continuation_context::use_arbitrary())));
        }



        //return the sample
        curSegment->GetNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection(), ppSample);



        if (*ppSample != nullptr)//this should NEVER be - this is here just to protect against unforseen race conditions while we remove bugs
        {

            //if we are playing alternate audio - keep the audio segment on the main track synchronized
            //if we are playing alternate audio - keep the audio segment tracker in the main segment in sync
            if (pPlaylist->pParentRendition != nullptr &&
                pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->HasCurrentSegmentTracker(AUDIO) &&
                pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(AUDIO)->HasMediaType(AUDIO))
            {
                auto nextsample = curSegment->PeekNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection());
                unsigned long long syncpos = (nextsample == nullptr ? pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(AUDIO)->CumulativeDuration : nextsample->SamplePTS->ValueInTicks);
                pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(AUDIO)->SetCurrentPosition(pPlaylist->pParentRendition->pParentPlaylist->ActiveVariant->spPlaylist->GetCurrentSegmentTracker(AUDIO)->GetPIDForMediaType(AUDIO),
                    syncpos,
                    pPlaylist->cpMediaSource->GetCurrentDirection(), true);
            }

            if ((curSegment->ReadQueues[PID].size() == 1 && SkipCounter > 0 /*|| curSegment->Discontinous*/) ||//first sample
                (oldsrcseg != nullptr && oldsrcseg->HasMediaType(AUDIO) && oldsrcseg->IsReadEOS(oldsrcseg->GetPIDForMediaType(AUDIO)) == false && brswitch)
                || (curSegment->Discontinous && brswitch) || curSegment->ReadQueues[PID].back()->ForceSampleDiscontinuity)
            {

                LOG("AUDIO Discontinuity");
                (*ppSample)->SetUINT32(MFSampleExtension_Discontinuity, (UINT32)TRUE);

            }
            LOGIF(brswitch || SegmentPIDEOS, "First AUDIO TS on Segment Switch : " << curSegment->ReadQueues[PID].front()->SamplePTS->ValueInTicks);
        }
        else
        {
            LOG("AUDIO : Unexpected empty sample");
            auto ptr = pPlaylist->cpMediaSource->cpAudioStream->CreateEmptySample();
            if (ptr != nullptr)
                *ppSample = ptr.Detach();
            pPlaylist->ApplyStreamTick(AUDIO, curSegment, segswitch ? oldsrcseg : nullptr, *ppSample);
        }

    }
    else
    {
        //if a audio stream is not playing, but the video stream index has changed, update the audio stream index as well to stay in sync
        if (pPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber != pPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber)
            pPlaylist->SetCurrentSegmentTracker(AUDIO, pPlaylist->GetCurrentSegmentTracker(VIDEO));
        auto ptr = pPlaylist->cpMediaSource->cpAudioStream->CreateEmptySample();
        if (ptr != nullptr)
            *ppSample = ptr.Detach();
        pPlaylist->ApplyStreamTick(AUDIO, curSegment, segswitch ? oldsrcseg : nullptr, *ppSample);
    }

    if (brswitch)
    {
        TrackType curTrackType = curSegment->HasMediaType(AUDIO) && curSegment->HasMediaType(VIDEO) ? TrackType::BOTH : (curSegment->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);
        if (lasttracktype != curTrackType)
        {

            if (lasttracktype == TrackType::BOTH && curTrackType == TrackType::AUDIO) //calls from audio sample request either switches from A/Only to A/V or vice versa
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->RaiseStreamSelectionChanged(lasttracktype, curTrackType);

            }
            else if (lasttracktype == TrackType::AUDIO && curTrackType == TrackType::BOTH)
            {
                pPlaylist->SetCurrentSegmentTracker(VIDEO, pPlaylist->GetCurrentSegmentTracker(AUDIO));
                //set the video track to synchronize
                auto nextaudsample = curSegment->PeekNextSample(PID, pPlaylist->cpMediaSource->GetCurrentDirection());
                curSegment->AdvanceUnreadQueue(curSegment->GetPIDForMediaType(VIDEO), nextaudsample->SamplePTS);
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->RaiseStreamSelectionChanged(lasttracktype, curTrackType);
            }
        }
    }

    if (segswitch && !brswitch && oldsegseq != curSegment->GetSequenceNumber() && curSegment->HasMediaType(AUDIO) && pPlaylist->pParentStream != nullptr && pPlaylist->cpMediaSource->spRootPlaylist->IsVariant)
    {
        auto oldseg = pPlaylist->GetSegment(oldsegseq);
        unsigned int From = oldseg->GetCloaking() != nullptr ? oldseg->GetCloaking()->pParentPlaylist->pParentStream->Bandwidth : oldseg->pParentPlaylist->pParentStream->Bandwidth;
        unsigned int To = curSegment->GetCloaking() != nullptr ? curSegment->GetCloaking()->pParentPlaylist->pParentStream->Bandwidth : curSegment->pParentPlaylist->pParentStream->Bandwidth;

        if (From != To)
        {
            pPlaylist->cpMediaSource->protectionRegistry.Register(task<HRESULT>([From, To, pPlaylist]()
            {
                if (pPlaylist->cpMediaSource->GetCurrentState() != MSS_ERROR && pPlaylist->cpMediaSource->GetCurrentState() != MSS_UNINITIALIZED)
                    pPlaylist->cpMediaSource->cpController->GetPlaylist()->RaiseBitrateSwitchCompleted(AUDIO, From, To);

                return S_OK;
            }));
        }
    }

    if (oldsrcseg != nullptr && oldsrcseg->GetCurrentState() == INMEMORYCACHE && oldsrcseg->IsReadEOS() && segswitch
        && ((curSegment->HasMediaType(VIDEO) && pPlaylist->HasCurrentSegmentTracker(AUDIO) && pPlaylist->HasCurrentSegmentTracker(VIDEO) &&
            pPlaylist->GetCurrentSegmentTracker(AUDIO)->GetSequenceNumber() == pPlaylist->GetCurrentSegmentTracker(VIDEO)->GetSequenceNumber()) || !curSegment->HasMediaType(VIDEO)))
    {
        LOG("Audio Scavenge 6");
        oldsrcseg->Scavenge();
    }

    return S_OK;
}



task<tuple<HRESULT, unsigned int>> Playlist::CheckAndBufferIfNeeded(unsigned int CurSegSeqNum, bool ForceWait, bool segswitch)
{
    LOG("CheckAndBufferIfNeeded(" << CurSegSeqNum << ") : ForceWait = " << (ForceWait ? "TRUE" : "FALSE"));

    if ((this->pParentRendition == nullptr) &&//not an alternate rendition playlist
        ((this->pParentStream != nullptr && this->pParentStream->IsActive) || //active main playlist
            (this->pParentStream == nullptr && !this->IsVariant))) //or non variant media playlist
    {
        //get the LAB length starting at given segment index
        unsigned long long time = GetCurrentLABLength(CurSegSeqNum, false, Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate));

        if (PauseBufferBuilding && time < DerivedTargetDuration * 2)
            PauseBufferBuilding = false;

        LOG("Buffer Left = " << time << ", Required LAB = " << Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate));

        if (time >= Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate) &&
            cpMediaSource->IsBuffering())
            cpMediaSource->EndBuffering();
        else if (time <= 0 && !cpMediaSource->IsBuffering())
        {//no buffer left   
            if ((IsLive || (!IsLive && CurSegSeqNum != Segments.back()->GetSequenceNumber() && (TotalDuration - GetSegment(CurSegSeqNum)->CumulativeDuration > time))))
            {

                LOG("CheckAndBufferIfNeeded - Starting Source Buffering on LAB Depletion..");
                cpMediaSource->StartBuffering(false, true); //put MF Media Source to buffering state   
            }
        }

        //if LAB less than what config stipulates on the main playlist

        if (time < Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate) && !PauseBufferBuilding)
        {
            //start a chained download at given segment, with ForceWait or the current media source 
            //buffering state determining whether StartStreamingAsync should wait for an actual segment download before returning
            LOG("CheckAndBufferIfNeeded -> StartStreamingAsync() : for " << CurSegSeqNum << ",Chained,ForceWait = " << (ForceWait ? true : (cpMediaSource->IsBuffering() ? true : false)));
            return StartStreamingAsync(this, CurSegSeqNum, true, ForceWait ? true : (cpMediaSource->IsBuffering() ? true : false));
        }
        //else if there is an alternate rendition and it is not downloaded
        else if (this->pParentStream != nullptr && this->pParentStream->GetActiveAudioRendition() != nullptr &&
            this->pParentStream->GetActiveAudioRendition()->spPlaylist->GetSegment(CurSegSeqNum)->GetCurrentState() != INMEMORYCACHE)
        {
            LOG("Starting to chain stream alternate audio segment" << CurSegSeqNum);
            //download alternate
            return StartStreamingAsync(this, CurSegSeqNum, true, ForceWait ? true : false);
        }
        //else if this is an alternate rendition and it is not downloaded
        else if (this->pParentStream != nullptr && this->pParentStream->GetActiveVideoRendition() != nullptr &&
            this->pParentStream->GetActiveVideoRendition()->spPlaylist->GetSegment(CurSegSeqNum)->GetCurrentState() != INMEMORYCACHE)
        {
            LOG("Starting to chain stream alternate video segment" << CurSegSeqNum);
            //download alternate
            return StartStreamingAsync(this, CurSegSeqNum, true, ForceWait ? true : false);
        }
    }


    //return immediate
    return task<tuple<HRESULT, unsigned int>>([CurSegSeqNum]()
    {
        return tuple<HRESULT, unsigned int>(S_OK, CurSegSeqNum);
    });

}

HRESULT Playlist::CheckAndBufferForBRSwitch(unsigned int CurSegSeqNum)
{
    LOG("CheckAndBufferForBRSwitch(" << CurSegSeqNum << ")");


    try
    {

        if ((this->pParentRendition == nullptr) &&//not an alternate rendition playlist
            ((this->pParentStream != nullptr && this->pParentStream->IsActive) || //active main playlist
                (this->pParentStream == nullptr && !this->IsVariant))) //or non variant media playlist
        {
            //get the LAB length starting at given segment index
            unsigned long long time = GetCurrentLABLength(CurSegSeqNum,
                false,
                Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate));

            if (PauseBufferBuilding && time < DerivedTargetDuration * 2)
                PauseBufferBuilding = false;

            LOG("Buffer Left = " << time << ", Required LAB = " << Configuration::GetCurrent()->GetRateAdjustedLABThreshold(cpMediaSource->curPlaybackRate->Rate));
            {

                if (std::try_lock(cpMediaSource->cpVideoStream->LockSwitch, cpMediaSource->cpAudioStream->LockSwitch) < 0)
                {
                    std::lock_guard<std::recursive_mutex> lockv(cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
                    std::lock_guard<std::recursive_mutex> locka(cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

                    auto videoswitch = cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
                    auto audioswitch = cpMediaSource->cpAudioStream->GetPendingBitrateSwitch();

                    auto videoplaylist = videoswitch != nullptr ? videoswitch->targetPlaylist : nullptr;
                    auto audioplaylist = audioswitch != nullptr ? audioswitch->targetPlaylist : nullptr;

                    if (videoswitch != nullptr &&
                        videoswitch->VideoSwitchedTo == nullptr &&
                        videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO) != nullptr)//trying to switch bitrate and have not found suitable video frame to switch to yet
                    {

                        if (videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->GetCurrentState() != INMEMORYCACHE)
                        {
                            auto curSegment = GetSegment(CurSegSeqNum);
                            TrackType curTrackType = curSegment->HasMediaType(AUDIO) && curSegment->HasMediaType(VIDEO) ?
                                TrackType::BOTH : (curSegment->HasMediaType(VIDEO) ? TrackType::VIDEO : TrackType::AUDIO);


                            if ((time == 0) ||
                                (curTrackType != TrackType::BOTH /*&& switchTrackType == TrackType::BOTH*/ &&
                                    CurSegSeqNum == videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber))
                            {
                                LOG("CheckAndBufferIfNeeded - Starting Source Buffering to obtain video switch target : Seq " << videoplaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber);
                                cpMediaSource->StartBuffering();

                                auto res = Playlist::StartStreamingAsync(
                                    videoplaylist,
                                    videoplaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber, true, true, true).get();

                                if (!SUCCEEDED(std::get<0>(res))) throw std::get<0>(res);

                                videoswitch->BufferBuildStarted = true;
                                if (audioswitch != nullptr)
                                    audioswitch->BufferBuildStarted = true;
                                cpMediaSource->EndBuffering();
                            }
                            else if (videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->GetCurrentState() != DOWNLOADING &&
                                videoswitch->BufferBuildStarted == false)
                            {

                                LOG("CheckAndBufferIfNeeded - Starting Streaming  video switch target : Seq " << videoplaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber);
                                videoswitch->BufferBuildStarted = true;
                                if (audioswitch != nullptr)
                                    audioswitch->BufferBuildStarted = true;
                                Playlist::StartStreamingAsync(
                                    videoplaylist,
                                    videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber, true, false, true);
                            }
                        }
                        else
                        {

                            if (videoswitch->BufferBuildStarted == false)
                            {

                                LOG("CheckAndBufferIfNeeded - Starting Streaming  video switch target : Seq " << videoplaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber);
                                videoswitch->BufferBuildStarted = true;
                                if (audioswitch != nullptr)
                                    audioswitch->BufferBuildStarted = true;


                                Playlist::StartStreamingAsync(
                                    videoplaylist,
                                    videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber, true, false, true);

                            }
                        }
                    }
                    if (audioswitch != nullptr && audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO) != nullptr)
                    {
                        if (audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->GetCurrentState() != INMEMORYCACHE)
                        {
                            if (time == 0)
                            {
                                LOG("CheckAndBufferIfNeeded - Starting Source Buffering to obtain audio switch target : Seq " << audioplaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber);
                                cpMediaSource->StartBuffering();

                                auto res = Playlist::StartStreamingAsync(
                                    audioplaylist,
                                    audioplaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber, true, true, true).get();

                                if (!SUCCEEDED(std::get<0>(res))) throw std::get<0>(res);

                                if (audioswitch != nullptr)
                                    audioswitch->BufferBuildStarted = true;

                                cpMediaSource->EndBuffering();
                            }
                            else if (audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->GetCurrentState() != DOWNLOADING &&
                                audioswitch->BufferBuildStarted == false)
                            {
                                LOG("CheckAndBufferIfNeeded - Starting Streaming  audio switch target : Seq " << audioplaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber);
                                Playlist::StartStreamingAsync(
                                    audioplaylist,
                                    audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber, true, false, true);
                            }
                        }
                        else
                        {
                            if (audioswitch->BufferBuildStarted == false)
                            {
                                LOG("CheckAndBufferIfNeeded - Starting Streaming audio switch target : Seq " << audioplaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber);
                                if (videoswitch != nullptr)
                                    videoswitch->BufferBuildStarted = true;
                                audioswitch->BufferBuildStarted = true;
                                Playlist::StartStreamingAsync(
                                    audioplaylist,
                                    audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber, true, false, true);

                            }
                        }
                    }
                }
            }


        }
    }
    catch (HRESULT h)
    {
        return h;
    }
    catch (task_canceled tc)
    {
        return S_OK;
    }
    catch (...)
    {
        return E_FAIL;
    }

    return S_OK;

}

HRESULT Playlist::BulkFetch(shared_ptr<MediaSegment> StartSeg, unsigned int prefetchcount, MFRATE_DIRECTION direction)
{

    std::vector<task<std::tuple<HRESULT, unsigned int>>> vecTasks;
    std::vector<shared_ptr<MediaSegment>> vecSegs;
    unsigned int seq = StartSeg->SequenceNumber;

    vecTasks.push_back(Playlist::StartStreamingAsync(this, seq));
    vecSegs.push_back(GetSegment(seq));

    for (unsigned int i = 1; i < prefetchcount; i++)
    {

        if (direction == MFRATE_FORWARD)
        {
            if (Segments.back()->SequenceNumber != seq)
            {
                seq = GetNextSegment(seq, MFRATE_DIRECTION::MFRATE_FORWARD)->SequenceNumber;
            }
            else
                break;
        }
        else
        {
            if (Segments.front()->SequenceNumber != seq)
            {
                seq = GetNextSegment(seq, MFRATE_DIRECTION::MFRATE_REVERSE)->SequenceNumber;
            }
            else
                break;
        }

        vecTasks.push_back(Playlist::StartStreamingAsync(this, seq));
        vecSegs.push_back(GetSegment(seq));
    }

    auto taskResults = when_all(begin(vecTasks), end(vecTasks)).get();
    if (std::find_if(begin(taskResults), end(taskResults), [](std::tuple<HRESULT, unsigned int> result)
    {
        return FAILED(std::get<0>(result));

    }) != taskResults.end())
        return E_FAIL;
    else
    {
        for (auto seg : vecSegs)
            seg->SetPTSBoundaries();
    }


    return S_OK;
}

///<summary>Starts downloading segments of a stream. Can perform both in a chained (keep downloading until buffer requirements are met) or non-chained(just download the segment requested) mode.</summary>
///<param name='pPlaylist'>The playlist that needs to be streamed</param>
///<param name='SequenceNumber'>The segment sequence number to start streaming at</param>
///<param name='Chained'>If true streaming will continue until LAB threshold is met or exceeded, if not it stops after one segment download</param>
///<param name='WaitNotifyActualDownloadOrEndOfSegments'>Notify on actual download. If set to false, the method returns success if it encounters an already downloaded segment. If set to true it waits to return until an actual segment download is finished.</param>
///<param name='Notifier'>TCE used to signal completion </param>
///<returns>Returns a tuple containing HRESULT indicating success or failure and the sequence number of the segment associated with the download.</returns>

task<tuple<HRESULT, unsigned int>> Playlist::StartStreamingAsync(
    Playlist *pPlaylist,
    unsigned int SequenceNumber,
    bool Chained,
    bool WaitNotifyActualDownloadOrEndOfSegments,
    bool ForceNonActive,
    task_completion_event<tuple<HRESULT, unsigned int>> Notifier,
    task_completion_event<tuple<HRESULT, unsigned int>> AltAudioNotifier,
    task_completion_event<tuple<HRESULT, unsigned int>> AltVideoNotifier
    )
{
    MediaSegmentState cursegstate;
    task<tuple<HRESULT, unsigned int>> retval = task<tuple<HRESULT, unsigned int>>(Notifier);
    bool MainSegmentProcessed = false;
    unsigned int NextSegSeqNum;
    bool Inactive = false;
    shared_ptr<MediaSegment> targetSeg = nullptr;

    /* LOGIIF(pPlaylist->pParentStream != nullptr,
       "StartStreamingAsync Params : Sequence " << SequenceNumber << ",Chained " << (Chained ? L"True" : L"False") << ", WaitNotify " << (WaitNotifyActualDownloadOrEndOfSegments ? L"True" : L"False") << ", ForceNonActive " << (ForceNonActive ? L"True" : L"False") << ",Bitrate = " << pPlaylist->pParentStream->Bandwidth,
       "StartStreamingAsync Params : Sequence " << SequenceNumber << ",Chained " << (Chained ? L"True" : L"False") << ", WaitNotify " << (WaitNotifyActualDownloadOrEndOfSegments ? L"True" : L"False") << ", ForceNonActive " << (ForceNonActive ? L"True" : L"False"));
       */

    {
        if (pPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR || pPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
        {
            Notifier.set(tuple<HRESULT, unsigned int>(E_FAIL, SequenceNumber));
            return task<tuple<HRESULT, unsigned int>>(Notifier);
        }

        {
            std::unique_lock<std::recursive_mutex> listlock(pPlaylist->LockSegmentList, std::defer_lock);
            if (pPlaylist->IsLive) listlock.lock();

            auto itrtargetSeg = std::find_if(pPlaylist->Segments.begin(), pPlaylist->Segments.end(), [SequenceNumber, Chained](shared_ptr<MediaSegment> seg)
            {
                return Chained ? seg->SequenceNumber >= SequenceNumber : seg->SequenceNumber == SequenceNumber; //in case of Live the sequence numbering might change in between two downloads - so if chained check for next greatest sequence number and start there
            });
            if (itrtargetSeg == pPlaylist->Segments.end())
            {
                Notifier.set(tuple<HRESULT, unsigned int>(E_INVALIDARG, SequenceNumber));
                return task<tuple<HRESULT, unsigned int>>(Notifier);
            }
            else
                targetSeg = *itrtargetSeg;
        }
    }


    Inactive = (pPlaylist->pParentRendition != nullptr && !pPlaylist->pParentRendition->IsActive && !ForceNonActive) ||
        (pPlaylist->pParentStream != nullptr && !pPlaylist->pParentStream->IsActive && !ForceNonActive);

    auto NextSeg = pPlaylist->GetNextSegment(SequenceNumber, pPlaylist->cpMediaSource->GetCurrentDirection());
    if (NextSeg == nullptr)
        Chained = false;
    else
        NextSegSeqNum = NextSeg->SequenceNumber;

    //check to see if this is a runaway download after bitrates have switched
    if (Inactive)
    {
        //we are done
        Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
        return task<tuple<HRESULT, unsigned int>>(Notifier);
    }

    cancellation_token_source toksrc;
    auto tkn = toksrc.get_token();


    {
        std::lock_guard<recursive_mutex> seggurard(targetSeg->LockSegment);
        //get the current state for the segment
        cursegstate = targetSeg->GetCurrentState();
    }

    //if it is already downloaded
    if (cursegstate == MediaSegmentState::INMEMORYCACHE)
    {
        //LOG("StartStreamingAsync() : Seq " << targetSeg->SequenceNumber << " = INMEMORY");
        //and we are not chaining
        if (!Chained)
        {
            //we are done
            //LOG("Not Chaining - not waiting")
            Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
            retval = task<tuple<HRESULT, unsigned int>>(Notifier);

        }
        else //we are chaining
        {

            // if caller requested us to hold notification until an actual segment is downloaded
            if (WaitNotifyActualDownloadOrEndOfSegments)
            {
                //LOG("Chaining to " << NextSegSeqNum << " and waiting...")
                //move on to downloading the next segment recursively
                retval = Playlist::StartStreamingAsync(pPlaylist, NextSegSeqNum, Chained, false, ForceNonActive, Notifier);
            }
            else
            {
                //LOG("Chaining to " << NextSegSeqNum << " but not waiting...")
                //notify now - but before that start downloading the next segment 
                Playlist::StartStreamingAsync(pPlaylist, NextSegSeqNum, Chained, false, ForceNonActive);
                Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                retval = task<tuple<HRESULT, unsigned int>>(Notifier);

            }

        }

        if (pPlaylist->cpMediaSource->IsBuffering())
        {
            //end media source buffering
            pPlaylist->cpMediaSource->EndBuffering();
        }
    }
    //the segment requested is already downloading
    else if (cursegstate == MediaSegmentState::DOWNLOADING)
    {
        //LOG("StartStreamingAsync() : Seq " << targetSeg->SequenceNumber << " = DOWNLOADING");
        //if we are NOT chaining
        if (!Chained)
        {
            //if caller wants to wait for an actual download
            if (WaitNotifyActualDownloadOrEndOfSegments)
            {
                //LOG("Not Chaining - waiting on " << targetSeg->GetSequenceNumber());
                //return task waiting on this segment's processing completion TCE
                retval = task<tuple<HRESULT, unsigned int>>(targetSeg->tceSegmentProcessingCompleted);
            }
            else
            {
                //LOG("Not Chaining - not waiting ");
                //return now
                Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                retval = task<tuple<HRESULT, unsigned int>>(Notifier);
            }
        }
        else //we are chaining
        {
            short chainassoc = 0;
            //and this one is not currently chained
            {
                std::lock_guard<recursive_mutex> seggurard(targetSeg->LockSegment);
                //get the current state for the segment
                chainassoc = targetSeg->GetChainAssociation();
            }
            if (chainassoc < 1)
            {
                targetSeg->AssociateChain();
                Playlist::StartStreamingAsync(pPlaylist, NextSegSeqNum, Chained, false, ForceNonActive);
            }

            //if caller wants to wait for actual download
            if (WaitNotifyActualDownloadOrEndOfSegments)
            {
                //LOG("Chaining to " << NextSegSeqNum << " and waiting...");
                //return task waiting on this segment's processing completion TCE
                retval = task<tuple<HRESULT, unsigned int>>(targetSeg->tceSegmentProcessingCompleted);
            }
            else
            {
                // LOG("Chaining to " << NextSegSeqNum << " but not waiting...");
                Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                retval = task<tuple<HRESULT, unsigned int>>(Notifier);
            }

        }
    }

    if (pPlaylist->pParentRendition == nullptr && (cursegstate == INMEMORYCACHE || cursegstate == DOWNLOADING)) //this is the main track
    {

        std::lock_guard<recursive_mutex> seggurard(targetSeg->LockSegment);

        if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition() != nullptr)//and we are playng alternate audio
        {


            //change retval appropriately 
            //    retval = task<tuple<HRESULT,unsigned int>>(targetSeg->tceSegmentProcessingCompleted);
            shared_ptr<MediaSegment> altseg = nullptr;
            bool EndsBeforeMain = false;
            {
                std::lock_guard<recursive_mutex> mergelock(pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->LockMerge);
                altseg = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->GetSegment(
                    pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->FindAltRenditionMatchingSegment(pPlaylist, SequenceNumber, EndsBeforeMain));
            }

            //auto altseg = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->GetSegment(SequenceNumber);
            if (altseg != nullptr)
            {
                auto alternateSegmentState = altseg->GetCurrentState();
                if (alternateSegmentState == DOWNLOADING)
                {
                    //combine the wait state (if any) of the main track segment with the alternate one
                    vector<task<tuple<HRESULT, unsigned int>>> vectasks;

                    vectasks.push_back(retval);
                    vectasks.push_back(task<tuple<HRESULT, unsigned int>>(altseg->tceSegmentProcessingCompleted));
                    retval = when_all(vectasks.begin(), vectasks.end()).then([](vector<tuple<HRESULT, unsigned int>> res) { return res.front(); });

                    if (EndsBeforeMain)
                    {
                        auto nextaltseg = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->GetNextSegment(altseg->GetSequenceNumber(), MFRATE_DIRECTION::MFRATE_FORWARD);
                        if (nextaltseg != nullptr)
                            StartStreamingAsync(pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), nextaltseg->GetSequenceNumber(), true, false, true);
                    }
                }
                else if (alternateSegmentState != INMEMORYCACHE)
                {
                    //combine the wait state (if any) of the main track segment with the alternate one
                    vector<task<tuple<HRESULT, unsigned int>>> vectasks;
                    vectasks.push_back(retval);
                    altseg->tceSegmentProcessingCompleted = AltAudioNotifier;
                    vectasks.push_back(task<tuple<HRESULT, unsigned int>>(altseg->tceSegmentProcessingCompleted));
                    retval = when_all(vectasks.begin(), vectasks.end()).then([](vector<tuple<HRESULT, unsigned int>> res) { return res.front(); });

                    //LOG("Downloading alternate audio seq " << altseg->SequenceNumber << "...");
                    altseg->DownloadSegmentDataAsync().then([pPlaylist, SequenceNumber, altseg, EndsBeforeMain](HRESULT hr)
                    {
                        LOG("Downloaded alternate audio seq " << SequenceNumber);
                        if (EndsBeforeMain)
                        {
                            auto nextaltseg = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist->GetNextSegment(altseg->GetSequenceNumber(), MFRATE_DIRECTION::MFRATE_FORWARD);
                            if (nextaltseg != nullptr)
                                StartStreamingAsync(pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist.get(), nextaltseg->GetSequenceNumber(), true, false, true);
                        }
                        altseg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(hr, SequenceNumber));
                        return tuple<HRESULT, unsigned int>(hr, SequenceNumber);
                    });
                }
            }
        }



    }

    if (cursegstate == INMEMORYCACHE || cursegstate == DOWNLOADING)
        return retval;

    {
        std::lock_guard<recursive_mutex> seggurard(targetSeg->LockSegment);

        //save the supplied notifier as the processing completion TCE
        targetSeg->tceSegmentProcessingCompleted = Notifier;
        //set buffering flag
        pPlaylist->IsBuffering = true;

        //ok we are about to download the segment - lets check to see if the segment will need decryption, and if so download the key if it has not already been downloaded
        if (targetSeg->EncKey != nullptr && targetSeg->EncKey->cpCryptoKey == nullptr && targetSeg->EncKey->Method != NOENCRYPTION)
        {

            //possible scenarios:
            //a) we have a cached key that has a matching URI and a matching IV - set the key to the cached key
            //b)we have a cached key that has a matching URI but the IV does not match - copy the key data from the cached key but use the IV in the new key
            //c) we do not have a cached key or we do have one with a different URI - download key

            if (pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey != nullptr && pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey->cpCryptoKey != nullptr)
            {
                if (targetSeg->EncKey->IsEqual(pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey))//case a)
                    targetSeg->EncKey = pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey;
                else if (targetSeg->EncKey->IsEqualKeyOnly(pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey)) //case b)
                    targetSeg->EncKey->cpCryptoKey = pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey->cpCryptoKey;
            }

            if (targetSeg->EncKey->cpCryptoKey == nullptr) //still null - download
            {
                if (FAILED(targetSeg->EncKey->DownloadKeyAsync().get()))
                {
                    if (pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey != nullptr) //use cached key - if decryption fails we will fail playback later
                        targetSeg->EncKey = pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey;
                    else //fail
                    {
                        pPlaylist->cpMediaSource->NotifyError(E_FAIL);
                        Notifier.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                        return task<tuple<HRESULT, unsigned int>>(Notifier);
                    }
                }
                else //cache key
                {
                    pPlaylist->cpMediaSource->spRootPlaylist->LastCachedKey = targetSeg->EncKey;
                }
            }

        }
        //start download
        if (Chained) targetSeg->AssociateChain();
    }

    auto  t = targetSeg->DownloadSegmentDataAsync().
        then([pPlaylist, SequenceNumber, NextSegSeqNum, Chained, WaitNotifyActualDownloadOrEndOfSegments, Notifier, ForceNonActive](task<HRESULT> hrt) -> task < tuple<HRESULT, unsigned int> >
    {


        shared_ptr<MediaSegment> targetSeg = nullptr;
        targetSeg = pPlaylist->GetSegment(SequenceNumber);


        try
        {

            //CHKTASK();
            HRESULT hr = hrt.get();

            unsigned int NextDownloadTargetSequence = NextSegSeqNum;

            if (pPlaylist->cpMediaSource->GetCurrentState() == MSS_ERROR || pPlaylist->cpMediaSource->GetCurrentState() == MSS_UNINITIALIZED)
            {
                Notifier.set(tuple<HRESULT, unsigned int>(E_FAIL, SequenceNumber));
                return task<tuple<HRESULT, unsigned int>>(Notifier);
            }


            targetSeg->ResetChainAssociation();

            if (targetSeg == nullptr)
            {
                Notifier.set(tuple<HRESULT, unsigned int>(E_INVALIDARG, SequenceNumber));
                return task<tuple<HRESULT, unsigned int>>(Notifier);
            }


            //dowload failed - notify
            if (FAILED(hr))
            {
                targetSeg->SetCurrentState(UNAVAILABLE);
                //LOG("Failed Download : Segment(seq=" << SequenceNumber << ",loc=" << targetSeg->MediaUri << ",speed=" << (targetSeg->pParentPlaylist->pParentStream != nullptr ? targetSeg->pParentPlaylist->pParentStream->Bandwidth : 0) << ")");
                Notifier.set(tuple<HRESULT, unsigned int>(hr, SequenceNumber));
                return task<tuple<HRESULT, unsigned int>>(Notifier);
            }
            else //download succeeded
            {

                {

                    std::lock_guard<std::recursive_mutex> lock(pPlaylist->LockSegmentTracking);
                    //if this is the very first segment being downloaded on the playlist - initialize the segment index trackers
                    //we always initialize the index trackers for both content types - regardless of if we have that content type in the current stream
                    if (pPlaylist->CurrentSegmentTracker.find(VIDEO) == pPlaylist->CurrentSegmentTracker.end())
                        pPlaylist->CurrentSegmentTracker.emplace(std::pair<ContentType, shared_ptr<MediaSegment>>(ContentType::VIDEO, targetSeg));
                    if (pPlaylist->CurrentSegmentTracker.find(AUDIO) == pPlaylist->CurrentSegmentTracker.end())
                        pPlaylist->CurrentSegmentTracker.emplace(std::pair<ContentType, shared_ptr<MediaSegment>>(ContentType::AUDIO, targetSeg));
                }

                //CHKTASK();
                if (pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->GetActiveAudioRendition() != nullptr && pPlaylist->cpMediaSource->GetCurrentState() != MSS_OPENING)//we are playng alternate audio
                {

                    auto altpl = pPlaylist->pParentStream->GetActiveAudioRendition()->spPlaylist;
                    std::lock_guard<std::recursive_mutex> lock(altpl->LockSegmentTracking);

                    auto altseg = altpl->GetSegment(SequenceNumber);
                    if (altseg != nullptr)
                    {
                        auto alternateSegmentState = altseg->GetCurrentState();
                        if (alternateSegmentState != DOWNLOADING && alternateSegmentState != INMEMORYCACHE)
                        {
                            if (FAILED(hr = altseg->DownloadSegmentDataAsync().get()))
                            {
                                //LOG("Failed Download : Alternate Rendition Segment(seq=" << altseg->SequenceNumber << ",loc=" << altseg->MediaUri);
                                targetSeg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(hr, SequenceNumber));
                                return task<tuple<HRESULT, unsigned int>>(Notifier);
                            }
                        }
                        if (altpl->CurrentSegmentTracker.find(AUDIO) == altpl->CurrentSegmentTracker.end())
                            altpl->CurrentSegmentTracker.emplace(std::pair<ContentType, shared_ptr<MediaSegment>>(ContentType::AUDIO, altseg));
                    }
                }
                //CHKTASK();
                unsigned int LastSegSeq = 0;

                LastSegSeq = pPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_DIRECTION::MFRATE_FORWARD ?
                    pPlaylist->Segments.back()->SequenceNumber : pPlaylist->Segments.front()->SequenceNumber;

                //only do the next check if this is the main track playlist
                if (pPlaylist->pParentRendition == nullptr && (pPlaylist->HasCurrentSegmentTracker(VIDEO) || pPlaylist->HasCurrentSegmentTracker(AUDIO)))
                {

                    auto MaxVal = pPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_DIRECTION::MFRATE_FORWARD ? pPlaylist->MaxCurrentSegment()->SequenceNumber : pPlaylist->MinCurrentSegment()->SequenceNumber;
                    //get the LAB state after this download
                    /*LOGIF(targetSeg->pParentPlaylist->pParentStream != nullptr, "StartStreamingAsync()::Checking for LAB on download(seq=" << MaxVal << ",loc=" << targetSeg->MediaUri << ",speed=" << targetSeg->pParentPlaylist->pParentStream->Bandwidth << ")");
                    LOGIF(targetSeg->pParentPlaylist->pParentStream == nullptr, "StartStreamingAsync()::Checking for LAB on download(seq=" << MaxVal << ",loc=" << targetSeg->MediaUri << ")");*/
                    unsigned long long LABLength = pPlaylist->GetCurrentLABLength(MaxVal, true);

                    if (pPlaylist->PauseBufferBuilding && LABLength < pPlaylist->DerivedTargetDuration * 2)
                        pPlaylist->PauseBufferBuilding = false;

                    if ((LABLength > 0 || (SequenceNumber == LastSegSeq)) && pPlaylist->cpMediaSource->IsBuffering())
                    {
                        //end media source buffering
                        pPlaylist->cpMediaSource->EndBuffering();
                    }
                    //if LAB is above threshold or this was the last segment or we are at EOS or we are not chained
                    if (LABLength >= Configuration::GetCurrent()->GetRateAdjustedLABThreshold(pPlaylist->cpMediaSource->GetCurrentPlaybackRate() != nullptr ? pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate : 1)
                        || SequenceNumber == LastSegSeq || Chained == false)
                    {
                        //LOGIF(targetSeg->pParentPlaylist->pParentStream != nullptr, "StartStreamingAsync()::Stopping downloading after seq " << SequenceNumber << ",speed=" << targetSeg->pParentPlaylist->pParentStream->Bandwidth << " Chained = " << (Chained ? L"TRUE" : L"FALSE") << ")");
                        //stop downloading
                        pPlaylist->IsBuffering = false;
                    }
                    else
                    {

                        //if this download succeeded
                        if (targetSeg->GetCurrentState() == INMEMORYCACHE && pPlaylist->pParentStream != nullptr && pPlaylist->pParentStream->IsActive &&
                            ((pPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD && NextDownloadTargetSequence >= SequenceNumber) ||
                                (pPlaylist->cpMediaSource->GetCurrentDirection() == MFRATE_REVERSE && NextDownloadTargetSequence <= SequenceNumber)) &&
                            pPlaylist->cpMediaSource->GetCurrentPlaybackRate()->Rate != 0.0)
                        {

                            if (std::try_lock(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, pPlaylist->cpMediaSource->cpAudioStream->LockSwitch) < 0)
                            {
                                std::lock_guard<std::recursive_mutex> lockv(pPlaylist->cpMediaSource->cpVideoStream->LockSwitch, adopt_lock);
                                std::lock_guard<std::recursive_mutex> locka(pPlaylist->cpMediaSource->cpAudioStream->LockSwitch, adopt_lock);

                                auto videoswitch = pPlaylist->cpMediaSource->cpVideoStream->GetPendingBitrateSwitch();
                                auto audioswitch = pPlaylist->cpMediaSource->cpAudioStream->GetPendingBitrateSwitch();

                                auto videoplaylist = videoswitch != nullptr ? videoswitch->targetPlaylist : nullptr;
                                auto audioplaylist = audioswitch != nullptr ? audioswitch->targetPlaylist : nullptr;
                                //stop buffering on source playlist if switch is already requested
                                if (
                                    (
                                        (videoswitch != nullptr && videoswitch->VideoSwitchedTo == nullptr && videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO) != nullptr && NextDownloadTargetSequence >= videoswitch->targetPlaylist->GetCurrentSegmentTracker(VIDEO)->SequenceNumber) ||
                                        (audioswitch != nullptr && audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO) != nullptr && NextDownloadTargetSequence >= audioswitch->targetPlaylist->GetCurrentSegmentTracker(AUDIO)->SequenceNumber)
                                        )
                                    &&
                                    (
                                        (videoplaylist != nullptr && pPlaylist->pParentStream != nullptr && videoplaylist->pParentStream->Bandwidth != pPlaylist->pParentStream->Bandwidth) ||
                                        (audioplaylist != nullptr && pPlaylist->pParentStream != nullptr && audioplaylist->pParentStream->Bandwidth != pPlaylist->pParentStream->Bandwidth)
                                        )
                                    )
                                {
                                    // LOGIF(targetSeg->pParentPlaylist->pParentStream != nullptr, "StartStreamingAsync()::Stopping downloading because switch pending after seq " << SequenceNumber << ",speed=" << targetSeg->pParentPlaylist->pParentStream->Bandwidth << " Chained = " << (Chained ? L"TRUE" : L"FALSE") << ")");
                                    targetSeg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                                    return task<tuple<HRESULT, unsigned int>>(Notifier);

                                }
                            }

                            if (pPlaylist->PauseBufferBuilding) //stop building buffer
                            {
                                // LOGIF(targetSeg->pParentPlaylist->pParentStream != nullptr, "StartStreamingAsync()::Stopping downloading because buffering is paused after seq " << SequenceNumber << ",speed=" << targetSeg->pParentPlaylist->pParentStream->Bandwidth << " Chained = " << (Chained ? L"TRUE" : L"FALSE") << ")");
                                targetSeg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                                return task<tuple<HRESULT, unsigned int>>(Notifier);
                            }

                            //  LOGIF(targetSeg->pParentPlaylist->pParentStream != nullptr, "StartStreamingAsync()::Continuing download to seq " << NextDownloadTargetSequence << ",speed=" << targetSeg->pParentPlaylist->pParentStream->Bandwidth << " Chained = " << (Chained ? L"TRUE" : L"FALSE") << ", ForceNonActive = " << (ForceNonActive ? L"TRUE" : L"FALSE") << ")");
                            Playlist::StartStreamingAsync(pPlaylist, NextDownloadTargetSequence, Chained, false, ForceNonActive); //continue buffering 
                        }

                    }

                }
                else if (pPlaylist->pParentRendition != nullptr && targetSeg->CumulativeDuration < pPlaylist->cpMediaSource->spRootPlaylist->ActiveVariant->spPlaylist->MaxCurrentSegment()->CumulativeDuration)
                {
                    //CHKTASK();
                    Playlist::StartStreamingAsync(pPlaylist, NextSegSeqNum, Chained, false, ForceNonActive); //continue streaming
                }
                //return
                targetSeg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(S_OK, SequenceNumber));
                return task<tuple<HRESULT, unsigned int>>(Notifier);
            }

        }
        catch (task_canceled)
        {
            targetSeg->Scavenge(true);
            targetSeg->SetCurrentState(UNAVAILABLE);
            if (pPlaylist->cpMediaSource->IsBuffering())
            {
                //end media source buffering
                pPlaylist->cpMediaSource->EndBuffering();
            }
            throw;
        }
        catch (...)
        {
            targetSeg->SetCurrentState(UNAVAILABLE);
            targetSeg->tceSegmentProcessingCompleted.set(tuple<HRESULT, unsigned int>(E_FAIL, SequenceNumber));

            if (pPlaylist->cpMediaSource->IsBuffering())
            {
                //end media source buffering
                pPlaylist->cpMediaSource->EndBuffering();
            }

            return task<tuple<HRESULT, unsigned int>>(Notifier);
        }
    }, tkn, task_continuation_context::use_arbitrary());

    pPlaylist->taskRegistry.Register(t, toksrc);

    return t;

}



shared_ptr<MediaSegment> Playlist::GetBitrateSwitchTarget(Playlist *fromPlaylist, bool IgnoreBuffer)
{
    shared_ptr<MediaSegment> FromMaxCurrentSeg = nullptr;

    unsigned int FromBaseSequenceNum = 0;
    unsigned int FromLastSegSequenceNum = 0;

    {

        std::lock_guard<std::recursive_mutex> slock(fromPlaylist->LockSegmentTracking);
        if (fromPlaylist->pParentStream->Bandwidth > pParentStream->Bandwidth && !IgnoreBuffer && !IsLive)//shifting down
            FromMaxCurrentSeg = fromPlaylist->MaxBufferedSegment();
        else //shifting up
            FromMaxCurrentSeg = fromPlaylist->MaxCurrentSegment();
        FromBaseSequenceNum = fromPlaylist->BaseSequenceNumber;
        FromLastSegSequenceNum = fromPlaylist->Segments.back()->SequenceNumber;
    }

    shared_ptr<MediaSegment> targetSeg = nullptr;

    if (Configuration::GetCurrent()->MatchSegmentsUsing == Microsoft::HLSClient::SegmentMatchCriterion::PROGRAMDATETIME
        && FromMaxCurrentSeg->ProgramDateTime != nullptr)
    {
        std::lock_guard<std::recursive_mutex> slock(LockSegmentList);

        auto match = std::find_if(Segments.begin(), Segments.end(), [this, FromMaxCurrentSeg](shared_ptr<MediaSegment> ms)
        {
            return ms->ProgramDateTime->ValueInTicks > FromMaxCurrentSeg->ProgramDateTime->ValueInTicks;
        });

        if (match != Segments.end() &&
            abs((long long)((*match)->ProgramDateTime->ValueInTicks - FromMaxCurrentSeg->ProgramDateTime->ValueInTicks)) < (long long)(DerivedTargetDuration * 2)) // too far apart
            targetSeg = *match;

    }
    else //fallback or sequence number match
    {
        if (this->IsLive == false)//VOD - we try to match using cumulative duration
        {
            auto match = std::find_if(Segments.begin(), Segments.end(), [this, FromMaxCurrentSeg](shared_ptr<MediaSegment> ms)
            {
                return ms->CumulativeDuration > FromMaxCurrentSeg->CumulativeDuration;
            });
            if (match != Segments.end() && (*match)->CumulativeDuration - FromMaxCurrentSeg->CumulativeDuration >= (DerivedTargetDuration / 2)) //at least a half segment diff - otherwise we are not likely to find a key frame
                targetSeg = *match;
        }

        if (targetSeg == nullptr) //live or cumulative duration based search did not work for some reason- just use sequence number
        {
            if (fromPlaylist->IsLive == false)
            {
                targetSeg = GetNextSegment(FromMaxCurrentSeg->SequenceNumber, MFRATE_FORWARD);
            }
            else
            {
                targetSeg = GetNextSegment(FromMaxCurrentSeg->SequenceNumber, MFRATE_FORWARD);
                if (targetSeg == nullptr && this->BaseSequenceNumber > fromPlaylist->BaseSequenceNumber)
                {
                    targetSeg = GetSegment(FromMaxCurrentSeg->SequenceNumber + (this->BaseSequenceNumber - fromPlaylist->BaseSequenceNumber));
                }
            }
        }
    }

    return targetSeg;
}

shared_ptr<MediaSegment> Playlist::GetBitrateSwitchTarget(Playlist *fromPlaylist, MediaSegment *fromSegment, bool IgnoreBuffer)
{
    unsigned int FromBaseSequenceNum = 0;
    unsigned int FromLastSegSequenceNum = 0;

    {

        std::lock_guard<std::recursive_mutex> slock(fromPlaylist->LockSegmentTracking);

        FromBaseSequenceNum = fromPlaylist->BaseSequenceNumber;
        FromLastSegSequenceNum = fromPlaylist->Segments.back()->SequenceNumber;
    }

    shared_ptr<MediaSegment> targetSeg = nullptr;

    MediaSegment * FromMaxCurrentSeg = nullptr;
    if (fromPlaylist->pParentStream->Bandwidth > pParentStream->Bandwidth && !IgnoreBuffer && !IsLive)//shifting down
        FromMaxCurrentSeg = fromPlaylist->MaxBufferedSegment().get();
    else //shifting up
        FromMaxCurrentSeg = fromSegment;

    if (Configuration::GetCurrent()->MatchSegmentsUsing == Microsoft::HLSClient::SegmentMatchCriterion::PROGRAMDATETIME && FromMaxCurrentSeg->ProgramDateTime != nullptr)
    {
        std::lock_guard<std::recursive_mutex> slock(LockSegmentList);
        //find the target segment that has the closest PDT to this one and return the segment following that
        auto itr = std::partition(Segments.begin(), Segments.end(), [this, FromMaxCurrentSeg](shared_ptr<MediaSegment> ms)
        {
            return ms->ProgramDateTime->ValueInTicks <= FromMaxCurrentSeg->ProgramDateTime->ValueInTicks;
        });
        if (itr == Segments.end())
            targetSeg = nullptr;
        else if (itr == Segments.begin())
        {
            if (Segments.size() > 1)
                targetSeg = GetNextSegment((*itr)->SequenceNumber, MFRATE_FORWARD);
            else
            {
                targetSeg = nullptr;
            }
        }
        else
        {
            auto diff1 = abs((long long)(FromMaxCurrentSeg->ProgramDateTime->ValueInTicks - (*(itr - 1))->ProgramDateTime->ValueInTicks));
            auto diff2 = abs((long long)(FromMaxCurrentSeg->ProgramDateTime->ValueInTicks - (*itr)->ProgramDateTime->ValueInTicks));

            if (diff1 > diff2)
                targetSeg = GetNextSegment((*itr)->SequenceNumber, MFRATE_FORWARD);
            else
                targetSeg = GetNextSegment((*(itr - 1))->SequenceNumber, MFRATE_FORWARD);
        }

        if (abs((long long)(targetSeg->ProgramDateTime->ValueInTicks - FromMaxCurrentSeg->ProgramDateTime->ValueInTicks)) >= (long long)this->DerivedTargetDuration * 2) //too far apart 
            targetSeg = nullptr;
    }
    else
    {
        if (this->IsLive == false)//VOD - we try to match using cumulative duration
        {
            auto match = std::find_if(Segments.begin(), Segments.end(), [this, FromMaxCurrentSeg](shared_ptr<MediaSegment> ms)
            {
                return ms->CumulativeDuration > FromMaxCurrentSeg->CumulativeDuration;
            });
            if (match != Segments.end() && (*match)->CumulativeDuration - FromMaxCurrentSeg->CumulativeDuration >= (DerivedTargetDuration / 2)) //at least a half segment diff - otherwise we are not likely to find a key frame
                targetSeg = *match;
        }
        if (targetSeg == nullptr)
        {
            if (fromPlaylist->IsLive == false)
            {
                targetSeg = GetNextSegment(FromMaxCurrentSeg->SequenceNumber, MFRATE_FORWARD);
            }
            else
            {
                targetSeg = GetNextSegment(FromMaxCurrentSeg->SequenceNumber, MFRATE_FORWARD);
                if (targetSeg == nullptr && this->BaseSequenceNumber > fromPlaylist->BaseSequenceNumber)
                {
                    targetSeg = GetSegment(FromMaxCurrentSeg->SequenceNumber + (this->BaseSequenceNumber - fromPlaylist->BaseSequenceNumber));
                }
            }
        }

    }
    return targetSeg;
}

///<summary>Prepares a playlist for a pending bitrate transfer</summary>
///<param name='pTarget'>Target playlist to transfer to</param>
///<returns>Indicates if a switch s possible</returns>
bool Playlist::PrepareBitrateTransfer(Playlist *pTarget, bool IgnoreBuffer)
{


    shared_ptr<MediaSegment> targetSeg = nullptr;

    if (IsLive)
    {
        pTarget->SetCurrentSegmentTracker(VIDEO, nullptr);
        pTarget->SetCurrentSegmentTracker(AUDIO, nullptr);
        //ensure latest version of the playlist is downloaded and merged
        if (FAILED(pTarget->pParentStream->DownloadPlaylistAsync().get())) return false;

        pTarget->MergeLivePlaylistChild();
    }


    pTarget->SetLABThreshold();


    targetSeg = pTarget->GetBitrateSwitchTarget(this, IgnoreBuffer);

    if (targetSeg == nullptr)
        return false;

    //last segment - no need to switch
    if (!IsLive && (cpMediaSource->GetCurrentDirection() == MFRATE_FORWARD && targetSeg->SequenceNumber == Segments.back()->SequenceNumber) ||
        (cpMediaSource->GetCurrentDirection() == MFRATE_REVERSE && targetSeg->SequenceNumber == Segments.front()->SequenceNumber))
    {
        return false;
    }

    if (!IsLive)
        pTarget->StartPTSOriginal = StartPTSOriginal;

    if (targetSeg != nullptr)
    {

        pTarget->SetCurrentSegmentTracker(VIDEO, targetSeg);
        pTarget->SetCurrentSegmentTracker(AUDIO, targetSeg);
    }

    if (targetSeg != nullptr)
    {


        if (targetSeg->GetCurrentState() == INMEMORYCACHE)
            targetSeg->SetPTSBoundaries();



        Playlist::StartStreamingAsync(pTarget, targetSeg->SequenceNumber, false, false, true);

    }


    shared_ptr<MediaSegment> MaxSeg = nullptr;

    MaxSeg = MaxCurrentSegment();
    LOGIF(MaxSeg != nullptr && targetSeg != nullptr, "Playlist::PrepareBitrateTransfer() : Bitrate switch prep from " << pParentStream->Bandwidth << " bps to " << pTarget->pParentStream->Bandwidth << " bps from sequence " << MaxSeg->SequenceNumber << "(base = " << this->BaseSequenceNumber << ") to " << targetSeg->SequenceNumber << "(base = " << pTarget->BaseSequenceNumber << ")");

    return true;
}

#pragma endregion

HRESULT Playlist::AddToBatch(std::wstring Uri)
{
    std::shared_ptr<Playlist> spPlaylist = nullptr;
    if (FAILED(Playlist::DownloadPlaylistAsync(this->cpMediaSource, Uri, spPlaylist).get()))
    {
        this->cpMediaSource->NotifyError(E_INVALIDARG);
        return E_INVALIDARG;
    }

    spPlaylist->Parse();
    if ((this->IsVariant && !spPlaylist->IsVariant) || (!this->IsVariant && spPlaylist->IsVariant))
    {
        this->cpMediaSource->NotifyError(E_INVALIDARG);
        return E_INVALIDARG;
    }

    if (this->IsVariant && Variants.size() != spPlaylist->Variants.size())
    {
        this->cpMediaSource->NotifyError(E_INVALIDARG);
        return E_INVALIDARG;

    }

    if (!CombinePlaylistBatch(spPlaylist))
    {
        this->cpMediaSource->NotifyError(E_INVALIDARG);
        return E_INVALIDARG;
    }

    return S_OK;
}

bool Playlist::CombinePlaylistBatch(shared_ptr<Playlist> spPlaylist)
{
    if (this->IsVariant && spPlaylist->IsVariant)
    {
        //master playlist merge
        auto itr = Variants.begin();
        auto itr2 = spPlaylist->Variants.begin();
        for (unsigned int idx = 0; idx < Variants.size(); idx++)
        {
            itr->second->BatchItems.emplace(std::pair<wstring, shared_ptr<Playlist>>(itr2->second->PlaylistUri, shared_ptr<Playlist>(nullptr)));
            itr++;
            itr2++;
        }
    }
    else if (!this->IsVariant && !spPlaylist->IsVariant)
    {
        LOG("Attempting Playlist Batch Merge...");

        try
        {


            std::unique_lock<std::recursive_mutex> listlock(LockSegmentList, std::defer_lock);
            if (IsLive) listlock.lock();
            std::shared_ptr<EncryptionKey> lastEncKey = nullptr;

            if (spPlaylist->Segments.size() > 0)
            {


                for (auto rpitr : spPlaylist->Segments)
                {
                    rpitr->pParentPlaylist = this;

                    if (rpitr->EncKey != nullptr && lastEncKey != nullptr && rpitr->EncKey->IsEqual(lastEncKey) && lastEncKey->cpCryptoKey != nullptr)
                        rpitr->EncKey = lastEncKey;
                    else
                        lastEncKey = rpitr->EncKey;
                    if (rpitr->EncKey != nullptr)
                        rpitr->EncKey->pParentPlaylist = this;


                    if (Segments.size() > 0)
                    {
                        rpitr->CumulativeDuration = Segments.back()->CumulativeDuration + rpitr->Duration;
                        rpitr->SetSequenceNumber(Segments.back()->SequenceNumber + 1);
                    }

                    rpitr->SetPTSBoundaries();
                    rpitr->Discontinous = true;
                    Segments.push_back(rpitr);
                }
                this->TotalDuration += spPlaylist->TotalDuration;
                if (this->cpMediaSource->spRootPlaylist->IsVariant && this->cpMediaSource->GetCurrentState() == MediaSourceState::MSS_OPENING)
                    this->cpMediaSource->spRootPlaylist->TotalDuration += spPlaylist->TotalDuration;

            }
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}
