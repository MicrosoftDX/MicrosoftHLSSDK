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
#include "HtmlCaptionsData.h"
#include "XamlCaptionsData.h"
#include "Core.h"
#include "RawCaptionData.h"

#include <collection.h>

namespace Microsoft { namespace CC608 {

  // Public interface of the component for HTML apps
  public ref class CC608HtmlController sealed
  {
  public:
    CC608HtmlController(void);

    void InjectTestData();

    // Add caption data to process and save for later display
    Windows::Foundation::IAsyncAction^ AddNewCaptionDataAsync(RawCaptionData^ data);
    
    [Windows::Foundation::Metadata::DefaultOverload]
    Windows::Foundation::IAsyncAction^ AddNewCaptionDataInUserDataEnvelopeAsync(RawCaptionData^ data);

    // used to get HTML data for captions at a given point in time
    Windows::Foundation::IAsyncOperation<Microsoft::CC608::HtmlCaptionsData^>^ GetHtmlCaptionsAsync(Windows::Foundation::TimeSpan currentPosition, uint16 videoHeightPixels);

    // used when seeking
    void Seek(Windows::Foundation::TimeSpan currentPosition);

    // use when switching media sources
    void Reset();

    // valid values are 1, 2, 3, and 4 (for CC1, CC2, CC3, and CC4), as well as 0 (zero) for no captions
    property int ActiveCaptionTrack
    {
      int get() { return _core.GetCaptionTrack(); }
      void set(int t) 
      { 
        if ((t < 0) || (t > 4))
        {
          throw ref new Platform::InvalidArgumentException("ActiveCaptionTrack property on CC608HtmlController must be 0 (for no captions) or between 1 and 4 (for CC1 through CC4).\nValue encountered: [" + t + "].");
        }

        _core.SetCaptionTrack(t);
      }
    }

  private:
    Core _core;
    std::mutex _mtx;
	Windows::Foundation::IAsyncAction^ AddNewCaptionDataInUserDataEnvelopeAsync(Windows::Foundation::Collections::IMap<unsigned long long, const Platform::Array<byte>^>^ data);
  };

}}