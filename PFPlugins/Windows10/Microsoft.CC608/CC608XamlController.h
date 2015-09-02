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

#include "pch.h"
#include "XamlCaptionsData.h"
#include "Core.h"
#include "RawCaptionData.h"
#include "CaptionOptions.h"

namespace Microsoft { namespace CC608 {

// Public interface of the component for XAML apps
[Windows::Foundation::Metadata::WebHostHidden]
public ref class CC608XamlController sealed
{
public:
  CC608XamlController(void);

  void InjectTestData();

  // Add caption data to process and save for later display
  Windows::Foundation::IAsyncAction^ AddNewCaptionDataAsync(RawCaptionData^ data);
  Windows::Foundation::IAsyncAction^ AddNewCaptionDataInUserDataEnvelopeAsync(RawCaptionData^ data);

  // Updates the model and checks if a render is required (runs async on background thread)
  Windows::Foundation::IAsyncOperation<bool>^ CaptionUpdateRequiredAsync(Windows::Foundation::TimeSpan currentPosition);

  // Renders model and returns XAML tree (runs sync and should be called on UI thread)
  Microsoft::CC608::XamlCaptionsData^ GetXamlCaptions(uint16 videoHeightPixels);

  // used when seeking
  void Seek(Windows::Foundation::TimeSpan currentPosition);

  // use when switching media sources
  void Reset();

  // Caption Options
  property CaptionOptions^ Options
  {
	  CaptionOptions^ get()
	  {
		  return options;
	  }

	  void set(CaptionOptions^ value)
	  {
		  options = value;

		  _core.Options = value;
	  }
  }

  // valid values are 1, 2, 3, and 4 (for CC1, CC2, CC3, and CC4), as well as 0 (zero) for no captions
  property int ActiveCaptionTrack
  {
    int get() { return _core.GetCaptionTrack(); }
    void set(int t) 
    {
      if ((t < 0) || (t > 4))
      {
        throw ref new Platform::InvalidArgumentException("ActiveCaptionTrack property on CC608XamlController must be 0 (for no captions) or between 1 and 4 (for CC1 through CC4).\nValue encountered: [" + t + "].");
      }

      _core.SetCaptionTrack(t);
    }
  }

private:
  Core _core;
  std::mutex _mtx;
  Microsoft::CC608::CaptionOptions^ options;

  [Windows::Foundation::Metadata::DefaultOverload]
  Windows::Foundation::IAsyncAction^ AddNewCaptionDataInUserDataEnvelopeAsync(Windows::Foundation::Collections::IMap<unsigned long long, Windows::Foundation::Collections::IVector<byte>^>^ map);
};

}}