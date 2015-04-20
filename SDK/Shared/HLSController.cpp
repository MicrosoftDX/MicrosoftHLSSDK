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

#include "HLSMediaSource.h" 
#include "Playlist.h"
#include "HLSResourceRequestEventArgs.h"
#include "HLSInitialBitrateSelectedEventArgs.h"
#include "HLSPlaylist.h" 
#include "HLSController.h"
#include "HLSVariantStream.h"

using namespace std;
using namespace Platform;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

void HLSController::RaisePrepareResourceRequest(ResourceType restype,
  wstring& szUrl,
  std::vector<shared_ptr<Cookie>>& cookies,
  std::map<wstring,
  wstring>& headers,
  IHLSContentDownloader^* externaldownloader)
{
  HLSResourceRequestEventArgs^ args = ref new HLSResourceRequestEventArgs(restype, szUrl, cookies, headers);
  if (args != nullptr && IsValid && IsPrepareResourceRequestSubscribed())
  {
    task<void> t(args->_tceSubmitted);
    PrepareResourceRequest(this, args);
    t.wait();
    szUrl = args->_resourceUrl;
    headers = args->_headers;
    cookies = args->_cookies; 
    *externaldownloader = args->GetDownloader();
  }
}

 

void HLSController::RaiseInitialBitrateSelected()
{

  if (IsValid && this->MediaSource != nullptr &&
    this->MediaSource->spRootPlaylist != nullptr &&
    this->MediaSource->spRootPlaylist->IsVariant &&
    this->MediaSource->spRootPlaylist->ActiveVariant != nullptr &&
    this->MediaSource->GetCurrentState() == MSS_OPENING && IsInitialBitrateSelectedSubscribed())
  {
    HLSInitialBitrateSelectedEventArgs^ args = ref new HLSInitialBitrateSelectedEventArgs();
    task<void> t(args->_tceSubmitted);
    InitialBitrateSelected(this, args);
    t.wait(); 
  }
}

HLSController::HLSController(CHLSMediaSource *psource, wstring controllerID) : 
  _pmediaSource(psource), 
  _id(controllerID), 
  _playlist(nullptr), 
  _initialbitrateselectedsubscriptioncount(0), 
  _prepareresrequestsubscriptioncount(0)
{

}
 
SegmentMatchCriterion HLSController::MatchSegmentsUsing::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->MatchSegmentsUsing;
}
void HLSController::MatchSegmentsUsing::set(SegmentMatchCriterion val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->MatchSegmentsUsing = val;
}
bool HLSController::IsValid::get()
{
  return (this->MediaSource != nullptr && 
    this->MediaSource->spRootPlaylist != nullptr &&
    this->MediaSource->GetCurrentState() != MediaSourceState::MSS_ERROR && 
    this->MediaSource->GetCurrentState() != MediaSourceState::MSS_UNINITIALIZED);
}

String^ HLSController::ID::get()
{
  return ref new String(_id.data());
}

unsigned int HLSController::GetLastMeasuredBandwidth()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (this->MediaSource->spHeuristicsManager != nullptr)
    return this->MediaSource->spHeuristicsManager->GetLastMeasuredBandwidth();
  else
    return 0;
}
Windows::Foundation::TimeSpan HLSController::MinimumBufferLength::get()
{

  if (!IsValid)  throw ref new Platform::ObjectDisposedException();

  return Windows::Foundation::TimeSpan{ (long long) Configuration::GetCurrent()->LABLengthInTicks };
}
void HLSController::MinimumBufferLength::set(Windows::Foundation::TimeSpan val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->SetLABLengthInTicks((unsigned long long)val.Duration);
}

Windows::Foundation::TimeSpan HLSController::PrefetchDuration::get()
{

  if (!IsValid)  throw ref new Platform::ObjectDisposedException();

  return Windows::Foundation::TimeSpan{ (long long) Configuration::GetCurrent()->PreFetchLengthInTicks };
}

void HLSController::PrefetchDuration::set(Windows::Foundation::TimeSpan val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->PreFetchLengthInTicks = (unsigned long long)val.Duration;
}

Windows::Foundation::TimeSpan HLSController::MinimumLiveLatency::get()
{

  if (!IsValid)  throw ref new Platform::ObjectDisposedException();

  return Windows::Foundation::TimeSpan{ (long long)Configuration::GetCurrent()->MinimumLiveLatency };
}

void HLSController::MinimumLiveLatency::set(Windows::Foundation::TimeSpan val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->MinimumLiveLatency = (unsigned long long)val.Duration;
}

 
Windows::Foundation::TimeSpan HLSController::BitrateChangeNotificationInterval::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Windows::Foundation::TimeSpan{ (long long) Configuration::GetCurrent()->BitrateChangeNotificationInterval };
}
void HLSController::BitrateChangeNotificationInterval::set(Windows::Foundation::TimeSpan val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->BitrateChangeNotificationInterval = (unsigned long long)val.Duration;
  //stop and restart notifier if needed
  if (this->MediaSource->spHeuristicsManager != nullptr &&
    this->MediaSource->spHeuristicsManager->IsBitrateChangeNotifierRunning())
  {
    this->MediaSource->spHeuristicsManager->StopNotifier();
    this->MediaSource->spHeuristicsManager->StartNotifier();
  }
}

bool HLSController::EnableAdaptiveBitrateMonitor::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->EnableBitrateMonitor;
}
void HLSController::EnableAdaptiveBitrateMonitor::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->EnableBitrateMonitor = val;
  if (MediaSource->GetCurrentState() == MediaSourceState::MSS_STARTED)
  {
    if (Configuration::GetCurrent()->EnableBitrateMonitor == true)
      MediaSource->spHeuristicsManager->StartNotifier();
    else
      MediaSource->spHeuristicsManager->StopNotifier();
  }
}


TrackType HLSController::TrackTypeFilter::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (Configuration::GetCurrent()->ContentTypeFilter == ContentType::AUDIO)
    return TrackType::AUDIO;
  else if (Configuration::GetCurrent()->ContentTypeFilter == ContentType::VIDEO)
    return TrackType::VIDEO;
  else
    return TrackType::BOTH;
}

void HLSController::TrackTypeFilter::set(TrackType val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  auto state = MediaSource->GetCurrentState();
  if (state != MediaSourceState::MSS_OPENING) return;
  if (val == TrackType::AUDIO)
    Configuration::GetCurrent()->ContentTypeFilter = ContentType::AUDIO; 
  else if (val == TrackType::VIDEO)
    Configuration::GetCurrent()->ContentTypeFilter = ContentType::VIDEO;
  else
    Configuration::GetCurrent()->ContentTypeFilter = ContentType::UNKNOWN;
   
}


bool HLSController::UseTimeAveragedNetworkMeasure::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->UseTimeAveragedNetworkMeasure;
}
void HLSController::UseTimeAveragedNetworkMeasure::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->UseTimeAveragedNetworkMeasure = val;
}

bool HLSController::AllowSegmentSkipOnSegmentFailure::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure;
}
void HLSController::AllowSegmentSkipOnSegmentFailure::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->AllowSegmentSkipOnSegmentFailure = val;
}

bool HLSController::ResumeLiveFromPausedOrEarliest::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->ResumeLiveFromPausedOrEarliest;
}
void HLSController::ResumeLiveFromPausedOrEarliest::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->ResumeLiveFromPausedOrEarliest = val;
}

bool HLSController::UpshiftBitrateInSteps::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->UpshiftBitrateInSteps;
}
void HLSController::UpshiftBitrateInSteps::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->UpshiftBitrateInSteps = val;
}

bool HLSController::ForceKeyFrameMatchOnSeek::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->ForceKeyFrameMatchOnSeek;
}
void HLSController::ForceKeyFrameMatchOnSeek::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->ForceKeyFrameMatchOnSeek = val;
}
 
//bool HLSController::ForceKeyFrameMatchOnBitrateSwitch::get()
//{
//  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
//  return Configuration::GetCurrent()->ForceKeyFrameMatchOnBitrateSwitch;
//}
//void HLSController::ForceKeyFrameMatchOnBitrateSwitch::set(bool val)
//{
//  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
//  Configuration::GetCurrent()->ForceKeyFrameMatchOnBitrateSwitch = val;
//}

bool HLSController::AutoAdjustScrubbingBitrate::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->AutoAdjustScrubbingBitrate;
}
void HLSController::AutoAdjustScrubbingBitrate::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->AutoAdjustScrubbingBitrate = val;
}

bool HLSController::AutoAdjustTrickPlayBitrate::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->AutoAdjustTrickPlayBitrate;
}
void HLSController::AutoAdjustTrickPlayBitrate::set(bool val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->AutoAdjustTrickPlayBitrate = val;
}
 

unsigned int HLSController::SegmentTryLimitOnBitrateSwitch::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch;
}
void HLSController::SegmentTryLimitOnBitrateSwitch::set(unsigned int val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->SegmentTryLimitOnBitrateSwitch = val;
}
 

float HLSController::MinimumPaddingForBitrateUpshift::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift;
}
void HLSController::MinimumPaddingForBitrateUpshift::set(float val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift = val;
}

float HLSController::MaximumToleranceForBitrateDownshift::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  return Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift;
}
void HLSController::MaximumToleranceForBitrateDownshift::set(float val)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift = val;
}


IHLSPlaylist^ HLSController::Playlist::get()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_playlist == nullptr)
    _playlist = ref new HLSPlaylist(this, false, 0);
  return _playlist;
}

HLSPlaylist^ HLSController::GetPlaylist()
{
  if (_playlist == nullptr)
    _playlist = ref new HLSPlaylist(this, false, 0);
  return _playlist;
}

bool HLSController::TryLock()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (MediaSource->spRootPlaylist != nullptr)
  {
    _lockclient = unique_lock<recursive_mutex>(MediaSource->spRootPlaylist->LockClient, try_to_lock);
    if (_lockclient)
      return true;
    else
      return false;
  }
  else
    return false;
}

void HLSController::Lock()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (MediaSource->spRootPlaylist != nullptr)
    _lockclient = unique_lock<recursive_mutex>(MediaSource->spRootPlaylist->LockClient);
}

void HLSController::Unlock()
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (MediaSource->spRootPlaylist != nullptr && _lockclient)
    _lockclient.unlock();
}

void HLSController::BatchPlaylists(Windows::Foundation::Collections::IVector<String^>^ PlaylistUrls)
{
  if (!IsValid)  throw ref new Platform::ObjectDisposedException();
  if (MediaSource == nullptr || MediaSource->spRootPlaylist == nullptr)
    return;

  for (unsigned int i = 0; i < PlaylistUrls->Size; i++) 
    MediaSource->spRootPlaylist->PlaylistBatch.emplace(std::pair<wstring, shared_ptr<Microsoft::HLSClient::Private::Playlist>>(PlaylistUrls->GetAt(i)->Data(), shared_ptr<Microsoft::HLSClient::Private::Playlist>(nullptr)));
}