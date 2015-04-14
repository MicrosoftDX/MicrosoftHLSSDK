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
#include <collection.h>
#include "Playlist\Rendition.h"  
#include "HLSController.h"
#include "HLSSubtitleLocator.h"  
#include "HLSAlternateRendition.h" 

using namespace Concurrency;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

HLSAlternateRendition::HLSAlternateRendition(HLSController ^controller,
	unsigned int idx,
	const std::wstring renditionType,
	unsigned int bitrate) :
	_controller(controller),
	_index(idx),
	_renditiontype(renditionType),
	_bitratekey(bitrate)
{
	if (controller == nullptr || !controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
}

///<summary>Is this rendition the default ?</summary>
bool HLSAlternateRendition::IsDefault::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->Default;
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->Default;
	else
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->Default;
}

bool HLSAlternateRendition::IsAutoSelect::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->AutoSelect;
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->AutoSelect;
	else
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->AutoSelect;
}

bool HLSAlternateRendition::IsForced::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->Forced;
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->Forced;
	else
		return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->Forced;
}

Platform::String^ HLSAlternateRendition::Url::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->PlaylistUri.data());
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->PlaylistUri.data());
	else
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->PlaylistUri.data());
}


Platform::String^ HLSAlternateRendition::Type::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->Type.data());
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->Type.data());
	else
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->Type.data());
}

Platform::String^ HLSAlternateRendition::GroupID::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->GroupID.data());
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->GroupID.data());
	else
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->GroupID.data());
}

Platform::String^ HLSAlternateRendition::Language::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->Language.data());
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->Language.data());
	else
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->Language.data());
}

Platform::String^ HLSAlternateRendition::Name::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();
	if (_renditiontype == Rendition::TYPEAUDIO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions->at(_index)->Name.data());
	else if (_renditiontype == Rendition::TYPEVIDEO)
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions->at(_index)->Name.data());
	else
		return ref new Platform::String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions->at(_index)->Name.data());
}

bool HLSAlternateRendition::IsActive::get()
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();

  //not the active variant - cannot be active
	if (_controller->MediaSource->spRootPlaylist->ActiveVariant->Bandwidth != _bitratekey)
		return false;
	else
	{
		auto targetRendition = (_renditiontype == Rendition::TYPEAUDIO) ?
			_controller->MediaSource->spRootPlaylist->ActiveVariant->AudioRenditions->at(_index) :
			_controller->MediaSource->spRootPlaylist->ActiveVariant->VideoRenditions->at(_index);
		return targetRendition->IsActive;
	}
}

void HLSAlternateRendition::IsActive::set(bool val)
{
	if (_controller == nullptr || !_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();


	auto targetRendition = (_renditiontype == Rendition::TYPEAUDIO) ?
		_controller->MediaSource->spRootPlaylist->ActiveVariant->AudioRenditions->at(_index) :
		_controller->MediaSource->spRootPlaylist->ActiveVariant->VideoRenditions->at(_index);
	//request was to activate and rendition is already active, or request was to deactivate and rendition is not active
	if ((targetRendition->IsActive && val) || (!targetRendition->IsActive && !val))
		return;

  //we are not running yet - set immediately
	if (_controller->MediaSource->GetCurrentState() == MSS_OPENING)
	{

		if (_renditiontype == Rendition::TYPEAUDIO)
			_controller->MediaSource->spRootPlaylist->ActiveVariant->SetActiveAudioRendition(targetRendition.get());
		else
			_controller->MediaSource->spRootPlaylist->ActiveVariant->SetActiveVideoRendition(targetRendition.get());

		targetRendition->IsActive = true;
		return;
	}
	// the tasks may run after the scope (and perhaps "this") goes away--make copies of the values needed
	HLSController ^controllerCopy = _controller;
	wstring _renditiontypeCopy = _renditiontype;

	//else if deactivating, on a background thread 
	if (targetRendition->IsActive && val)
	{
		task<HRESULT>([controllerCopy, _renditiontypeCopy]() {
			//issue rendition chage request back to the main track
			return controllerCopy->MediaSource->RenditionChangeRequest(_renditiontypeCopy, nullptr, controllerCopy->MediaSource->spRootPlaylist->ActiveVariant); });
	}
	else
	{
		//else if activating, on a background thread
		task<HRESULT>([targetRendition, controllerCopy, _renditiontypeCopy]() {
			//issue rendition change request
			return controllerCopy->MediaSource->RenditionChangeRequest(_renditiontypeCopy, targetRendition, controllerCopy->MediaSource->spRootPlaylist->ActiveVariant); });
	}
}

HLSSubtitleRendition::HLSSubtitleRendition(HLSController ^controller, unsigned int idx, unsigned int bandwidth)
{
	std::wstring type = Rendition::TYPESUBTITLES;
	_internalRenditionImpl = ref new HLSAlternateRendition(controller, idx, type, bandwidth);
}

Windows::Foundation::Collections::IVector<IHLSSubtitleLocator^>^ HLSSubtitleRendition::GetSubtitleLocators()
{
	if (_internalRenditionImpl->_controller == nullptr || !_internalRenditionImpl->_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();

	auto pRenditionObj = _internalRenditionImpl->_controller->MediaSource->spRootPlaylist->Variants[_internalRenditionImpl->_bitratekey]->SubtitleRenditions->at(_internalRenditionImpl->_index);

	if (pRenditionObj->spPlaylist == nullptr || pRenditionObj->spPlaylist->Segments.size() == 0)
		return nullptr;

	auto count = (unsigned int) pRenditionObj->spPlaylist->Segments.size();
	auto retval = ref new Platform::Collections::Vector<IHLSSubtitleLocator^>(count);

	std::transform(begin(pRenditionObj->spPlaylist->Segments), end(pRenditionObj->spPlaylist->Segments), begin(retval), [](shared_ptr<MediaSegment> seg)
	{
		return ref new HLSSubtitleLocator(seg->MediaUri, seg->SequenceNumber, (float) seg->StartPTSNormalized->ValueInTicks / 10000000, (float) seg->Duration / 10000000);
	});

	return retval;

}

Windows::Foundation::IAsyncOperation<unsigned int>^ HLSSubtitleRendition::RefreshAsync()
{
	if (_internalRenditionImpl->_controller == nullptr || !_internalRenditionImpl->_controller->IsValid)
		throw ref new Platform::ObjectDisposedException();

	return create_async([this]()
	{
		if (_internalRenditionImpl->_controller == nullptr || !_internalRenditionImpl->_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

		auto pRenditionObj = _internalRenditionImpl->_controller->MediaSource->spRootPlaylist->Variants[_internalRenditionImpl->_bitratekey]->SubtitleRenditions->at(_internalRenditionImpl->_index);
		
    if (pRenditionObj->spPlaylist == nullptr)
			pRenditionObj->DownloadRenditionPlaylistAsync().get();
		else if (_internalRenditionImpl->_controller->MediaSource->spRootPlaylist->IsLive)
		{
			pRenditionObj->DownloadRenditionPlaylistAsync().get();
			pRenditionObj->spPlaylist->MergeAlternateRenditionPlaylist();
		}
		return (unsigned int) pRenditionObj->spPlaylist->Segments.size();
	});
}

