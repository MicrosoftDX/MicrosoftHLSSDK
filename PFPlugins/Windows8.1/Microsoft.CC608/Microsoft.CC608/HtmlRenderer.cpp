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
#include "pch.h"
#include "HtmlRenderer.h"
#include <sstream>

using namespace Microsoft::CC608;
using namespace std;

HtmlRenderer::HtmlRenderer(void) : _rowLogic()
{
}

HtmlRenderer::~HtmlRenderer(void)
{
}

// returns the complete HTML for the captions specified by displayMemory, scaled to the specified video height
std::wstring HtmlRenderer::RenderHtml(const Memory& displayMemory, const unsigned short videoHeightPixels, const bool animateUp)
{
  auto coreHtml = RenderCoreHtml(displayMemory);

  auto html = WrapCoreHtml(videoHeightPixels, coreHtml, animateUp);

  return html;
}

// creates the core HTML without scaling information
std::wstring HtmlRenderer::RenderCoreHtml(const Memory& displayMemory)
{
  wstringstream ss;

  // top 10% spacer (at top of screen)
  ss << L"<div style=\"height: 10%;\"></div>\n";

  for(const auto& row : displayMemory.Rows)
  {
    ss << _rowLogic.RenderRow(row);
  }

  // bottom 10% spacer
  ss << L"<div style=\"height: 10%;\"></div>\n";

  return ss.str();
}

// wrap the core HTML in wrapper HTML that sets the size properly (called just before the HTML will be added to the display)
std::wstring HtmlRenderer::WrapCoreHtml(const unsigned short videoHeightPixels, const std::wstring& coreHtml, const bool animateUp)
{
  // don't render if too small to read
  if (videoHeightPixels < 40)
  {
    return L"";
  }

  wstringstream ss;

  float lineHeight = videoHeightPixels * (0.80f / 15.0f); // captions take 80% of the height, and there are 15 rows of captions  
  float fontSize = lineHeight * 0.88f;  // rough value that works--make sure that underlines are visible (not overlaid by row below)

  ss << L"<div class=\"pf-608-roll-up-container\">\n";

  ss << L"<div style=\"height: " << videoHeightPixels << L"px; line-height: " << lineHeight << L"px; font-size: " << fontSize << L"px;\"";

  // if requested, add an animation class that will animate the whole div up one row
  if (animateUp)
  {
    ss << " class=\"pf-608-roll-up\"";
  }
    
  ss << ">\n";

  ss << coreHtml;

  ss << L"</div>\n";

  ss << L"</div>\n";

  return ss.str();
}
