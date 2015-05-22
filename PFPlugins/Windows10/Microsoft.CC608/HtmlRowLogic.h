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

#include <string>
#include <sstream>

#include "pch.h"
#include "MemoryRow.h"

namespace Microsoft { namespace CC608 {

// Internal helper class to render a row of 608 memory to HTML--users should call HtmlRenderer, which in turn calls this
class HtmlRowLogic
{
public:
  HtmlRowLogic(void);
  ~HtmlRowLogic(void);

  std::wstring RenderRow(const Microsoft::CC608::MemoryRow& row);

private:
  bool _isInSpan;
  std::wstringstream _ss;

  CC608CharColor _currentColor;
  bool _currentItalic;
  bool _currentFlash;
  bool _currentUnderline;

  void InitializeForRow();

  void StartSpan();
  void CloseSpan();

  void ProcessCell(const MemoryCell& cell);
};

}}