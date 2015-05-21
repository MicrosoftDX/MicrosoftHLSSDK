#pragma once

#include "Memory.h"
#include "XamlRowLogic.h"
#include "CaptionOptions.h"

namespace Microsoft { namespace CC608 {

class XamlRenderer
{
public:
  XamlRenderer(void);
  ~XamlRenderer(void);

  Windows::UI::Xaml::UIElement^ RenderXaml(
	  const Memory& displayMemory, 
	  const unsigned short videoHeightPixels, const bool animateUp);
  
  // <summary>Set the caption options</summary>
  // <param name="options">the options</param>
  void SetOptions(CaptionOptions^ options);

private:
  XamlRowLogic _rowLogic;
};


}}

