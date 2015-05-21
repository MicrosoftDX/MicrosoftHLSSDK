#pragma once

#include <vector>

#include "pch.h"
#include "MemoryCell.h"

namespace Microsoft { namespace CC608 {

  class MemoryRow
  {
  public:
    MemoryRow(void);
    ~MemoryRow(void);

    void Clear();
    bool ContainsText() const;

    std::vector<MemoryCell> Cells;
  };

}}
