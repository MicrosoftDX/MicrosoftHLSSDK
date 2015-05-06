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

  void AddByArray(unsigned long long ts, const Platform::Array<byte_t>^ bytes);

  Windows::Foundation::Collections::IVector<RawCaptionDataSubset^>^ GetData();

private:
  Platform::Collections::Vector<RawCaptionDataSubset^>^ _vector;
};


}}