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

#include <memory>
#include <mutex>
#include <deque>
#include <collection.h>
#include "MediaSegment.h" 
#include "HLSSegment.h"
#include "HLSController.h"
#include "HLSPlaylist.h"
#include "HLSInbandCCPayload.h"
#include "HLSID3MetadataStream.h"

using namespace Platform;
using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;


HLSSegment::~HLSSegment()
{
//  LOG("Segment RCW Destroyed : " << _sequenceNumber);
  return;
}

IHLSPlaylist^ HLSSegment::ParentPlaylist::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  return _playlist;
}
std::shared_ptr<MediaSegment> HLSSegment::FindMatch(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum)
{
  if (controller == nullptr || !controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  std::shared_ptr<MediaSegment> retval = nullptr;

  if (nullptr == controller ||
    nullptr == controller->MediaSource ||
    nullptr == controller->MediaSource->spRootPlaylist) return retval;

  std::shared_ptr<Playlist> targetPlaylist = nullptr;
  if (controller->MediaSource->spRootPlaylist->IsVariant == false)
  {
    targetPlaylist = controller->MediaSource->spRootPlaylist;
  }
  else
  {
    if (controller->MediaSource->spRootPlaylist->Variants.find(forBitrate) == controller->MediaSource->spRootPlaylist->Variants.end() ||
      controller->MediaSource->spRootPlaylist->Variants[forBitrate]->spPlaylist == nullptr)
      targetPlaylist = nullptr;
    else
      targetPlaylist = controller->MediaSource->spRootPlaylist->Variants[forBitrate]->spPlaylist;
  }
  if (targetPlaylist == nullptr)
    return retval;

  std::unique_lock<std::recursive_mutex> listlock(targetPlaylist->LockSegmentList,std::defer_lock);
  if (targetPlaylist->IsLive) listlock.lock();

  auto found = std::find_if(targetPlaylist->Segments.begin(), targetPlaylist->Segments.end(), [seqNum](std::shared_ptr<MediaSegment> mseg)
  {
    return mseg->SequenceNumber == seqNum;
  });
  if (found != targetPlaylist->Segments.end())
    retval = *found;

  return retval;
}

std::shared_ptr<MediaSegment> HLSSegment::FindMatch(HLSController^ controller, Rendition *pRendition, unsigned int seqNum)
{
  if (controller == nullptr || !controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  std::shared_ptr<MediaSegment> retval = nullptr;

  if (nullptr == controller ||
    nullptr == controller->MediaSource ||
    nullptr == controller->MediaSource->spRootPlaylist) return retval; 
 
  if (pRendition == nullptr || pRendition->spPlaylist == nullptr)
    return retval;

  
  std::unique_lock<std::recursive_mutex> listlock(pRendition->spPlaylist->LockSegmentList, std::defer_lock);
  if (pRendition->spPlaylist->IsLive) listlock.lock(); 

  auto found = std::find_if(pRendition->spPlaylist->Segments.begin(), pRendition->spPlaylist->Segments.end(), [seqNum](std::shared_ptr<MediaSegment> mseg)
  {
    return mseg->SequenceNumber == seqNum;
  });
  if (found != pRendition->spPlaylist->Segments.end())
    retval = *found;

  return retval;
}

HLSSegment::HLSSegment(HLSController^ controller, unsigned int bitrate, std::shared_ptr<MediaSegment> found) : _controller(controller)
{

  if (controller == nullptr || !controller->IsValid)
    throw ref new Platform::ObjectDisposedException();

  std::lock_guard<std::recursive_mutex> lock(found->LockSegment);

  _forBitrate = bitrate;
  _sequenceNumber = found->GetSequenceNumber();

  if (found->HasMediaType(ContentType::AUDIO) && found->HasMediaType(ContentType::VIDEO))
    _mediaType = TrackType::BOTH;
  else if (found->HasMediaType(ContentType::AUDIO))
    _mediaType = TrackType::AUDIO;
  else if (found->HasMediaType(ContentType::AUDIO))
    _mediaType = TrackType::VIDEO;

  _url = found->MediaUri;
  _segmentState = (found->GetCurrentState() == INMEMORYCACHE) ? SegmentState::LOADED : SegmentState::NOTLOADED;

  if (controller->MediaSource->spRootPlaylist->IsVariant)
    _playlist = ref new HLSPlaylist(controller, false, 0);
  else
    _playlist = ref new HLSPlaylist(controller, true, bitrate);

  if (found->UnresolvedTags.find(UnresolvedTagPlacement::PreSegment) != found->UnresolvedTags.end())
  {
    _unprocessedTagsPRE = ref new Platform::Collections::Vector<String^>((unsigned int) found->UnresolvedTags[UnresolvedTagPlacement::PreSegment].size());
    std::transform(found->UnresolvedTags[UnresolvedTagPlacement::PreSegment].begin(),
      found->UnresolvedTags[UnresolvedTagPlacement::PreSegment].end(),
      begin(_unprocessedTagsPRE), [](std::wstring tagdata) { return ref new String(tagdata.data()); });
  }
  if (found->UnresolvedTags.find(UnresolvedTagPlacement::PostSegment) != found->UnresolvedTags.end())
  {
    _unprocessedTagsPOST = ref new Platform::Collections::Vector<String^>((unsigned int) found->UnresolvedTags[UnresolvedTagPlacement::PostSegment].size());
    std::transform(found->UnresolvedTags[UnresolvedTagPlacement::PostSegment].begin(),
      found->UnresolvedTags[UnresolvedTagPlacement::PostSegment].end(),
      begin(_unprocessedTagsPOST), [](std::wstring tagdata) { return ref new String(tagdata.data()); });
  }
  if (found->UnresolvedTags.find(UnresolvedTagPlacement::WithSegment) != found->UnresolvedTags.end())
  {
    _unprocessedTagsWITHIN = ref new Platform::Collections::Vector<String^>((unsigned int) found->UnresolvedTags[UnresolvedTagPlacement::WithSegment].size());
    std::transform(found->UnresolvedTags[UnresolvedTagPlacement::WithSegment].begin(),
      found->UnresolvedTags[UnresolvedTagPlacement::WithSegment].end(),
      begin(_unprocessedTagsWITHIN), [](std::wstring tagdata) { return ref new String(tagdata.data()); });
  }

  if (_segmentState == SegmentState::LOADED){
    LoadCC(found);
    LoadMetadata(found);
  }

  //LOG("Segment RCW Created : " << _sequenceNumber);
}

SegmentState HLSSegment::LoadState::get()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  auto found = HLSSegment::FindMatch(_controller, _forBitrate, _sequenceNumber);
  if (found != nullptr)
    return found->GetCurrentState() == INMEMORYCACHE ? SegmentState::LOADED : SegmentState::NOTLOADED;
  else
    return SegmentState::UNAVAILABLE;
}

void HLSSegment::LoadCC(shared_ptr<MediaSegment> found)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_segmentState == SegmentState::LOADED && found->CCSamples.size() > 0)
  {
    _ccpayloads = ref new Platform::Collections::Vector<IHLSInbandCCPayload^>((unsigned int) found->CCSamples.size());
    std::transform(begin(found->CCSamples), end(found->CCSamples), begin(_ccpayloads), [found](shared_ptr<SampleData> sd)
    {
      return ref new HLSInbandCCPayload(found->pParentPlaylist->IsLive ? 
        (found->Discontinous == false ? sd->SamplePTS->ValueInTicks : (sd->DiscontinousTS != nullptr ? sd->DiscontinousTS->ValueInTicks : sd->SamplePTS->ValueInTicks)) :
        (found->Discontinous == false ? found->TSAbsoluteToRelative(sd->SamplePTS)->ValueInTicks : (sd->DiscontinousTS != nullptr ? found->TSAbsoluteToRelative(sd->DiscontinousTS)->ValueInTicks : found->TSAbsoluteToRelative(sd->SamplePTS)->ValueInTicks)),
        std::get<0>(*(sd->spInBandCC.get())), std::get<1>(*(sd->spInBandCC.get())));
    });
  }
  else
    _ccpayloads = nullptr;
}
void HLSSegment::LoadCC(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  auto found = HLSSegment::FindMatch(controller, forBitrate, seqNum);
  if (found != nullptr)
    LoadCC(found);
}

void HLSSegment::LoadMetadata(shared_ptr<MediaSegment> found)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  std::vector<HLSID3MetadataStream^> metadatastreams;
  for (auto itr = found->MetadataStreams.begin(); itr != found->MetadataStreams.end(); itr++)
  {
    HLSID3MetadataStream^ pStream = nullptr;
    auto pid = (*itr);
    std::vector<HLSID3MetadataPayload^> unreadunits;
    //push unread ones first
    if (found->UnreadQueues.find(pid) != found->UnreadQueues.end() && found->UnreadQueues[pid].size() > 0)
    {

      for (auto itr = found->UnreadQueues[pid].begin(); itr != found->UnreadQueues[pid].end(); itr++)
      {
        try{
          auto pld = ref new HLSID3MetadataPayload(
            found->pParentPlaylist->IsLive ? (found->Discontinous == false ? (*itr)->SamplePTS->ValueInTicks : ((*itr)->DiscontinousTS != nullptr ? (*itr)->DiscontinousTS->ValueInTicks : (*itr)->SamplePTS->ValueInTicks)) :
            (found->Discontinous == false ? found->TSAbsoluteToRelative((*itr)->SamplePTS)->ValueInTicks : ((*itr)->DiscontinousTS != nullptr ? found->TSAbsoluteToRelative((*itr)->DiscontinousTS)->ValueInTicks : found->TSAbsoluteToRelative((*itr)->SamplePTS)->ValueInTicks)),
            (*itr)->elemData);
          unreadunits.push_back(pld);
        }
        catch (InvalidArgumentException^ exInvalidArg)
        {

        }
        catch (...)
        {
        }
      }
    }
    std::vector<HLSID3MetadataPayload^> readunits;
    if (found->ReadQueues.find(pid) != found->ReadQueues.end() && found->ReadQueues[pid].size() > 0)
    {
      for (auto itr = found->ReadQueues[pid].begin(); itr != found->ReadQueues[pid].end(); itr++)
      {
        try{
          auto pld = ref new HLSID3MetadataPayload(
            found->pParentPlaylist->IsLive ? (found->Discontinous == false ? (*itr)->SamplePTS->ValueInTicks : ((*itr)->DiscontinousTS != nullptr ? (*itr)->DiscontinousTS->ValueInTicks : (*itr)->SamplePTS->ValueInTicks)) :
            (found->Discontinous == false ? found->TSAbsoluteToRelative((*itr)->SamplePTS)->ValueInTicks : ((*itr)->DiscontinousTS != nullptr ? found->TSAbsoluteToRelative((*itr)->DiscontinousTS)->ValueInTicks : found->TSAbsoluteToRelative((*itr)->SamplePTS)->ValueInTicks)),
            (*itr)->elemData);
          readunits.push_back(pld);
        }
        catch (...)
        {
        }
      }
    }


    if (readunits.size() == 0 && unreadunits.size() > 0)
      metadatastreams.push_back(ref new HLSID3MetadataStream(pid, unreadunits));
    else if (readunits.size() > 0 && unreadunits.size() == 0)
      metadatastreams.push_back(ref new HLSID3MetadataStream(pid, readunits));
    else if (readunits.size() > 0 && unreadunits.size() > 0)
    {
      std::vector<HLSID3MetadataPayload^> units;
      units.resize(readunits.size() + unreadunits.size());
      std::merge(begin(readunits), end(readunits), begin(unreadunits), end(unreadunits), begin(units), [this](HLSID3MetadataPayload^ cp1, HLSID3MetadataPayload^ cp2)
      {
        return cp1->Timestamp.Duration < cp2->Timestamp.Duration;
      });
      metadatastreams.push_back(ref new HLSID3MetadataStream(pid, units));
    }
  }

  if (metadatastreams.size() > 0)
  {
    _metadatastreams = ref new Platform::Collections::Vector<IHLSID3MetadataStream^>((unsigned int) metadatastreams.size());
    std::copy(begin(metadatastreams), end(metadatastreams), begin(_metadatastreams));
  }
  else
    _metadatastreams = nullptr;
}
void HLSSegment::LoadMetadata(HLSController^ controller, unsigned int forBitrate, unsigned int seqNum)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  auto found = HLSSegment::FindMatch(controller, forBitrate, seqNum);
  if (found != nullptr)
    LoadMetadata(found);
}

IVector<IHLSInbandCCPayload^>^ HLSSegment::GetInbandCCUnits()
{
  if (_controller == nullptr || !_controller->IsValid)
    throw ref new Platform::ObjectDisposedException();
  if (_ccpayloads == nullptr)
    LoadCC(_controller, _forBitrate, _sequenceNumber);

  return _ccpayloads;
}

IVector<IHLSID3MetadataStream^>^ HLSSegment::GetMetadataStreams()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (_metadatastreams == nullptr)
    LoadMetadata(_controller, _forBitrate, _sequenceNumber);

  return _metadatastreams;
}

IVector<Platform::String^>^ HLSSegment::GetUnprocessedTags(UnprocessedTagPlacement placement)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();
  if (placement == UnprocessedTagPlacement::PRE)
    return _unprocessedTagsPRE;
  else if (placement == UnprocessedTagPlacement::POST)
    return _unprocessedTagsPOST;
  else
    return _unprocessedTagsWITHIN;
}

Windows::Foundation::IAsyncAction^ HLSSegment::SetPIDFilter(Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ pidfilter)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();


  auto found = HLSSegment::FindMatch(_controller, this->_forBitrate, this->_sequenceNumber);
  if (found == nullptr)
    throw ref new Platform::ObjectDisposedException();

  shared_ptr<std::map<ContentType, unsigned short>> pfinternal = nullptr;

  if (pidfilter != nullptr)
  {
    pfinternal = make_shared<std::map<ContentType, unsigned short>>();
    if (pidfilter->Size > 0)
    {
      for (auto itm : pidfilter)
      {
        if (itm->Key != TrackType::AUDIO && itm->Key != TrackType::VIDEO && itm->Key != TrackType::METADATA)
          continue;
        pfinternal->emplace(std::pair<ContentType, unsigned short>(itm->Key == TrackType::AUDIO ? ContentType::AUDIO : 
          (itm->Key == TrackType::VIDEO ? ContentType::VIDEO : ContentType::METADATA), itm->Value));
      }
    }
  }
 
  return create_async(
    [found, pfinternal]()
  {
    found->SetPIDFilter(pfinternal);
  });
}

Windows::Foundation::IAsyncAction^ HLSSegment::ResetPIDFilter(TrackType forType)
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();


  auto found = HLSSegment::FindMatch(_controller, this->_forBitrate, this->_sequenceNumber);
  if (found == nullptr)
    throw ref new Platform::ObjectDisposedException();

  shared_ptr<std::map<ContentType, unsigned short>> pfinternal = nullptr; 

  return create_async(
    [found, forType]()
  {
    found->ResetPIDFilter(forType == TrackType::AUDIO ? ContentType::AUDIO : (forType == TrackType::VIDEO ? ContentType::VIDEO : ContentType::METADATA ));
  });
}

Windows::Foundation::IAsyncAction^ HLSSegment::ResetPIDFilter()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException(); 

  auto found = HLSSegment::FindMatch(_controller, this->_forBitrate, this->_sequenceNumber);
  if (found == nullptr)
    throw ref new Platform::ObjectDisposedException();

  shared_ptr<std::map<ContentType, unsigned short>> pfinternal = nullptr;

  return create_async(
    [found]()
  {
    found->ResetPIDFilter();
  });
}

Windows::Foundation::Collections::IMap<TrackType, unsigned short>^ HLSSegment::GetPIDFilter()
{
  if (_controller == nullptr || !_controller->IsValid)  throw ref new Platform::ObjectDisposedException();

  auto found = HLSSegment::FindMatch(_controller, this->_forBitrate, this->_sequenceNumber);
  if (found == nullptr)
    throw ref new Platform::ObjectDisposedException();
  auto pfinternal = found->GetPIDFilter();
  if(pfinternal.empty())
    return nullptr;

  Platform::Collections::Map<TrackType, unsigned short>^ ret = ref new Platform::Collections::Map<TrackType, unsigned short>();
  for (auto p : pfinternal)
    ret->Insert(p.first == ContentType::AUDIO ? TrackType::AUDIO : (p.first == ContentType::VIDEO ? TrackType::VIDEO : TrackType::METADATA), p.second);

  return ret;

}