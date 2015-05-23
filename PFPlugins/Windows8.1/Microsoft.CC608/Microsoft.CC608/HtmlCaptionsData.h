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

namespace Microsoft { namespace CC608 {

// Simple ref class for returning HTML caption data to the js plugin, with a flag to tell if there is data or not
public ref class HtmlCaptionsData sealed
{
public:
  HtmlCaptionsData(bool requiresUpdate, Platform::String^ markup) : _requiresUpdate(requiresUpdate), _markup(markup)
  {
  }

  property Platform::Boolean RequiresUpdate
  {
    Platform::Boolean get() { return _requiresUpdate; }
  }

  property Platform::String^ CaptionMarkup
  {
    Platform::String^ get() { return _markup; }
  }

private:
  bool _requiresUpdate;
  Platform::String^ _markup;
};

}}

