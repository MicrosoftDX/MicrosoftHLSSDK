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
#include <ppltasks.h>
#include "HLSManager.h"

using namespace Microsoft::HLSClientExtensions;
using namespace Microsoft::HLSClient;
using namespace Microsoft::CC608;
using namespace Platform;
using namespace concurrency;

using Microsoft::HLSClient::IHLSControllerFactory;
using Microsoft::HLSClient::IHLSController;

HLSManager::HLSManager() :
_isInitialized(false),
_isPlaylistEventsHandled(false),
_startupBitrateSpecified(false),
_minBitrateSpecified(false),
_maxBitrateSpecified(false),
_controllerFactory(ref new Microsoft::HLSClient::HLSControllerFactory()),
_controllerFactoryPropertySet(ref new Windows::Foundation::Collections::PropertySet())
{
  _controllerFactoryPropertySet->Insert(L"ControllerFactory", _controllerFactory);
}

void HLSManager::Initialize(Windows::Media::MediaExtensionManager^ extensionManager)
{
  if (_isInitialized)
  {
    return;
  }

  // dispatcher is used to raise events on the ui thread, 
  // required when this component is used from JavaScript
  _dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

  _extensionManager = extensionManager;

  // register scheme handlers so that we can intercept the initial playlist request if necessary
  _extensionManager->RegisterSchemeHandler("Microsoft.HLSClient.HLSPlaylistHandler", "ms-hls:", _controllerFactoryPropertySet);
  _extensionManager->RegisterSchemeHandler("Microsoft.HLSClient.HLSPlaylistHandler", "ms-hls-s:", _controllerFactoryPropertySet);


  // register both common MIME types
  RegisterByteStreamHandler(L".m3u8", L"application/vnd.apple.mpegurl");
  RegisterByteStreamHandler(L".m3u8", L"application/x-mpegurl");

  _controllerFactory->HLSControllerReady +=
    ref new Windows::Foundation::TypedEventHandler<IHLSControllerFactory^, IHLSController^>(this, &HLSManager::ControllerFactory_HLSControllerReady);

  _isInitialized = true;
}

void HLSManager::Uninitialize()
{
  UninitializePlaylist();
  _controller = nullptr;
  _isInitialized = false;
}

// Supports adding a non-standard registration if necessary. (For example, some servers may serve the playlist in a different way.)
void HLSManager::AddNonStandardRegistration(Platform::String^ fileExtension, Platform::String^ mimeType)
{
  RegisterByteStreamHandler(fileExtension, mimeType);
}

// Registers byte stream handler. 
void HLSManager::RegisterByteStreamHandler(Platform::String^ fileExtension, Platform::String^ mimeType)
{
  if (fileExtension->IsEmpty() || mimeType->IsEmpty())
  {
    throw ref new InvalidArgumentException(L"fileExtension and mimeType cannot be empty");
  }

  ByteStreamHandlerInfo b(std::wstring(fileExtension->Data()), std::wstring(mimeType->Data()));

  // only add if it doesn't already exist
  if (std::find(_byteStreamHandlerInfos.begin(), _byteStreamHandlerInfos.end(), b) != _byteStreamHandlerInfos.end())
  {
    throw ref new InvalidArgumentException(L"Pipeline registration already exists. Add each registration only once.");
  }

  // add to list, so can check if others are added
  _byteStreamHandlerInfos.push_back(b);

  _extensionManager->RegisterByteStreamHandler(L"Microsoft.HLSClient.HLSPlaylistHandler", fileExtension, mimeType, _controllerFactoryPropertySet);
}

void HLSManager::ControllerFactory_HLSControllerReady(IHLSControllerFactory^ sender, IHLSController^ controller)
{
  // uninitialize the current playlist
  if (_controller != nullptr && _controller->IsValid)
  {
    UninitializePlaylist();
  }

  _controller = controller;
  InitializePlaylist();

  // raise the controller-ready event for the app in case they need advanced scenerios not directly supported by the plugin
  // (this is how they get access to the controller)
  task_completion_event<void> tceJsEvent;

  RunAsyncOnUIThread([this, &tceJsEvent]()
  {
    if (_controller != nullptr && _controller->IsValid)
      HLSControllerReady(this, _controller);
    tceJsEvent.set();
  });

  // block here to ensure that the event handler can set parameters before the SDK has moved on
  task<void>(tceJsEvent).wait();
}

// this event cannot be cancelled, but the corresponding event on the HLS Client SDK can be cancelled
void HLSManager::Controller_BitrateSwitchSuggested(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ eventArgs)
{
  // By dispatching this onto a message queue for processing by the UI thread, it is impossible to cancel the 
  // bitrate switch. (The code in the HLS Client SDK waits for the event handlers to execute, setting the .Cancel property to true
  // if desired, and then reads the .Cancel property and cancels if it is true. However, by dispatching to the UI thread,
  // the event handler completes while the message is still in the UI thread's message queue, and the HLS Client SDK checks the 
  // value of .Cancel before the UI thread runs the code that might set it to true.)
  //
  // Therefore, this event is not cancellable. If users want to cancel, they will need to write
  // their own WinRT component, and then listen for the corresponding event on the HLS Client SDK that does not dispatch to the UI thread.

  // Note: the risks of making this block while waiting for the JS thread to process it outweigh
  // the benefits of making this cancellable. The issue is that this event happens during playback, and can cause video
  // quality issues.

  RunAsyncOnUIThread([this, eventArgs]()
  {
    try{
      if (_controller != nullptr && _controller->IsValid)
        // change the sender as we are changing the event type
        BitrateSwitchSuggestedCannotCancel(this, eventArgs);
    }
    catch (...)
    {

    }
  });
}

void HLSManager::Controller_BitrateSwitchCompleted(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ eventArgs)
{
  RunAsyncOnUIThread([this, eventArgs]()
  {
    try{
      if (_controller != nullptr && _controller->IsValid)
        // change the sender as we are changing the event type
        BitrateSwitchCompleted(this, eventArgs);
    }
    catch (...)
    {

    }
  });
}

void HLSManager::Controller_BitrateSwitchCancelled(IHLSPlaylist^ sender, IHLSBitrateSwitchEventArgs^ eventArgs)
{
  RunAsyncOnUIThread([this, eventArgs]()
  {

    try{
      if (_controller != nullptr && _controller->IsValid)
        // change the sender as we are changing the event type
        BitrateSwitchCancelled(this, eventArgs);
    }
    catch (...)
    {

    }
  });
}

void HLSManager::Controller_StreamSelectionChanged(IHLSPlaylist^ sender, IHLSStreamSelectionChangedEventArgs^ eventArgs)
{
  RunAsyncOnUIThread([this, eventArgs]()
  {
    try{
      if (_controller != nullptr && _controller->IsValid)
        // change the sender as we are changing the event type
        StreamSelectionChanged(this, eventArgs);
    }
    catch (...)
    {

    }
  });
}

void HLSManager::Controller_SegmentSwitched(IHLSPlaylist^ sender, IHLSSegmentSwitchEventArgs^ eventArgs)
{
  RunAsyncOnUIThread([this, eventArgs]()
  {
    try{
      if (_controller != nullptr && _controller->IsValid)
    // change the sender as we are changing the event type
    SegmentSwitched(this, eventArgs);
    }
    catch (...)
    {

    }
  });
}

// raise the event on the UI thread
void HLSManager::RunAsyncOnUIThread(std::function<void(void)> action)
{
  _dispatcher->RunAsync(
    Windows::UI::Core::CoreDispatcherPriority::Normal,
    ref new Windows::UI::Core::DispatchedHandler(action));
}

unsigned int HLSManager::StartupBitrate::get()
{
  return _startupBitrate;
}

void HLSManager::StartupBitrate::set(unsigned int value)
{
  _startupBitrate = value;
  _startupBitrateSpecified = true;
}

unsigned int HLSManager::MinBitrate::get()
{
  return _minBitrate;
}

void HLSManager::MinBitrate::set(unsigned int value)
{
  _minBitrate = value;
  _minBitrateSpecified = true;

  if (_controller != nullptr && _controller->IsValid)
  {
    _controller->Playlist->MinimumAllowedBitrate = _minBitrate;
  }
}

unsigned int HLSManager::MaxBitrate::get()
{
  return _maxBitrate;
}

void HLSManager::MaxBitrate::set(unsigned int value)
{
  _maxBitrate = value;
  _maxBitrateSpecified = true;

  if (_controller != nullptr && _controller->IsValid)
  {
    _controller->Playlist->MaximumAllowedBitrate = _maxBitrate;
  }
}

Microsoft::HLSClient::IHLSControllerFactory^ HLSManager::ControllerFactory::get()
{
  return _controllerFactory;
}

void HLSManager::InitializePlaylist()
{
  _isPlaylistEventsHandled = true;

  _bitrateSwitchSuggestedCookie =
    _controller->Playlist->BitrateSwitchSuggested +=
    ref new Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>(this, &HLSManager::Controller_BitrateSwitchSuggested);

  _bitrateSwitchCompletedCookie =
    _controller->Playlist->BitrateSwitchCompleted +=
    ref new Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>(this, &HLSManager::Controller_BitrateSwitchCompleted);

  _bitrateSwitchCancelledCookie =
    _controller->Playlist->BitrateSwitchCancelled +=
    ref new Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSBitrateSwitchEventArgs^>(this, &HLSManager::Controller_BitrateSwitchCancelled);

  _streamSelectionChangedCookie =
    _controller->Playlist->StreamSelectionChanged +=
    ref new Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSStreamSelectionChangedEventArgs^>(this, &HLSManager::Controller_StreamSelectionChanged);

  _segmentSwitchedCookie =
    _controller->Playlist->SegmentSwitched +=
    ref new Windows::Foundation::TypedEventHandler<IHLSPlaylist^, IHLSSegmentSwitchEventArgs^>(this, &HLSManager::Controller_SegmentSwitched);

  if (_startupBitrateSpecified && _controller != nullptr && _controller->IsValid)
  {
    _controller->Playlist->StartBitrate = _startupBitrate;
  }

  if (_minBitrateSpecified  && _controller != nullptr && _controller->IsValid)
  {
    _controller->Playlist->MinimumAllowedBitrate = _minBitrate;
  }

  if (_maxBitrateSpecified && _controller != nullptr && _controller->IsValid)
  {
    _controller->Playlist->MaximumAllowedBitrate = _maxBitrate;
  }
}

void HLSManager::UninitializePlaylist()
{
  if (_isPlaylistEventsHandled)
  {
    _isPlaylistEventsHandled = false;
    _controller->Playlist->BitrateSwitchSuggested -= _bitrateSwitchSuggestedCookie;
    _controller->Playlist->BitrateSwitchCompleted -= _bitrateSwitchCompletedCookie;
    _controller->Playlist->BitrateSwitchCancelled -= _bitrateSwitchCancelledCookie;
    _controller->Playlist->StreamSelectionChanged -= _streamSelectionChangedCookie;
    _controller->Playlist->SegmentSwitched -= _segmentSwitchedCookie;
  }
}
