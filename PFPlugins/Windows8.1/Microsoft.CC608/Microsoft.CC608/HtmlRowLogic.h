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