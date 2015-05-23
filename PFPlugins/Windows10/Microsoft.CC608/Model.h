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

#include "pch.h"
#include "CC608CharColor.h"
#include "Memory.h"
#include "ModelMode.h"

namespace Microsoft { namespace CC608 {

  // Represents the current state of the system, and includes displayed memory and non-displayed memory
  class Model
  {
  public:
    Model(void);
    ~Model(void);

    void Character(const wchar_t c);
    void Pac(const short row, const short indent, const CC608CharColor color, const bool underline, const bool italics);
    void MidRowCode(const CC608CharColor color, const bool underline);
    void MidRowCode(const bool underline, const bool italics);
    void ResumeCaptionLoading();
    void Backspace();
    void DeleteToEndOfRow();
    void RollUpCaptions(const short rows);
    void FlashOn();
    void ResumeDirectCaptioning();
    void EraseDisplayedMemory();
    void EraseNonDisplayedMemory();
    void CarriageReturn();
    void EndOfCaption();
    void TabOffset(const short columns);
    void TransparentSpace();
    void Dump();

    Memory displayedMemory;

    // returns a version number indicating if the display memory has changed
    unsigned int GetDisplayVersionNumber() const;

    // returns if the current model needs animation, and clears the flag if it does
    bool NeedsAnimation();

    void Clear();

  private:
    void DisplayedMemoryChanged();

    Memory _nonDisplayedMemory;
    Memory* _currentMemory;
    ModelMode _mode;

    unsigned int _displayVersion;

    bool _pendingAnimation;
  };

}}
