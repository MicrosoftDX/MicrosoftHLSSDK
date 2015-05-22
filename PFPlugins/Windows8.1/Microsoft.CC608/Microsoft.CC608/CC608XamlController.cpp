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

#include "CC608XamlController.h"
#include "MockDataSource.h"
#include "RawCaptionDataInternal.h"

using namespace Microsoft::CC608;
using namespace Windows::UI::Xaml::Controls;

using namespace std;
using namespace concurrency;

CC608XamlController::CC608XamlController(void) : _core(), _mtx(), options(nullptr)
{
}

void CC608XamlController::InjectTestData()
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

Windows::Foundation::IAsyncAction^ CC608XamlController::AddNewCaptionDataAsync(RawCaptionData^ data)
{
  return create_async([this, data]()
  {
    RawCaptionDataInternal d(data);
    _core.AddCaptionData(d.Data);
  });
}

Windows::Foundation::IAsyncAction^ CC608XamlController::AddNewCaptionDataInUserDataEnvelopeAsync(RawCaptionData^ data)
{
  return create_async([this, data]()
  {
    RawCaptionDataInternal d(data);
    _core.AddCaptionDataInUserDataEnvelope(d.Data);
  });
}

Windows::Foundation::IAsyncAction^ CC608XamlController::AddNewCaptionDataInUserDataEnvelopeAsync(Windows::Foundation::Collections::IMap<unsigned long long, Windows::Foundation::Collections::IVector<byte_t>^>^ map)
{
  return create_async([this, map]()
  {
    RawCaptionDataInternal d(map);
    _core.AddCaptionDataInUserDataEnvelope(d.Data);
  });
}

Windows::Foundation::IAsyncOperation<bool>^ CC608XamlController::CaptionUpdateRequiredAsync(Windows::Foundation::TimeSpan currentPosition)
{
  // this method runs async on a background thread (as it does not need to manipulate UI elements)
  return create_async([this, currentPosition]() {

    // try to get the lock... (may not work--"try_to_lock")
    unique_lock<mutex> modelLock(_mtx, try_to_lock);

    // ...and if we don't, return immediately
    if (!modelLock.owns_lock())
    {
      return false;
    }
  
    Timestamp ts(currentPosition);

    // process
    _core.AdvanceModelTo(ts);

    return _core.NeedsUpdate();
  });
}

Microsoft::CC608::XamlCaptionsData^ CC608XamlController::GetXamlCaptions(uint16 videoHeightPixels)
{
  // this method needs to be called on the UI thread, as it manipulates UI elements
  // (because of this, it cannot be async)

  if ((videoHeightPixels <= 0) || (videoHeightPixels > 10000000))
  {
    throw ref new Platform::InvalidArgumentException("videoHeightPixels parameter to CC608XamlController::GetXamlCaptions() cannot be negative, zero, or more than 10,000,000.\nActual value: [" + videoHeightPixels + "].");
  }

  // try to get the lock... (may not work--"try_to_lock")
  unique_lock<mutex> modelLock(_mtx, try_to_lock);

  // ...and if we don't, return immediately
  if (!modelLock.owns_lock())
  {
    return ref new XamlCaptionsData(false, nullptr);
  }
  
  // given the locking, the update may have already been displayed and not be needed now
  if (!_core.NeedsUpdate())
  {
    return ref new XamlCaptionsData(false, nullptr);
  }

  // render
  return ref new XamlCaptionsData(true, _core.GetCurrentXaml(videoHeightPixels));
}

void CC608XamlController::Seek(Windows::Foundation::TimeSpan currentPosition)
{
  lock_guard<mutex> modelLock(_mtx);

  Timestamp ts(currentPosition);

  _core.Seek(ts);
}

void CC608XamlController::Reset()
{
  lock_guard<mutex> modelLock(_mtx);

  _core.Reset();
}
