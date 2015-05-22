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
#include "pch.h"
#include "Core.h"
#include "Logger.h"

#include "ByteDecoder.h"

using namespace Microsoft::CC608;
using namespace std;

Core::Core() : _htmlRenderer(), _xamlRenderer(), _upDecoder(nullptr), 
_spModel(make_shared<Model>()), _currentPosition(0), _previousDisplayVersionNumber(0), _captionTrack(0), Options(nullptr)
{
  _upDecoder = unique_ptr<ByteDecoder>(new ByteDecoder(_spModel));
}

Core::~Core(void)
{
}

void Core::AddCaptionData(const Timestamp ts, const std::vector<byte_t>& byteCodes)
{
  _queue.AddCaptionData(ts, byteCodes);
}

void Core::AddCaptionData(const std::vector<std::pair<Timestamp, std::vector<byte_t>>>& data)
{
  _queue.AddCaptionData(data);
}

void Core::AddCaptionDataInUserDataEnvelope(const Timestamp ts, const std::vector<byte_t>& bytes)
{
  auto byteCodes = _upDecoder->Extract608ByteCodes(bytes);

  _queue.AddCaptionData(ts, byteCodes);
}

void Core::AddCaptionDataInUserDataEnvelope(const std::vector<std::pair<Timestamp, std::vector<byte_t>>>& data)
{
 vector<pair<Timestamp, vector<byte_t>>> v;

  for(const auto& d : data)
  {
    //LogRawData(d);

    Timestamp ts(d.first);
    auto byteCodes = _upDecoder->Extract608ByteCodes(d.second);
    v.emplace_back(make_pair(ts, byteCodes));
  }

  _queue.AddCaptionData(v);
}

void Core::Seek(const Timestamp seekPosition)
{
  _currentPosition = seekPosition;
  _spModel->Clear();
  _queue.Clear();
  _previousDisplayVersionNumber = 0;
}

void Core::Reset()
{
  _currentPosition = Timestamp();
  _queue.Clear();
  _spModel->Clear();
  _previousDisplayVersionNumber = 0;
}

void Core::AdvanceModelTo(const Timestamp newPosition)
{
  auto bytes = _queue.GetSortedCaptionData(_currentPosition, newPosition);

  _upDecoder->ParseBytes(bytes);

  _currentPosition = newPosition;
}

bool Core::NeedsUpdate() const
{
  if (_previousDisplayVersionNumber == 0)
  {
    // not yet updated
    return true;
  }

  // only update if the model has a different version number than the last rendered value
  return (_previousDisplayVersionNumber != _spModel->GetDisplayVersionNumber());
}

std::wstring Core::GetCurrentHtml(const unsigned short videoHeightPixels)
{
  _previousDisplayVersionNumber = _spModel->GetDisplayVersionNumber();
  return _htmlRenderer.RenderHtml(_spModel->displayedMemory, videoHeightPixels, _spModel->NeedsAnimation());
}

Windows::UI::Xaml::UIElement^ Core::GetCurrentXaml(const unsigned short videoHeightPixels)
{
	_xamlRenderer.SetOptions(this->Options);

  _previousDisplayVersionNumber = _spModel->GetDisplayVersionNumber();
  return _xamlRenderer.RenderXaml(_spModel->displayedMemory, videoHeightPixels, _spModel->NeedsAnimation());
}

void Core::SetCaptionTrack(const int captionTrack)
{
  assert(captionTrack >= 0 && captionTrack < 5);

  if (_captionTrack == captionTrack)
  {
    // no change--do not clear any data
    return;
  }

  // if switching from one field to another, need to clear the queue of any data, as this data will be invalid
  // (CC1 and CC2 are field 1, while CC3 and CC4 are field 2)
  switch(captionTrack)
  {
  case 0:
    // nothing to do here
    break;

  case 1:
  case 2:
    if (_captionTrack > 2)
    {
      // switching from field 2 to field 1
      
      // all data in the queue is now worthless
      _queue.Clear();
    }
    break;

  case 3:
  case 4:
    if (_captionTrack < 3)
    {
      // switching from field 1 to field 2

      // clear cache (as all data is now incorrect)
      _queue.Clear();
    }
    break;
  }

  // regardless of field, existing data in model is now invalid
  _spModel->Clear();
  _previousDisplayVersionNumber = 0;

  _captionTrack = captionTrack;
  _upDecoder->SetCaptionTrack(captionTrack);
}

void Core::LogRawData(const std::pair<Timestamp, std::vector<byte_t>>& d) const
{
  unsigned int displayCounter = 0;
  wstringstream ss;

  for(const byte_t b : d.second)
  {
    if ((displayCounter != 0) && ((displayCounter % 4) == 0))
    {
      // visually separate every four bytes
      ss << " ";
    }

    if (b < 0x10)
    {
      // add the leading zero if necessary
      ss << "0" << std::hex << b;
    }
    else
    {
      ss << std::hex << b;
    }
    ++displayCounter;
  }

  DebugWrite(L"RAW 608 DATA FOLLOWS FOR TIMESTAMP: " << d.first.GetTicks() << " (Expected Header: 0x47413934) " << ss.str());
}