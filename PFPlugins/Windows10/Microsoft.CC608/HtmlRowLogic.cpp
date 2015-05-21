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

