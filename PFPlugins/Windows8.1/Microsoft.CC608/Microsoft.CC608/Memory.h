#pragma once

#include <vector>

#include "pch.h"
#include "MemoryRow.h"

namespace Microsoft { namespace CC608 {

  // Represents a grid of CC memory (either display or non-display)
  class Memory
  {
  public:
    Memory(void);
    ~Memory(void);

    void Character(const wchar_t character);
    void TransparentSpace();
    void Pac(short row, const short indent, const CC608CharColor color, const bool underline, const bool italics);
    void MidRowCode(const CC608CharColor color, const bool underline);
    void MidRowCode(const bool underline, const bool italics);
    void FlashOn();
    void CarriageReturn();
    void TabOffset(const short columns);
    void Backspace();
    void DeleteToEndOfRow();
    void EraseDisplayedMemory();
    void EraseNonDisplayedMemory();
    void SetRollUpBaseRow(short baseRow);
    void SetRollUpRowNumber(short numberRows);
    void Clear();

    void Dump();

    std::vector<MemoryRow> Rows;

  private:
    short GetValidRowPosition(short position);
    short GetValidCellPosition(short position);

  private:
    short _currentRow;
    short _currentCell;
    short _rollUpBaseRow;
    short _rollUpNumberRows;
  };

}}
