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
#include "MemoryRow.h"
#include "HtmlRowLogic.h"

using namespace Microsoft::CC608;
using namespace std;

HtmlRowLogic::HtmlRowLogic(void)
{
}

HtmlRowLogic::~HtmlRowLogic(void)
{
}

void HtmlRowLogic::InitializeForRow()
{
  // clear the data from the wstringstream
  // (warning: .clear() clears the error state of the stream, rather than the data!)
  _ss.str(L"");

  _isInSpan = false;
  
  _currentColor = CC608CharColor::White;
  _currentFlash = false;
  _currentItalic = false;
  _currentUnderline = false;
}

void HtmlRowLogic::StartSpan()
{
  if (_isInSpan)
  {
    CloseSpan();
  }

  _ss << L"<span class=\"pf-608-bg-black ";

  if (_currentItalic)
  {
    _ss << L"pf-608-italic ";
  }

  if (_currentFlash)
  {
    // not supported
  }

  if (_currentUnderline)
  {
    _ss << L"pf-608-underline ";
  }

  switch(_currentColor)
  {
    case CC608CharColor::White:
      _ss << L"pf-608-white ";
      break;

    case CC608CharColor::Green:
      _ss << L"pf-608-green ";
      break;

    case CC608CharColor::Blue:
      _ss << L"pf-608-blue ";
      break;

    case CC608CharColor::Red:
      _ss << L"pf-608-red ";
      break;

    case CC608CharColor::Magenta:
      _ss << L"pf-608-magenta ";
      break;

    case CC608CharColor::Cyan:
      _ss << L"pf-608-cyan ";
      break;

    case CC608CharColor::Yellow:
      _ss << L"pf-608-yellow ";
      break;

    default:
      _ss << L"pf-608-white ";  // fallback!
      break;
  }

  _ss << L"\">";  // complete the opening span tag

  _isInSpan = true;
}

void HtmlRowLogic::CloseSpan()
{
  if (!_isInSpan)
  {
    return;
  }
  
  _ss << L"</span>";
  _isInSpan = false;
}

void HtmlRowLogic::ProcessCell(const MemoryCell& cell)
{
  if (cell.Attributes.ContainsAttributes())
  {
    _currentColor = cell.Attributes.GetColor();
    _currentFlash = cell.Attributes.IsFlash();
    _currentItalic = cell.Attributes.IsItalics();
    _currentUnderline = cell.Attributes.IsUnderline();

    if (_isInSpan)
    {
      CloseSpan();
    }
  }

  if (cell.IsTransparentSpace || cell.Character == 0)
  {
    if (_isInSpan)
    {
      CloseSpan();
    }

    _ss << L"&nbsp;";
    return;
  }

  // if arrive here, need a background (ie, span)
  if (!_isInSpan)
  {
    StartSpan();
  }

  if (cell.Character == L' ')
  {
    _ss << "&nbsp;";
    return;
  }

  _ss << cell.Character;
}

std::wstring HtmlRowLogic::RenderRow(const Microsoft::CC608::MemoryRow& row)
{
  InitializeForRow();

  _ss << L"<div style=\"height: 5.33%;\">";

  if (row.ContainsText())
  {
    for (auto& cell : row.Cells)
    {
      ProcessCell(cell);
    }
  }

  if (_isInSpan)
  {
    CloseSpan();
  }

  _ss << L"</div>\n";

  return _ss.str();
}

