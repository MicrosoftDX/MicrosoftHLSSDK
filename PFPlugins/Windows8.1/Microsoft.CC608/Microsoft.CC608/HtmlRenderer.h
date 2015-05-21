#pragma once

#include <string>

#include "pch.h"
#include "Memory.h"
#include "HtmlRowLogic.h"

namespace Microsoft { namespace CC608 {

// Renders 608 caption memory into HTML markup for display.
class HtmlRenderer
{
public:
  // render the memory into HTML and return the string 
  std::wstring RenderHtml(const Memory& displayMemory, const unsigned short videoHeightPixels, const bool animateUp);

  HtmlRenderer(void);
  ~HtmlRenderer(void);

private:
  HtmlRowLogic _rowLogic;

  std::wstring HtmlRenderer::RenderCoreHtml(const Memory& displayMemory);
  std::wstring HtmlRenderer::WrapCoreHtml(const unsigned short videoHeightPixels, const std::wstring& coreHtml, const bool animateUp);
};

}}