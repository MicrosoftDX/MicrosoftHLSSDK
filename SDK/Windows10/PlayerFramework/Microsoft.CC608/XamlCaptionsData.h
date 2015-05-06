#pragma once

#include "pch.h"

namespace Microsoft { namespace CC608 {

[Windows::Foundation::Metadata::WebHostHiddenAttribute]
public ref class XamlCaptionsData sealed
{
public:
  XamlCaptionsData(bool requiresUpdate, Windows::UI::Xaml::UIElement^ captionsRoot) : _requiresUpdate(requiresUpdate), _captionsRoot(captionsRoot)
  {
  }

  property Platform::Boolean RequiresUpdate
  {
    Platform::Boolean get() { return _requiresUpdate; }
  }

  property Windows::UI::Xaml::UIElement^ CaptionsRoot
  {
    Windows::UI::Xaml::UIElement^ get() { return _captionsRoot; }
  }

private:
  bool _requiresUpdate;
  Windows::UI::Xaml::UIElement^ _captionsRoot;
};

}}
