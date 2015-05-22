// Class1.cpp
#include "pch.h"
#include "BackgroundAudioTask.h" 

using namespace HLSWP_BackgroundAudioSupport;
using namespace Platform;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::Media;
using namespace Windows::Media::Playback;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Microsoft::HLSClient;

HLSWP_BackgroundAudioSupport::BackgroundAudioTask::BackgroundAudioTask() : _taskinstance(nullptr)
{

}

void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::Run(IBackgroundTaskInstance^ taskInstance)
{
  OutputDebugString(L"Background: Task Starting\r\n");

  _taskinstance = taskInstance;

  //wire up the transport controls
  _transportcontrols = SystemMediaTransportControls::GetForCurrentView();
  _transportcontrols->IsStopEnabled = false;
  _transportcontrols->IsPauseEnabled = true;
  _transportcontrols->IsPlayEnabled = true;
  _transportcontrols->IsRewindEnabled = false;
  _transportcontrols->IsFastForwardEnabled = false;
  _transportcontrols->ButtonPressed += ref new Windows::Foundation::TypedEventHandler <
    Windows::Media::SystemMediaTransportControls ^,
    Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^ > (
    this,
    &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnButtonPressed);


  //wire up task completion and cancellation
  _taskinstance->Canceled += ref new Windows::ApplicationModel::Background::BackgroundTaskCanceledEventHandler(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnCanceled);
  _taskinstance->Task->Completed += ref new Windows::ApplicationModel::Background::BackgroundTaskCompletedEventHandler(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnCompleted);
  //handle messages from foreground
  BackgroundMediaPlayer::MessageReceivedFromForeground += ref new Windows::Foundation::EventHandler<Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMessageReceivedFromForeground);




  //inform foreground of initialization
  ValueSet^ vs = ref new ValueSet();
  vs->Insert(L"INIT_BACKGROUND_DONE", "");
  BackgroundMediaPlayer::SendMessageToForeground(vs);


  //keep alive
  _keepalive = _taskinstance->GetDeferral();

  OutputDebugString(L"Background: Task Started\r\n");
}

void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnButtonPressed(Windows::Media::SystemMediaTransportControls ^sender,
  Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^args)
{
  if (_controller == nullptr || !_controller->IsValid) return;

  if (args->Button == SystemMediaTransportControlsButton::Pause && BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Playing)
  {
    BackgroundMediaPlayer::Current->Pause();
  }
  else  if (args->Button == SystemMediaTransportControlsButton::Play && BackgroundMediaPlayer::Current->CurrentState != MediaPlayerState::Playing)
  {
    BackgroundMediaPlayer::Current->Play();
  }
}


void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnCanceled(
  Windows::ApplicationModel::Background::IBackgroundTaskInstance ^sender,
  Windows::ApplicationModel::Background::BackgroundTaskCancellationReason reason)
{
  OutputDebugString(L"Background: Task Canceled\r\n");
  _controllerFactory = nullptr;
}


void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnCompleted(
  Windows::ApplicationModel::Background::BackgroundTaskRegistration ^sender,
  Windows::ApplicationModel::Background::BackgroundTaskCompletedEventArgs ^args)
{
  OutputDebugString(L"Background: Task Completed\r\n");
  ValueSet^ vs = ref new ValueSet();
  vs->Insert(L"BACKGROUND_COMPLETED", L"");
  BackgroundMediaPlayer::SendMessageToForeground(vs);

  _controller = nullptr;
  _controllerFactory = nullptr;
  _keepalive.Get()->Complete();
}


void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::StartPlayback(String^ _url)
{
  //register HLS extension
  _controllerFactory = ref new HLSControllerFactory();
  _controllerFactory->HLSControllerReady += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSControllerFactory ^, Microsoft::HLSClient::IHLSController ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnHLSControllerReady);
  PropertySet^ ps = ref new PropertySet();
  ps->Insert(L"ControllerFactory", _controllerFactory);
  meMgr = ref new MediaExtensionManager();
  meMgr->RegisterByteStreamHandler(L"Microsoft.HLSClient.HLSPlaylistHandler", L".m3u8", L"application/x-mpegurl", ps);

  _mediaFailedEventToken = BackgroundMediaPlayer::Current->MediaFailed += ref new Windows::Foundation::TypedEventHandler<Windows::Media::Playback::MediaPlayer ^, Windows::Media::Playback::MediaPlayerFailedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMediaFailed);
  _mediaOpenedEventToken = BackgroundMediaPlayer::Current->MediaOpened += ref new Windows::Foundation::TypedEventHandler<Windows::Media::Playback::MediaPlayer ^, Platform::Object ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMediaOpened);

  //set source 
  BackgroundMediaPlayer::Current->SetUriSource(ref new Windows::Foundation::Uri(_url));
  OutputDebugString(L"Background: Playback Started\r\n");
}

void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnHLSControllerReady(Microsoft::HLSClient::IHLSControllerFactory ^sender, Microsoft::HLSClient::IHLSController ^args)
{
  OutputDebugString(L"Background: Controller Ready\r\n");
  //IF HLS registration works well - we should get here
  _controller = args;
  //if this is a variant playlist - set it to the lowest bitrate to play while on the background and also set the track type filter to audio only in case te lowest bitrate has both audio and video
  if (_controller->IsValid && _controller->Playlist != nullptr && _controller->Playlist->IsMaster)
  {
    _controller->EnableAdaptiveBitrateMonitor = false;
    _controller->Playlist->StartBitrate = _controller->Playlist->GetVariantStreams()->GetAt(0)->Bitrate;
    _controller->TrackTypeFilter = TrackType::AUDIO;
  }
}


void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMediaOpened(Windows::Media::Playback::MediaPlayer ^sender, Platform::Object ^args)
{
  OutputDebugString(L"Background: Media Opened\r\n");
  if (_si != nullptr && _si->Position >= 0){
    TimeSpan pos;
    pos.Duration = (long long)_si->Position;
    BackgroundMediaPlayer::Current->Position = pos;
  }
  BackgroundMediaPlayer::Current->Play();
}

void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMediaFailed(Windows::Media::Playback::MediaPlayer ^sender, Windows::Media::Playback::MediaPlayerFailedEventArgs ^args)
{
  //WHOOPS
  OutputDebugString(L"Background: Media Failed\r\n");
  auto msg = args->ErrorMessage;
  auto hr = args->ExtendedErrorCode;

  _si = nullptr;
  _controller = nullptr;
}

void HLSWP_BackgroundAudioSupport::BackgroundAudioTask::OnMessageReceivedFromForeground(Platform::Object ^sender,
  Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args)
{

  if (args->Data->HasKey(L"INIT_BACKGROUND"))//initialization
    return;
  else if (args->Data->HasKey(L"FOREGROUND_DEACTIVATED"))
  {
    OutputDebugString(L"Background: Received foreground deactivation message\r\n");
    //deserialize stream info
    _si = StreamInfo::FromString(dynamic_cast<String^>(args->Data->Lookup(L"FOREGROUND_DEACTIVATED")));
    if (_si != nullptr && _si->StreamURL != nullptr) //start playback
      StartPlayback(_si->StreamURL);
  }
  else if (args->Data->HasKey(L"FOREGROUND_SUSPENDED"))
  {
    OutputDebugString(L"Background: Received foreground suspension message\r\n");
    //deserialize stream info
    auto si = StreamInfo::FromString(dynamic_cast<String^>(args->Data->Lookup(L"FOREGROUND_SUSPENDED")));
    //are we already playing ?
    if ((BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Playing || BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Buffering || BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Opening)
      && _si != nullptr && _si->StreamURL == si->StreamURL) //we are already playing this stream in the background - do nothing
    {
      return;
    }
    else
    {
      _si = si;
      if (_si != nullptr && _si->StreamURL != nullptr) //start playback
        StartPlayback(_si->StreamURL);
    }
  }
  else if (args->Data->HasKey(L"FOREGROUND_RESUMED") || args->Data->HasKey(L"FOREGROUND_ACTIVATED"))
  {
    if (args->Data->HasKey(L"FOREGROUND_RESUMED"))
      OutputDebugString(L"Background: Received foreground resumption message\r\n");
    else
      OutputDebugString(L"Background: Received foreground activation message\r\n");
    if ((BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Playing || BackgroundMediaPlayer::Current->CurrentState == MediaPlayerState::Buffering)
      && _si != nullptr)
    {

      //stop and deinitialize
      _si->Position = BackgroundMediaPlayer::Current->Position.Duration;
    
      BackgroundMediaPlayer::Current->SetUriSource(nullptr);
      _controller = nullptr;
      _controllerFactory = nullptr;
      BackgroundMediaPlayer::Current->MediaOpened -= _mediaOpenedEventToken;
      BackgroundMediaPlayer::Current->MediaFailed -= _mediaFailedEventToken;

      //hand over current stream state to the foreground
      ValueSet^ vs = ref new ValueSet();
      vs->Insert(L"BACKGROUND_STREAMSTATE", _si->ToString());
      _si = nullptr;
      BackgroundMediaPlayer::SendMessageToForeground(vs);
    }
  }
}

