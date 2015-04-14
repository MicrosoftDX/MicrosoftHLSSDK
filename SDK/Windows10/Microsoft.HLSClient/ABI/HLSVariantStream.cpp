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

#include <collection.h> 
#include "Playlist\Rendition.h"  
#include "Playlist\Playlist.h" 
#include "HLSController.h"
#include "HLSPlaylist.h"
#include "HLSAlternateRendition.h" 
#include "HLSVariantStream.h"

using namespace Platform::Collections;
using namespace Platform;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

HLSVariantStream::HLSVariantStream(HLSController^ controller, unsigned int index, bool IsIndexBitrate) : _controller(controller)
{
  if (controller == nullptr || !controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (IsIndexBitrate)
    _bitratekey = index;
  else
  {
    //find the bitrate 
    unsigned int i = 0;
    for (auto itr = controller->MediaSource->spRootPlaylist->Variants.begin(); itr != controller->MediaSource->spRootPlaylist->Variants.end(); itr++, i++)
    {
      if (i == index)
      {
        //store it
        _bitratekey = itr->first;
        break;
      }
    }
  }
}

bool HLSVariantStream::IsActive::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->IsActive;
}
bool HLSVariantStream::HasResolution::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->HasResolution;
}
unsigned int HLSVariantStream::HorizontalResolution::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->HorizontalResolution;
}
unsigned int HLSVariantStream::VerticalResolution::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VerticalResolution;
}
String^ HLSVariantStream::Url::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return ref new String(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->PlaylistUri.data());
}
IHLSPlaylist^ HLSVariantStream::Playlist::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_playlist == nullptr && _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->spPlaylist != nullptr)
    _playlist = ref new HLSPlaylist(_controller, true, _bitratekey);
  return _playlist;
}

Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ HLSVariantStream::GetPlaylistBatchItemDurations()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->BatchItems.size() == 0)
    return nullptr;

  auto retval = ref new Platform::Collections::Vector<Windows::Foundation::TimeSpan,TSEqual>((unsigned int)_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->BatchItems.size() + 1);
  unsigned long long TotDur = 0; 

  std::transform(begin(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->BatchItems),
    end(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->BatchItems), 
    begin(retval) + 1, 
    [this,&TotDur](std::pair<wstring, shared_ptr<Microsoft::HLSClient::Private::Playlist>> itm)
  {
    TotDur += itm.second->TotalDuration;
    return Windows::Foundation::TimeSpan{ (long long) itm.second->TotalDuration };  
  });


  //for the first playlist in the batch - since it does not get added as a part of the batch
  retval->SetAt(0, Windows::Foundation::TimeSpan{ (long long) (_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->spPlaylist->TotalDuration - TotDur) });
  return retval;
}
Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ HLSVariantStream::GetAudioRenditions()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_audioRenditions == nullptr)
  { 

    if (_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions == nullptr ||
      _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions.get()->size() == 0)
      return nullptr;

    _audioRenditions = ref new Vector<IHLSAlternateRendition^>((unsigned int)_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions.get()->size());
    unsigned int ctr = 0;
    std::transform(begin(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions.get())),
      end(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->AudioRenditions.get())), begin(_audioRenditions), [this, &ctr](shared_ptr<Rendition> rend)
    {
      return ref new HLSAlternateRendition(_controller, ctr++, Rendition::TYPEAUDIO, _bitratekey);
    });

  }
  return _audioRenditions;

}
Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ HLSVariantStream::GetVideoRenditions()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_videoRenditions == nullptr)
  { 
    if (_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions == nullptr ||
      _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions.get()->size() == 0)
      return nullptr;

    _videoRenditions = ref new Vector<IHLSAlternateRendition^>((unsigned int) _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions.get()->size());
    unsigned int ctr = 0;
    std::transform(begin(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions.get())),
      end(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->VideoRenditions.get())), begin(_videoRenditions), [this, &ctr](shared_ptr<Rendition> rend)
    {
      return ref new HLSAlternateRendition(_controller, ctr++, Rendition::TYPEVIDEO, _bitratekey);
    });

  }
  return _videoRenditions;
}
Windows::Foundation::Collections::IVector<IHLSAlternateRendition^>^ HLSVariantStream::GetSubtitleRenditions()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_subtitleRenditions == nullptr)
  { 
    if (_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions == nullptr ||
      _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions.get()->size() == 0)
      return nullptr;

    _subtitleRenditions = ref new Vector<IHLSAlternateRendition^>((unsigned int) _controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions.get()->size());
    unsigned int ctr = 0;
    std::transform(begin(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions.get())),
      end(*(_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->SubtitleRenditions.get())), begin(_subtitleRenditions), [this, &ctr](shared_ptr<Rendition> rend)
    {
      return ref new HLSSubtitleRendition(_controller, ctr++, _bitratekey);
    });

  }
  return _subtitleRenditions;
}

void HLSVariantStream::LockBitrate()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  //cancel any pending changes 
  _controller->MediaSource->TryCancelPendingBitrateSwitch(true);

  //if the current bitrate is not this one
  if (!_controller->MediaSource->spRootPlaylist->Variants[_bitratekey]->IsActive)
  {

    auto controller = _controller;
    auto bwkey = _bitratekey;
    task<void>([controller, bwkey]() {
      //schedule the change
      bool Cancel = false;
      controller->MediaSource->SetBitrateLock(false);
      controller->MediaSource->BitrateSwitchSuggested(bwkey, bwkey, Cancel, true);
      controller->MediaSource->SetBitrateLock(true);

    });
  }
  else
    _controller->MediaSource->spHeuristicsManager->SetLastSuggestedBandwidth(_bitratekey);
}