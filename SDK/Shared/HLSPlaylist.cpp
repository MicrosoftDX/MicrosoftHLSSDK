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

#include <string>
#include <algorithm>
#include <collection.h> 
#include "Playlist.h" 
#include "HLSController.h"
#include "HLSPlaylist.h"
#include "HLSVariantStream.h"
#include "HLSSegment.h"
#include "HLSSlidingWindow.h"
#include "HLSBitrateSwitchEventArgs.h"
#include "HLSSegmentSwitchEventArgs.h"
#include "HLSStreamSelectionChangedEventArgs.h" 


using namespace Platform;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;


HLSPlaylist::HLSPlaylist(HLSController^ controller, bool IsVariantChild, unsigned int bitratekey) :
_controller(controller), _bitratekey(IsVariantChild ? new unsigned int(bitratekey) : nullptr)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  if (_variants.size() == 0 && !IsVariantChild)
  {
    for (auto itr = _controller->MediaSource->spRootPlaylist->Variants.begin(); itr != _controller->MediaSource->spRootPlaylist->Variants.end(); itr++)
      _variants.push_back(ref new HLSVariantStream(_controller, (*itr).first, true));
  }
}
bool HLSPlaylist::IsMaster::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return (_bitratekey != nullptr) ? _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->IsVariant :
    _controller->MediaSource->spRootPlaylist->IsVariant;
}

String^ HLSPlaylist::Url::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return (_bitratekey != nullptr) ? ref new String((_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->BaseUri +
    _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->FileName).data()) :
    ref new String((_controller->MediaSource->spRootPlaylist->BaseUri + _controller->MediaSource->spRootPlaylist->FileName).data());
}

bool HLSPlaylist::IsLive::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  return (_bitratekey != nullptr) ? _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->IsLive :
    _controller->MediaSource->spRootPlaylist->IsLive;
}

IHLSVariantStream^ HLSPlaylist::ActiveVariantStream::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_bitratekey != nullptr)
    return nullptr;
  if (_controller->MediaSource->spRootPlaylist->ActiveVariant == nullptr)
    return nullptr;

  auto br = _controller->MediaSource->spRootPlaylist->ActiveVariant->Bandwidth;
  auto found = std::find_if(begin(_variants), end(_variants), [br](HLSVariantStream^ stm) { return stm->Bitrate == br; });
  if (found != end(_variants))
    return *found;
  else
    return nullptr;
}

unsigned int HLSPlaylist::MaximumAllowedBitrate::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  return _controller->MediaSource->spRootPlaylist->MaxAllowedBitrate;
}

void HLSPlaylist::MaximumAllowedBitrate::set(unsigned int Bitrate)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  if (Bitrate < _controller->MediaSource->spRootPlaylist->MinAllowedBitrate)
    throw ref new InvalidArgumentException();
  _controller->MediaSource->spRootPlaylist->MaxAllowedBitrate = Bitrate;
  //set bandwidth bounds
  _controller->MediaSource->spHeuristicsManager->SetBandwidthBounds(Bitrate, _controller->MediaSource->spRootPlaylist->MinAllowedBitrate);
}

unsigned int HLSPlaylist::MinimumAllowedBitrate::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  return _controller->MediaSource->spRootPlaylist->MinAllowedBitrate;
}

void HLSPlaylist::MinimumAllowedBitrate::set(unsigned int Bitrate)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  if (Bitrate > _controller->MediaSource->spRootPlaylist->MaxAllowedBitrate)
    throw ref new InvalidArgumentException();
  _controller->MediaSource->spRootPlaylist->MinAllowedBitrate = Bitrate;
  //set bandwidth bounds
  _controller->MediaSource->spHeuristicsManager->SetBandwidthBounds(_controller->MediaSource->spRootPlaylist->MaxAllowedBitrate, Bitrate);
}

unsigned int HLSPlaylist::StartBitrate::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  return _controller->MediaSource->spRootPlaylist->StartBitrate;
}

void HLSPlaylist::StartBitrate::set(unsigned int Bitrate)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant == false)
    throw ref new NotImplementedException();
  _controller->MediaSource->spRootPlaylist->StartBitrate = Bitrate;
}

unsigned int HLSPlaylist::BaseSequenceNumber::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant)
  {
    if (nullptr == _bitratekey)
      throw ref new NotImplementedException();
    if (_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist == nullptr)
      throw ref new NotImplementedException();
    return _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->BaseSequenceNumber;
  }
  else
  {
    return _controller->MediaSource->spRootPlaylist->BaseSequenceNumber;
  }
}

Windows::Foundation::TimeSpan HLSPlaylist::DerivedTargetDuration::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant)
  {
    if (nullptr == _bitratekey)
      throw ref new NotImplementedException();
    if (_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist == nullptr)
      throw ref new NullReferenceException();
    return Windows::Foundation::TimeSpan{ (long long)(_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->DerivedTargetDuration) };
  }
  else
  {
    return Windows::Foundation::TimeSpan{ (long long)(_controller->MediaSource->spRootPlaylist->DerivedTargetDuration) };
  }
}

Windows::Foundation::TimeSpan HLSPlaylist::PlaylistTargetDuration::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant)
  {
    if (nullptr == _bitratekey)
      throw ref new NotImplementedException();
    if (_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist == nullptr)
      throw ref new NullReferenceException();
    return Windows::Foundation::TimeSpan{ (long long)(_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->PlaylistTargetDuration) };
  }
  else
  {
    return Windows::Foundation::TimeSpan{ (long long)(_controller->MediaSource->spRootPlaylist->PlaylistTargetDuration) };
  }
}

unsigned int HLSPlaylist::SegmentCount::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant)
  {
    if (nullptr == _bitratekey)
      throw ref new NotImplementedException();
    if (_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist == nullptr)
      throw ref new NullReferenceException();
    return (unsigned int)_controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->Segments.size();
  }
  else
  {
    return (unsigned int)_controller->MediaSource->spRootPlaylist->Segments.size();
  }
}

HLSPlaylistType HLSPlaylist::PlaylistType::get()
{
  if (nullptr == _controller || nullptr == _controller->MediaSource || nullptr == _controller->MediaSource->spRootPlaylist)
    throw ref new NullReferenceException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant && _bitratekey != nullptr)
  {
    auto playlist = _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist;
    if (playlist == nullptr)
      throw ref new NullReferenceException();
    return playlist->PlaylistType;
  }
  else
  {
    return _controller->MediaSource->spRootPlaylist->PlaylistType;
  }
}

IHLSVariantStream^ HLSPlaylist::ParentStream::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (nullptr == _bitratekey)
    throw ref new NotImplementedException();
  return ref new HLSVariantStream(_controller, *_bitratekey, true);
}

IHLSSlidingWindow^ HLSPlaylist::SlidingWindow::get()
{

  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  IHLSSlidingWindow^ retval = nullptr;
  if (_controller->MediaSource->spRootPlaylist->IsLive == false)
    return nullptr;
  if (_controller->MediaSource->GetCurrentState() != MSS_STARTED)
    return nullptr;

  if (_bitratekey != nullptr)
  {
    if (_controller->MediaSource->spRootPlaylist->ActiveVariant == nullptr || _controller->MediaSource->spRootPlaylist->ActiveVariant->Bandwidth != *_bitratekey)
      return nullptr;
    auto _start = _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->GetSlidingWindowStart();
    auto _end = _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->GetSlidingWindowEnd();
    auto _livepos = _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->FindApproximateLivePosition();
    if (_start == nullptr || _end == nullptr || _livepos == nullptr)
      return nullptr;

    return ref new HLSSlidingWindow(_start->ValueInTicks, _end->ValueInTicks, _livepos->ValueInTicks);
  }
  else if (_controller->MediaSource->spRootPlaylist->IsVariant && _controller->MediaSource->spRootPlaylist->ActiveVariant != nullptr && _controller->MediaSource->spRootPlaylist->ActiveVariant->spPlaylist != nullptr) // this is the root playlist - is it a variant container ? 
  {
    auto _start = _controller->MediaSource->spRootPlaylist->ActiveVariant->spPlaylist->GetSlidingWindowStart();
    auto _end = _controller->MediaSource->spRootPlaylist->ActiveVariant->spPlaylist->GetSlidingWindowEnd();
    auto _livepos = _controller->MediaSource->spRootPlaylist->ActiveVariant->spPlaylist->FindApproximateLivePosition();
    if (_start == nullptr || _end == nullptr || _livepos == nullptr)
      return nullptr;
    return ref new HLSSlidingWindow(_start->ValueInTicks, _end->ValueInTicks, _livepos->ValueInTicks);
  }
  else //this is a media playlist - but live
  {
    auto _start = _controller->MediaSource->spRootPlaylist->GetSlidingWindowStart();
    auto _end = _controller->MediaSource->spRootPlaylist->GetSlidingWindowEnd();
    auto _livepos = _controller->MediaSource->spRootPlaylist->FindApproximateLivePosition();
    if (_start == nullptr || _end == nullptr || _livepos == nullptr)
      return nullptr;
    return ref new HLSSlidingWindow(_start->ValueInTicks, _end->ValueInTicks, _livepos->ValueInTicks);
  }


  return nullptr;
}

Windows::Foundation::Collections::IVector<Windows::Foundation::TimeSpan>^ HLSPlaylist::GetPlaylistBatchItemDurations()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_bitratekey != nullptr || _controller->MediaSource->spRootPlaylist->PlaylistBatch.size() == 0)
    return nullptr;

  auto retval = ref new Platform::Collections::Vector<Windows::Foundation::TimeSpan, TSEqual>((unsigned int)_controller->MediaSource->spRootPlaylist->PlaylistBatch.size() + 1);
  unsigned long long TotDur = 0;

  std::transform(begin(_controller->MediaSource->spRootPlaylist->PlaylistBatch),
    end(_controller->MediaSource->spRootPlaylist->PlaylistBatch), begin(retval) + 1, [this, &TotDur](std::pair<wstring, shared_ptr<Playlist>> itm)
  {
    TotDur += itm.second->TotalDuration;
    return Windows::Foundation::TimeSpan{ (long long)itm.second->TotalDuration };
  });

  retval->SetAt(0, Windows::Foundation::TimeSpan{ (long long)(_controller->MediaSource->spRootPlaylist->TotalDuration - TotDur) });
  return retval;
}

Windows::Foundation::Collections::IVector<IHLSVariantStream^>^ HLSPlaylist::GetVariantStreams()
{

  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_bitratekey != nullptr || _controller->MediaSource->spRootPlaylist->Variants.size() == 0)
    return nullptr;


  auto retval = ref new Platform::Collections::Vector<IHLSVariantStream^>((unsigned int)_variants.size());

  if (_variants.size() > 0)
    std::copy(begin(_variants), end(_variants), begin(retval));
  return retval;
}

void HLSPlaylist::ResetBitrateLock()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_controller->MediaSource->spRootPlaylist->IsVariant)
    _controller->MediaSource->SetBitrateLock(false);
  return;
}

void HLSPlaylist::SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  shared_ptr<std::map<ContentType, unsigned short>> sppidfilter = nullptr;

  if (pidfilter != nullptr)
  {
    sppidfilter = make_shared<std::map<ContentType, unsigned short>>();
    for (auto itm : pidfilter)
    {
      if (itm->Key != TrackType::AUDIO && itm->Key != TrackType::VIDEO && itm->Key != TrackType::METADATA)
        throw ref new InvalidArgumentException();
      else
        sppidfilter->emplace(std::pair<ContentType, unsigned short>(itm->Key == TrackType::AUDIO ? ContentType::AUDIO :
        (itm->Key == TrackType::VIDEO ? ContentType::VIDEO : ContentType::METADATA), itm->Value));
    }
  }

  if (_bitratekey == nullptr)
  {
    _controller->MediaSource->spRootPlaylist->SetPIDFilter(sppidfilter);
  }
  else
  {
    _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->SetPIDFilter(sppidfilter);
  }
}



void HLSPlaylist::ResetPIDFilter(TrackType forType)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_bitratekey == nullptr)
  {
    _controller->MediaSource->spRootPlaylist->ResetPIDFilter(forType == TrackType::AUDIO ? ContentType::AUDIO :
      (forType == TrackType::VIDEO ? ContentType::VIDEO : ContentType::METADATA));
  }
  else
  {
    _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->ResetPIDFilter(forType == TrackType::AUDIO ? ContentType::AUDIO :
      (forType == TrackType::VIDEO ? ContentType::VIDEO : ContentType::METADATA));
  }
}

void HLSPlaylist::ResetPIDFilter()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_bitratekey == nullptr)
  {
    _controller->MediaSource->spRootPlaylist->ResetPIDFilter();
  }
  else
  {
    _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->ResetPIDFilter();
  }
}

Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ HLSPlaylist::GetPIDFilter()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  std::map<ContentType, unsigned short> pfinternal;

  if (_bitratekey == nullptr)
  {
    pfinternal = _controller->MediaSource->spRootPlaylist->GetPIDFilter();
  }
  else
  {
    pfinternal = _controller->MediaSource->spRootPlaylist->Variants[*_bitratekey]->spPlaylist->GetPIDFilter();
  }

  if (pfinternal.empty())
    return nullptr;

  Platform::Collections::Map<TrackType, unsigned short>^ ret = ref new Platform::Collections::Map<TrackType, unsigned short>();
  for (auto p : pfinternal)
    ret->Insert(p.first == ContentType::AUDIO ? TrackType::AUDIO : (p.first == ContentType::VIDEO ? TrackType::VIDEO : TrackType::METADATA), p.second);

  return ret;

}

IHLSSegment^ HLSPlaylist::GetSegmentBySequenceNumber(unsigned int SequenceNumber)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  auto found = HLSSegment::FindMatch(_controller, *_bitratekey, SequenceNumber);
  if (found == nullptr)
    return nullptr;
  else
    return ref new HLSSegment(_controller, *_bitratekey, found);
}

void HLSPlaylist::RaiseBitrateSwitchSuggested(unsigned int From, unsigned int To, unsigned int LastMeasured, bool& Cancel)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (_bitratekey != nullptr) return;
  try
  {
    HLSBitrateSwitchEventArgs^ args = ref new HLSBitrateSwitchEventArgs(From, To, LastMeasured, TrackType::BOTH);
    _BitrateSwitchSuggested(this, args);
    Cancel = args->Cancel;
  }
  catch (...)
  {
  }

}
///<summary>Raise bitrate switch completed event</summary> 
void HLSPlaylist::RaiseBitrateSwitchCompleted(ContentType type, unsigned int From, unsigned int To)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (_bitratekey != nullptr) return;
  try
  {
    _BitrateSwitchCompleted(this, ref new HLSBitrateSwitchEventArgs(From, To, To, type == ContentType::AUDIO ? TrackType::AUDIO : TrackType::VIDEO));
  }
  catch (...)
  {
  }
}

///<summary>Raise bitrate switch cancelled event</summary> 
void HLSPlaylist::RaiseBitrateSwitchCancelled(unsigned int From, unsigned int To)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (_bitratekey != nullptr) return;
  try
  {
    _BitrateSwitchCancelled(this, ref new HLSBitrateSwitchEventArgs(From, To, To, TrackType::BOTH));
  }
  catch (...)
  {
  }
}

///<summary>Raise bitrate switch cancelled event</summary> 
void HLSPlaylist::RaiseStreamSelectionChanged(TrackType from, TrackType to)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (_bitratekey != nullptr) return;
  try
  {
    _StreamSelectionChanged(this, ref new HLSStreamSelectionChangedEventArgs(from, to));
  }
  catch (...)
  {
  }
}

void HLSPlaylist::RaiseSegmentSwitched(Playlist *from, Playlist *to, unsigned int fromseq, unsigned int toseq)
{
  if (_controller == nullptr || !_controller->IsValid)  return;

  std::shared_ptr<MediaSegment> foundFromSeg = nullptr;
  std::shared_ptr<MediaSegment> foundToSeg = nullptr;
  try
  {
    if (from != nullptr && from->pParentRendition != nullptr)
      foundFromSeg = HLSSegment::FindMatch(_controller, from->pParentRendition, fromseq);
    else
      foundFromSeg = HLSSegment::FindMatch(_controller, (from == nullptr || from->pParentStream == nullptr) ? 0 : from->pParentStream->Bandwidth, fromseq);
    if (to != nullptr && to->pParentRendition != nullptr)
      foundToSeg = HLSSegment::FindMatch(_controller, to->pParentRendition, toseq);
    else
      foundToSeg = HLSSegment::FindMatch(_controller, (to == nullptr || to->pParentStream == nullptr) ? 0 : to->pParentStream->Bandwidth, toseq);
    HLSSegmentSwitchEventArgs^ args = ref new HLSSegmentSwitchEventArgs(from != nullptr && from->pParentStream != nullptr ? from->pParentStream->Bandwidth : 0,
      to != nullptr && to->pParentStream != nullptr ? to->pParentStream->Bandwidth : 0,
      from != nullptr && foundFromSeg != nullptr ? ref new HLSSegment(_controller, from->pParentStream == nullptr ? 0 : from->pParentStream->Bandwidth, foundFromSeg) : nullptr,
      to != nullptr ? ref new HLSSegment(_controller, to->pParentStream == nullptr ? 0 : to->pParentStream->Bandwidth, foundToSeg) : nullptr);

    _SegmentSwitched(this, args);
  }
  catch (...)
  {

  }
}

void HLSPlaylist::RaiseSegmentDataLoaded(Playlist *from, unsigned int forseq)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (from == nullptr) return;
  std::shared_ptr<MediaSegment> foundFromSeg = nullptr;
  try
  {
    if (from != nullptr && from->pParentRendition != nullptr)
      foundFromSeg = HLSSegment::FindMatch(_controller, from->pParentRendition, forseq);
    else
      foundFromSeg = HLSSegment::FindMatch(_controller, from->pParentStream == nullptr ? 0 : from->pParentStream->Bandwidth, forseq);
    if (foundFromSeg != nullptr)
      _SegmentDataLoaded(this, ref new HLSSegment(_controller, from->pParentStream == nullptr ? 0 : from->pParentStream->Bandwidth, foundFromSeg));
  }
  catch (...)
  {

  }
}

void HLSPlaylist::RaiseSlidingWindowChanged(Playlist *from)
{
  if (_controller == nullptr || !_controller->IsValid)  return;
  if (_controller->MediaSource->GetCurrentState() != MSS_STARTED) return;
  if (from == nullptr) return;
  try
  {
    auto _start = from->GetSlidingWindowStart();
    auto _end = from->GetSlidingWindowEnd();
    auto _livepos = from->FindApproximateLivePosition();
    _SlidingWindowChanged(this, (_start == nullptr || _end == nullptr || _livepos == nullptr) ? nullptr : ref new HLSSlidingWindow(_start->ValueInTicks, _end->ValueInTicks, _livepos->ValueInTicks));
  }
  catch (...)
  {
  }
}