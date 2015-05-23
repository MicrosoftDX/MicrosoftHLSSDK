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

#include <collection.h>

namespace Microsoft { namespace CC608 {

// Not for use by end-users! 
// This is a simple data-holder class for 608 caption data at a specific timestamp. It is used by the RawCaptionData component.
// This class can accept byte code data as either an IPropertyValue (with a UInt8 array) or a Vector<byte_t>.
public ref class RawCaptionDataSubset sealed
{
public:
  RawCaptionDataSubset(unsigned long long ts, const Platform::Array<byte_t>^ bytes);

  unsigned long long GetTs();
  Platform::Array<byte_t>^ GetBytes();

private:
  unsigned long long _ts;
  Platform::Array<byte_t>^ _bytes;
};


}}