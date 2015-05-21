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

