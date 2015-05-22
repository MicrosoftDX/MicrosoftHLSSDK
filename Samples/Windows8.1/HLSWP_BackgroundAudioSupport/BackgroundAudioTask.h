#pragma once 
#include <string>
#include <sstream>
#include <agile.h>

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Microsoft::HLSClient;
using namespace Windows::ApplicationModel::Background;
using namespace Windows::Media; 
using namespace Windows::Media::Playback; 
using namespace std; 

namespace HLSWP_BackgroundAudioSupport
{

  public ref class StreamInfo sealed
  {
  private:
    String^ _streamurl;
    unsigned long long _position; 
  public:

    StreamInfo(String^ url, unsigned long long position)
    {
      _streamurl = url;
      _position = position; 
    }
    property String^ StreamURL
    {
      String^ get()
      {
        return _streamurl;
      }
      void set(String^ val)
      {
        _streamurl = val;
      }
    } 

    property unsigned long long Position
    {
      unsigned long long get()
      {
        return _position;
      }
      void set(unsigned long long val)
      {
        _position = val;
      }
    }
   

    virtual String^ ToString() override
    {
      wstring buff(_streamurl->Data());

      std::wostringstream wospos;
      wospos << _position;
      buff = buff + L"^" + wospos.str();

      return ref new String(buff.data());
    }

    static StreamInfo^ FromString(String^ flat)
    {
      std::wstring szflat = flat->Data();
      size_t off = 0;
      size_t cpos = szflat.find_first_of('^');
      String^ url = ref new String(szflat.substr(off, cpos).data());

      off = cpos + 1; 
      std::wistringstream wisposition(szflat.substr(off, szflat.size() - off));
      unsigned long long pos = 0;
      wisposition >> pos; 

      return ref new StreamInfo(url, pos);
    }
  };

  public ref class StreamStateTransfer sealed
  {
  public:
    static void PersistStreamState(String^ Url, unsigned long long Position, bool IsLive, bool IsLiveEvent)
    {
      StreamInfo^ si = ref new StreamInfo(Url, Position);
      ApplicationData::Current->LocalSettings->Values->Insert(Url, si);
    }

    static StreamInfo^ ReadStreamState(String^ Url)
    {
      return ApplicationData::Current->LocalSettings->Values->HasKey(Url) == false ?
        nullptr :
        dynamic_cast<StreamInfo^>(ApplicationData::Current->LocalSettings->Values->Lookup(Url));
    }
  };

 
  public ref class BackgroundAudioTask sealed : public IBackgroundTask
    {
    private:
      IBackgroundTaskInstance^ _taskinstance;
      SystemMediaTransportControls^ _transportcontrols;
      HLSControllerFactory^ _controllerFactory;
      IHLSController^ _controller;
      MediaExtensionManager^ meMgr;
      StreamInfo^ _si;
      Platform::Agile<BackgroundTaskDeferral^> _keepalive;
      EventRegistrationToken _mediaFailedEventToken, _mediaOpenedEventToken;
    public:
      BackgroundAudioTask();

      virtual void Run(IBackgroundTaskInstance^ taskInstance);
      void OnButtonPressed(Windows::Media::SystemMediaTransportControls ^sender, Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^args);
      void OnCanceled(Windows::ApplicationModel::Background::IBackgroundTaskInstance ^sender, Windows::ApplicationModel::Background::BackgroundTaskCancellationReason reason);
      void OnCompleted(Windows::ApplicationModel::Background::BackgroundTaskRegistration ^sender, Windows::ApplicationModel::Background::BackgroundTaskCompletedEventArgs ^args);
      void OnMessageReceivedFromForeground(Platform::Object ^sender, Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args);
      
      void StartPlayback(String^ _url);
      void OnHLSControllerReady(Microsoft::HLSClient::IHLSControllerFactory ^sender, Microsoft::HLSClient::IHLSController ^args);
      void OnMediaOpened(Windows::Media::Playback::MediaPlayer ^sender, Platform::Object ^args);
      void OnMediaFailed(Windows::Media::Playback::MediaPlayer ^sender, Windows::Media::Playback::MediaPlayerFailedEventArgs ^args);
  };
}