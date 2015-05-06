#pragma once

#include "pch.h"
#include "CC608CharColor.h"

namespace Microsoft { namespace CC608 {

  // attributes for a cell, this is very memory efficient, using one byte to 
  // store all of the attributes for a cell, might not be necessary but wanted
  // to use the least amount of memory possible for each cell
  class MemoryAttributes
  {
  public:
    MemoryAttributes(void);
    ~MemoryAttributes(void);

    void Clear();
    bool ContainsAttributes() const;

    void SetUnderline(bool value);
    bool IsUnderline() const;

    void SetItalics(bool value);
    bool IsItalics() const;

    void SetFlash(bool value);
    bool IsFlash() const;

    void SetColor(CC608CharColor value);
    CC608CharColor GetColor() const;

  private:
    void SetBit(unsigned char mask, bool value);
    bool GetBit(unsigned char mask) const;

  private: 
    unsigned char _attributes;
  };

}}

