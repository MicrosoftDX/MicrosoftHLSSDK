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
