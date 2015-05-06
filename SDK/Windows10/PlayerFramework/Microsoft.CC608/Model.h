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
