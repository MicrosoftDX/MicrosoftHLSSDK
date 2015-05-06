#pragma once

#include "pch.h"
#include "MemoryAttributes.h"

namespace Microsoft { namespace CC608 {

  class MemoryCell
  {
  public:
    MemoryCell(void);
    ~MemoryCell(void);

  public:
    void Clear();
    bool IsTransparentSpaceOrNullChar() const;

  public:
    wchar_t Character;
    MemoryAttributes Attributes;
    bool IsTransparentSpace;
  };

}}
