#pragma once
#include <string>
#include <sstream>
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Microsoft::HLSClient;
using namespace Windows::Media::Playback;
using namespace Windows::Media;
using namespace std;


namespace HLSWP_BackgroundAudioSupport
{
  public ref class StreamInfo sealed
  {
  private:
    String^ _streamurl;
    unsigned long long _position;
    bool _iseventplaylist;
    bool _islive;
  public:

    StreamInfo(String^ url, unsigned long long position, bool islive, bool isevent)
    {
      _streamurl = url;
      _position = position;
      _islive = islive;
      _iseventplaylist = isevent;
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
    property bool IsEventPlaylist
    {
      bool get()
      {
        return _iseventplaylist;
      }
      void set(bool val)
      {
        _iseventplaylist = val;
      }
    }
    property bool IsLive
    {
      bool get()
      {
        return _islive;
      }
      void set(bool val)
      {
        _islive = val;
      }
    }

    virtual String^ ToString() override 
    {
      wstring buff(_streamurl->Data());

      std::wostringstream wospos;
      wospos << _position;
      buff = buff + L"," + wospos.str() + L"," + (_islive ? L"true" : L"false") + +L"," + (_iseventplaylist ? L"true" : L"false");

      return ref new String(buff.data());
    }

    static StreamInfo^ FromString(String^ flat)
    {
      std::wstring szflat = flat->Data();
      size_t off = 0;
      size_t cpos = szflat.find_first_of(',');
      String^ url = ref new String(szflat.substr(off, cpos).data());

      off = cpos + 1;
      cpos = szflat.find_first_of(',',off); 
      std::wistringstream wisposition(szflat.substr(off, cpos - off));
      unsigned long long pos = 0;
      wisposition >> pos;

      off = cpos + 1;
      cpos = szflat.find_first_of(',', off);
      auto szislive = szflat.substr(off, cpos - off);
       
      off = cpos + 1;
      auto szisEventPlaylist = szflat.substr(off, szflat.size() - off);
      
      return ref new StreamInfo(url, pos, szislive == L"true" ? true : false, szisEventPlaylist == L"true" ? true : false);
    }
  };

  public ref class StreamStateTransfer sealed
  {
  public:
    static void PersistStreamState(String^ Url, unsigned long long Position, bool IsLive, bool IsLiveEvent)
    {
      StreamInfo^ si = ref new StreamInfo(Url, Position, IsLive, IsLiveEvent);
      ApplicationData::Current->LocalSettings->Values->Insert(Url, si);
    }

    static StreamInfo^ ReadStreamState(String^ Url)
    {
      return ApplicationData::Current->LocalSettings->Values->HasKey(Url) == false ?
        nullptr :
        dynamic_cast<StreamInfo^>(ApplicationData::Current->LocalSettings->Values->Lookup(Url));
    }
  };

  public enum class MESSAGEDEF
  {
    FOREGROUND_DEACTIVATING,
    FOREGROUND_ACTIVATING
  };


  public ref class ForegroundMessenger sealed
  {
  public:
    ForegroundMessenger();
    void Send_ForegroundSuspending(String^ streamUrl);
    void Send_ForegroundDeactivating(String^ streamUrl, unsigned long long  position, bool isLive, bool isEventPlaylist);
    void OnMessageReceivedFromBackground(Platform::Object ^sender, Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args);
  };

  public ref class BackgroundMessenger sealed
  {
  private:
    StreamInfo^ _si;
    IHLSController^ _controller;
    HLSControllerFactory^ _controllerFactory;
    SystemMediaTransportControls^ _transportControls;
    MediaExtensionManager^ meMgr;
    void WireTransportControls(bool IsLive, bool IsEventPlaylist);
    void StartPlayback(String^ _url);
  public:
    BackgroundMessenger();
    BackgroundMessenger(HLSControllerFactory^ _factory);
    void Send_BackgroundTaskStarted();
    void OnMessageReceivedFromForeground(Platform::Object ^sender, Windows::Media::Playback::MediaPlayerDataReceivedEventArgs ^args);
    void OnButtonPressed(Windows::Media::SystemMediaTransportControls ^sender, Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs ^args);
    void OnHLSControllerReady(Microsoft::HLSClient::IHLSControllerFactory ^sender, Microsoft::HLSClient::IHLSController ^args);
    void OnMediaOpened(Windows::Media::Playback::MediaPlayer ^sender, Platform::Object ^args);
    void OnMediaFailed(Windows::Media::Playback::MediaPlayer ^sender, Windows::Media::Playback::MediaPlayerFailedEventArgs ^args);
  };
}