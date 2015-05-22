#include "Communications.h"

using namespace HLSWP_BackgroundAudioSupport;
using namespace Windows::Media;
using namespace Windows::Media::Playback;
using namespace Windows::Foundation::Collections;

HLSWP_BackgroundAudioSupport::ForegroundMessenger::ForegroundMessenger()
{
  BackgroundMediaPlayer::MessageReceivedFromBackground += ref new Windows::Foundation::EventHandler<Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::ForegroundMessenger::OnMessageReceivedFromBackground);
}

void HLSWP_BackgroundAudioSupport::ForegroundMessenger::OnMessageReceivedFromBackground(Platform::Object ^sender, Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args)
{

}

void HLSWP_BackgroundAudioSupport::ForegroundMessenger::Send_ForegroundSuspending(String^ StreamURL)
{

}
void HLSWP_BackgroundAudioSupport::ForegroundMessenger::Send_ForegroundDeactivating(String^ streamURL, unsigned long long position, bool isLive, bool isEventPlaylist)
{
  //StreamStateTransfer::PersistStreamState(streamURL, position, isLive, isEventPlaylist);
  ValueSet^ props = ref new ValueSet();
  auto si = ref new StreamInfo(streamURL, position, isLive, isEventPlaylist);
  props->Insert(L"DATA", si->ToString());
  props->Insert(L"MESSAGEID", ref new String(L"FOREGROUND_DEACTIVATING"));
 
  BackgroundMediaPlayer::SendMessageToBackground(props);
}




HLSWP_BackgroundAudioSupport::BackgroundMessenger::BackgroundMessenger()
{
  BackgroundMediaPlayer::MessageReceivedFromForeground += ref new Windows::Foundation::EventHandler<Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMessageReceivedFromForeground);
}

HLSWP_BackgroundAudioSupport::BackgroundMessenger::BackgroundMessenger(HLSControllerFactory^ factory) :_controllerFactory(factory)
{ 
  BackgroundMediaPlayer::MessageReceivedFromForeground += ref new Windows::Foundation::EventHandler<Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMessageReceivedFromForeground);
  _controllerFactory->HLSControllerReady += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSControllerFactory ^, Microsoft::HLSClient::IHLSController ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnHLSControllerReady);
}

void HLSWP_BackgroundAudioSupport::BackgroundMessenger::Send_BackgroundTaskStarted()
{

}

void HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMessageReceivedFromForeground(Platform::Object ^sender, Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args)
{
  if (args->Data->HasKey("MESSAGEID") == false)
    return;
   
  if (dynamic_cast<String^>(args->Data->Lookup("MESSAGEID")) == L"FOREGROUND_DEACTIVATING")
  {
   
    _si = StreamInfo::FromString((String^) args->Data->Lookup("DATA"));
    //extract the stream details
    WireTransportControls(_si->IsLive, _si->IsEventPlaylist);

    StartPlayback(_si->StreamURL);
   
  }
}

void HLSWP_BackgroundAudioSupport::BackgroundMessenger::WireTransportControls(bool IsLive, bool IsEventPlaylist)
{
  
    _transportControls = SystemMediaTransportControls::GetForCurrentView();
    _transportControls->ButtonPressed += ref new Windows::Foundation::TypedEventHandler<Windows::Media::SystemMediaTransportControls ^, Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnButtonPressed);
    _transportControls->IsPlayEnabled = true;
    _transportControls->IsPauseEnabled = true;
   
}

void HLSWP_BackgroundAudioSupport::BackgroundMessenger::StartPlayback(String^ _url)
{ 

  _controllerFactory = ref new HLSControllerFactory();
  _controllerFactory->HLSControllerReady += ref new Windows::Foundation::TypedEventHandler<Microsoft::HLSClient::IHLSControllerFactory ^, Microsoft::HLSClient::IHLSController ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnHLSControllerReady);
  PropertySet^ ps = ref new PropertySet();
  ps->Insert(L"ControllerFactory", _controllerFactory);
  meMgr = ref new MediaExtensionManager();
  meMgr->RegisterByteStreamHandler(L"Microsoft.HLSClient.HLSPlaylistHandler", L".m3u8", L"application/x-mpegurl", ps);

  BackgroundMediaPlayer::Current->MediaFailed += ref new Windows::Foundation::TypedEventHandler<Windows::Media::Playback::MediaPlayer ^, Windows::Media::Playback::MediaPlayerFailedEventArgs ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMediaFailed);
  BackgroundMediaPlayer::Current->MediaOpened += ref new Windows::Foundation::TypedEventHandler<Windows::Media::Playback::MediaPlayer ^, Platform::Object ^>(this, &HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMediaOpened);
  BackgroundMediaPlayer::Current->SetUriSource(ref new Windows::Foundation::Uri(_url));
} 
void HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnHLSControllerReady(Microsoft::HLSClient::IHLSControllerFactory ^sender, Microsoft::HLSClient::IHLSController ^args)
{
  _controller = args;
  //if this is a variant playlist - set it to the lowest bitrate to play while on the background
  if (_controller->IsValid && _controller->Playlist != nullptr && _controller->Playlist->IsMaster)
  {
    _controller->EnableAdaptiveBitrateMonitor = false;
    _controller->Playlist->StartBitrate = _controller->Playlist->GetVariantStreams()->GetAt(0)->Bitrate;
  }
}


void HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMediaOpened(Windows::Media::Playback::MediaPlayer ^sender, Platform::Object ^args)
{
  if (_si != nullptr && _si->Position >= 0){
    TimeSpan pos;
    pos.Duration = (long long) _si->Position;
    BackgroundMediaPlayer::Current->Position = pos;
  }
  BackgroundMediaPlayer::Current->Play();
}

void HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnButtonPressed(Windows::Media::SystemMediaTransportControls ^sender, Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^args)
{

}



void HLSWP_BackgroundAudioSupport::BackgroundMessenger::OnMediaFailed(Windows::Media::Playback::MediaPlayer ^sender, Windows::Media::Playback::MediaPlayerFailedEventArgs ^args)
{
  if (_si != nullptr && _si->Position >= 0){
    TimeSpan pos;
    pos.Duration = (long long) _si->Position;
    BackgroundMediaPlayer::Current->Position = pos;
  }
  BackgroundMediaPlayer::Current->Play();
}
