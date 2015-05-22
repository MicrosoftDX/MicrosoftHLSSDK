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
#include "Model.h"
#include "MemorySize.h"
#include "Logger.h"

using namespace Microsoft::CC608;
using namespace std;

Model::Model() : displayedMemory(), _nonDisplayedMemory(), _mode(ModelMode::RollUp3), _displayVersion(0), _pendingAnimation(false)
{
  _currentMemory = &displayedMemory;
}

Model::~Model(void)
{
}

void Model::Character(const wchar_t c)
{
  _currentMemory->Character(c);

  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || 
    _mode == ModelMode::RollUp4 || _mode == ModelMode::PaintOn)
  {
    DisplayedMemoryChanged();
  }
}

void Model::TransparentSpace()
{
  _currentMemory->TransparentSpace();

  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || 
    _mode == ModelMode::RollUp4 || _mode == ModelMode::PaintOn)
  {
    DisplayedMemoryChanged();
  }
}

void Model::Pac(const short row, const short indent, const CC608CharColor color, const bool underline, const bool italics)
{
  _currentMemory->Pac(row, indent, color, underline, italics);

  // for rollup mode, the rollup window moves to the new specified base row
  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || _mode == ModelMode::RollUp4)
  {
    _currentMemory->SetRollUpBaseRow(row);
    DisplayedMemoryChanged();
  }
}

void Model::MidRowCode(const CC608CharColor color, const bool underline)
{
  _currentMemory->MidRowCode(color, underline);
}

void Model::MidRowCode(const bool underline, const bool italics)
{
  _currentMemory->MidRowCode(underline, italics);
}

void Model::FlashOn()
{
  _currentMemory->FlashOn();
}

void Model::Backspace()
{
  _currentMemory->Backspace();

  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || 
    _mode == ModelMode::RollUp4 || _mode == ModelMode::PaintOn)
  {
    DisplayedMemoryChanged();
  }
}

void Model::DeleteToEndOfRow()
{
  _currentMemory->DeleteToEndOfRow();

  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || 
    _mode == ModelMode::RollUp4 || _mode == ModelMode::PaintOn)
  {
    DisplayedMemoryChanged();
  }
}

void Model::EraseDisplayedMemory()
{
  displayedMemory.Clear();
  DisplayedMemoryChanged();
}

void Model::EraseNonDisplayedMemory()
{
  _nonDisplayedMemory.Clear();
}

void Model::CarriageReturn()
{
  // carriage return only affects rollup mode
  if (_mode == ModelMode::RollUp2 || _mode == ModelMode::RollUp3 || _mode == ModelMode::RollUp4)
  {
    _currentMemory->CarriageReturn();
    _pendingAnimation = true;
    DisplayedMemoryChanged();
  }
}

void Model::TabOffset(const short columns)
{
  _currentMemory->TabOffset(columns);
}

// set to rollup mode
void Model::RollUpCaptions(const short rows)
{
  assert(rows >= 2 && rows <= 4);

  // rollup mode erases captions from all other modes
  if (_mode == ModelMode::PaintOn || _mode == ModelMode::PopOn)
  {
    // if not in Roll-up mode already, clear display memory
    displayedMemory.Clear();
  }

  _nonDisplayedMemory.Clear();
  _currentMemory = &displayedMemory;
  _pendingAnimation = false;

  // init base row defaults to bottom if not in rollup mode already
  if (_mode == ModelMode::PaintOn || _mode == ModelMode::PopOn)
  {
    _currentMemory->SetRollUpBaseRow(MemorySize::Rows);
    _currentMemory->Pac(MemorySize::Rows, 0, CC608CharColor::White, false, false);
  }

  switch (rows)
  {
    case 2:
      _mode = ModelMode::RollUp2;
      _currentMemory->SetRollUpRowNumber(2);
      break;

    case 4:
      _mode = ModelMode::RollUp4;
      _currentMemory->SetRollUpRowNumber(4);
      break;

    // use RU3 if specified an incorrect row
    default:
      _mode = ModelMode::RollUp3;
      _currentMemory->SetRollUpRowNumber(3);
      break;
  }

  DisplayedMemoryChanged();
}

// set to popon mode
void Model::ResumeCaptionLoading()
{
  _mode = ModelMode::PopOn;
  _currentMemory = &_nonDisplayedMemory;
  _pendingAnimation = false;
}

void Model::EndOfCaption()
{  
  // switch to popon mode if not currently in the mode
  _mode = ModelMode::PopOn;

  // swap and display the previously non-displayed memory
  std::swap(displayedMemory, _nonDisplayedMemory);
  DisplayedMemoryChanged();

  // write captions to the non-displayed memory again,
  // don't erase the non-displayed memory
  _currentMemory = &_nonDisplayedMemory;
}

// seto to painton mode
void Model::ResumeDirectCaptioning()
{
  // clear displayed memory
  displayedMemory.Clear();
  DisplayedMemoryChanged();

  _mode = ModelMode::PaintOn;
  _currentMemory = &displayedMemory;

  _pendingAnimation = false;
}

// returns a version number to be used to determine if anything visual has changed
unsigned int Model::GetDisplayVersionNumber() const
{
  return _displayVersion;
}

// debug, display contents of displayed memory
void Model::Dump()
{
  DebugWrite("Displayed memory\n");
  displayedMemory.Dump();
}

// clear all memory and reset to initial state
void Model::Clear()
{
  displayedMemory.Clear();
  _nonDisplayedMemory.Clear();

  _mode = ModelMode::RollUp3;
  _currentMemory = &displayedMemory;

  _pendingAnimation = false;
  _displayVersion = 0;

  DisplayedMemoryChanged();
}

// internal method used to track changes to the displayed memory
void Model::DisplayedMemoryChanged()
{
  ++_displayVersion;
}

bool Model::NeedsAnimation()
{
  // only animate if we have pending animation AND we are in a roll-up mode!
  auto needs = (_pendingAnimation && ((_mode == ModelMode::RollUp2) || (_mode == ModelMode::RollUp3) || (_mode == ModelMode::RollUp4)));

  // always reset after it is rendered
  _pendingAnimation = false;

  return needs;
}
