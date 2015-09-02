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

#include <collection.h>

#include "RawCaptionDataSubset.h"

namespace Microsoft { namespace CC608 {

// Simple class to use for passing raw caption data across the ABI to the plugin. 
// Can accept data either as an IPropertyValue (UInt8 array) or as an IVector<UInt8>.
//
// This class should be instantiated in the JS or C# code that gets the data, and then the AddByXXX methods should be used to load up all the data available, 
// and then it should be passed to the CC608XamlController or CC608HtmlController for processing.
public ref class RawCaptionData sealed
{
public:
  RawCaptionData(void);
  RawCaptionData(unsigned long long ts, const Platform::Array<byte_t>^ bytes);

  void AddByArray(unsigned long long ts, const Platform::Array<byte_t>^ bytes);

  Windows::Foundation::Collections::IVector<RawCaptionDataSubset^>^ GetData();

private:
  Platform::Collections::Vector<RawCaptionDataSubset^>^ _vector;
};


}}