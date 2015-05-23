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

#include <ppltasks.h>
#include <memory>

#include "Logger.h"
#include "CC608HtmlController.h"
#include "MockDataSource.h"
#include "RawCaptionDataInternal.h"

using namespace Microsoft::CC608;
using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;
using namespace std;
using namespace concurrency;

CC608HtmlController::CC608HtmlController(void) : _core(), _mtx()
{
}

void CC608HtmlController::InjectTestData()
{
  auto mock = MockDataSource();

  // add sample data
  //mock.TestPattern03_MidRowCodes();

  mock.RollUpSample();
  mock.RollUpSample();
  mock.RollUpSample();
  mock.RollUpSample();
  mock.RollUpSample();
  mock.RollUpSample();


  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  //mock.PopOnSample();
  
  // previously, the following sample data seemed to make the UI thread stop responding
  //mock.PopOnSample();
  //mock.RollUpSample();
  //mock.PaintOnSample();

  auto p = mock.GetNextPair();

  while (p.valid)
  {
    auto v = vector<byte_t>();
    v.push_back(p.byte1);
    v.push_back(p.byte2);

    _core.AddCaptionData(p.ts, v);

    p = mock.GetNextPair();
  }
}

Windows::Foundation::IAsyncAction^ CC608HtmlController::AddNewCaptionDataAsync(RawCaptionData^ data)
{
  return create_async([this, data]()
  {
    auto d = RawCaptionDataInternal(data);
    _core.AddCaptionData(d.Data);
  });
}

Windows::Foundation::IAsyncAction^ CC608HtmlController::AddNewCaptionDataInUserDataEnvelopeAsync(RawCaptionData^ data)
{
  return create_async([this, data]()
  {
    auto d = RawCaptionDataInternal(data);
    _core.AddCaptionDataInUserDataEnvelope(d.Data);
  });
}

Windows::Foundation::IAsyncAction^ CC608HtmlController::AddNewCaptionDataInUserDataEnvelopeAsync(Windows::Foundation::Collections::IMap<unsigned long long, const Platform::Array<byte_t>^>^ data)
{
  return create_async([this, data]()
  {
    auto d = RawCaptionDataInternal(data);
    _core.AddCaptionDataInUserDataEnvelope(d.Data);
  });
}

Windows::Foundation::IAsyncOperation<HtmlCaptionsData^>^ CC608HtmlController::GetHtmlCaptionsAsync(Windows::Foundation::TimeSpan currentPosition, uint16 videoHeightPixels)
{
  if ((videoHeightPixels <= 0) || (videoHeightPixels > 10000000))
  {
    throw ref new Platform::InvalidArgumentException("videoHeightPixels parameter to CC608HtmlController::GetHtmlCaptionsAsync() cannot be negative, zero, or more than 10,000,000.\nActual value: [" + videoHeightPixels + "].");
  }

  return create_async([this, currentPosition, videoHeightPixels]() { 

    // try to get the lock...
    unique_lock<mutex> modelLock(_mtx, try_to_lock);

    //...and if we don't, then just return rather than waiting (and getting further behind)
    if (!modelLock.owns_lock())
    {
      return ref new HtmlCaptionsData(false, L"");
    }
    
    Timestamp ts(currentPosition);

    // process
    _core.AdvanceModelTo(ts);

    if (_core.NeedsUpdate())
    {
      // render
      auto html = _core.GetCurrentHtml(videoHeightPixels);

      // return the new data
      return ref new HtmlCaptionsData(true, ref new Platform::String(html.c_str()));
    }

    // no visual updates since last render
    return ref new HtmlCaptionsData(false, L"");
  });
}

void CC608HtmlController::Seek(Windows::Foundation::TimeSpan currentPosition)
{
  lock_guard<mutex> modelLock(_mtx);

  Timestamp ts(currentPosition);
  _core.Seek(ts);
}

void CC608HtmlController::Reset()
{
  lock_guard<mutex> modelLock(_mtx);

  _core.Reset();
}
