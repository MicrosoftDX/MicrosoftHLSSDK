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
#include <vector>
#include <functional>
#include "ByteStreamHandlerInfo.h"

namespace Microsoft
{
	namespace HLSClientExtensions
	{
		ref class HLSManager;

		public delegate void HLSControllerReadyEventHandler(Microsoft::HLSClientExtensions::HLSManager^ sender, Microsoft::HLSClient::IHLSController^ controller);
		public delegate void BitrateSwitchEventHandler(Microsoft::HLSClientExtensions::HLSManager^ sender, Microsoft::HLSClient::IHLSBitrateSwitchEventArgs^ eventArgs);
		public delegate void StreamSelectionChangedEventHandler(Microsoft::HLSClientExtensions::HLSManager^ sender, Microsoft::HLSClient::IHLSStreamSelectionChangedEventArgs^ eventArgs);
		public delegate void SegmentSwitchedEventHandler(Microsoft::HLSClientExtensions::HLSManager^ sender, Microsoft::HLSClient::IHLSSegmentSwitchEventArgs^ eventArgs);

		public ref class HLSManager sealed
		{
		public:
			HLSManager();

			property bool IsInitialized
			{ 
				bool get() { return _isInitialized; }
			}

			property unsigned int StartupBitrate
			{
				unsigned int get();
				void set(unsigned int value);
			}

			property unsigned int MaxBitrate
			{
				unsigned int get();
				void set(unsigned int value);
			}

			property unsigned int MinBitrate
			{
				unsigned int get();
				void set(unsigned int value);
			}

			property Microsoft::HLSClient::IHLSControllerFactory^ ControllerFactory
			{
				Microsoft::HLSClient::IHLSControllerFactory^ get();
			}

			event HLSControllerReadyEventHandler^ HLSControllerReady;

			// see additional notes in the implementation about why this event cannot be cancelled
			event BitrateSwitchEventHandler^ BitrateSwitchSuggestedCannotCancel;
			event BitrateSwitchEventHandler^ BitrateSwitchCompleted;
			event BitrateSwitchEventHandler^ BitrateSwitchCancelled;

			event StreamSelectionChangedEventHandler^ StreamSelectionChanged;

			event SegmentSwitchedEventHandler^ SegmentSwitched;

			// used to track which stream this instance is playing
			property Platform::String^ SourceUri;

			void Initialize(Windows::Media::MediaExtensionManager^ extensionManager);
			void Uninitialize();
			void AddNonStandardRegistration(Platform::String^ fileExtension, Platform::String^ mimeType);

		private:
			bool _isInitialized;
			std::vector<ByteStreamHandlerInfo> _byteStreamHandlerInfos;
			Windows::Media::MediaExtensionManager^ _extensionManager;
			Microsoft::HLSClient::IHLSController^ _controller;
			Windows::UI::Core::CoreDispatcher^ _dispatcher;
			Microsoft::HLSClient::IHLSControllerFactory^ _controllerFactory;
			Windows::Foundation::Collections::PropertySet^ _controllerFactoryPropertySet;
			bool _isPlaylistEventsHandled;

			unsigned int _startupBitrate;
			bool _startupBitrateSpecified;
			unsigned int _minBitrate;
			bool _minBitrateSpecified;
			unsigned int _maxBitrate;
			bool _maxBitrateSpecified;
			
			void HLSManager::RunAsyncOnUIThread(std::function<void(void)> action);
			void RegisterByteStreamHandler(Platform::String^ fileExtension, Platform::String^ mimeType);
			void InitializePlaylist();
			void UninitializePlaylist();

			void ControllerFactory_HLSControllerReady(Microsoft::HLSClient::IHLSControllerFactory^ sender, Microsoft::HLSClient::IHLSController^ controller);

			void Controller_BitrateSwitchSuggested(Microsoft::HLSClient::IHLSPlaylist^ sender, Microsoft::HLSClient::IHLSBitrateSwitchEventArgs^ eventArgs);
			void Controller_BitrateSwitchCompleted(Microsoft::HLSClient::IHLSPlaylist^ sender, Microsoft::HLSClient::IHLSBitrateSwitchEventArgs^ eventArgs);
			void Controller_BitrateSwitchCancelled(Microsoft::HLSClient::IHLSPlaylist^ sender, Microsoft::HLSClient::IHLSBitrateSwitchEventArgs^ eventArgs);

			void Controller_StreamSelectionChanged(Microsoft::HLSClient::IHLSPlaylist^ sender, Microsoft::HLSClient::IHLSStreamSelectionChangedEventArgs^ eventArgs);

			void Controller_SegmentSwitched(Microsoft::HLSClient::IHLSPlaylist^ sender, Microsoft::HLSClient::IHLSSegmentSwitchEventArgs^ eventArgs);

			Windows::Foundation::EventRegistrationToken _controllerReadyCookie;

			Windows::Foundation::EventRegistrationToken _bitrateSwitchSuggestedCookie;
			Windows::Foundation::EventRegistrationToken _bitrateSwitchCompletedCookie;
			Windows::Foundation::EventRegistrationToken _bitrateSwitchCancelledCookie;

			Windows::Foundation::EventRegistrationToken _streamSelectionChangedCookie;

			Windows::Foundation::EventRegistrationToken _segmentSwitchedCookie;
		};
	}
}