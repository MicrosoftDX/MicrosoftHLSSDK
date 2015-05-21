#include "pch.h"
#include <algorithm>
#include "Memory.h"
#include "MemorySize.h"
#include "Logger.h"

using namespace Microsoft::CC608;

Memory::Memory(void)
{
  Rows.resize(MemorySize::Rows);

  // each row starts with the default attribute of white
  for (auto& row : Rows)
  {
    row.Cells[0].Attributes.Clear();
    row.Cells[0].Attributes.SetColor(CC608CharColor::White);
  }

  // init cursor position
  _currentRow = 0;
  _currentCell = 0;

  // rollup base row default to bottom row
  _rollUpBaseRow = MemorySize::Rows;
  _rollUpNumberRows = 3;
}

Memory::~Memory(void)
{
}

void Memory::Clear()
{
  // clear all rows
  for (auto& row : Rows)
  {
    row.Clear();
  }
}

void Memory::Character(const wchar_t character)
{
  // set character for current position
  Rows[_currentRow].Cells[_currentCell].Character = character;

  // move to next position, stay on last cell if at end of row
  _currentCell = GetValidCellPosition(_currentCell + 1);
}

void Memory::TransparentSpace()
{
   // set transparanent space for current position
  Rows[_currentRow].Cells[_currentCell].IsTransparentSpace = true;

  // move to next position, stay on last cell if at end of row
  _currentCell = GetValidCellPosition(_currentCell + 1);
}

void Memory::Pac(const short row, const short indent, const CC608CharColor color, const bool underline, const bool italics)
{
  assert(row >= 1 && row <= MemorySize::Rows);
  assert(indent >= 0 && indent < MemorySize::Cells);

  // row is 1-based, but indent is 0-based
  _currentRow = GetValidRowPosition(row - 1);
  _currentCell = GetValidCellPosition(indent);

  Rows[_currentRow].Cells[_currentCell].Attributes.SetColor(color);
  Rows[_currentRow].Cells[_currentCell].Attributes.SetUnderline(underline);
  Rows[_currentRow].Cells[_currentCell].Attributes.SetItalics(italics);
}

void Memory::MidRowCode(const CC608CharColor color, const bool underline)
{
  // first set the attributes for the current position
  Rows[_currentRow].Cells[_currentCell].Attributes.SetColor(color);
  Rows[_currentRow].Cells[_currentCell].Attributes.SetUnderline(underline);

  // miderow code always turns flash off
  Rows[_currentRow].Cells[_currentCell].Attributes.SetFlash(false);

  // midrow code adds a space char
  Character(L' ');
}

void Memory::MidRowCode(const bool underline, const bool italics)
{
  // first set the attributes for the current position
  Rows[_currentRow].Cells[_currentCell].Attributes.SetUnderline(underline);
  Rows[_currentRow].Cells[_currentCell].Attributes.SetItalics(italics);

  // miderow code always turns flash off
  Rows[_currentRow].Cells[_currentCell].Attributes.SetFlash(false);

  // midrow code adds a space char
  Character(L' ');
}

void Memory::FlashOn()
{
  // first set the flash attribute for the current position,
  // does not change any other attributes
  Rows[_currentRow].Cells[_currentCell].Attributes.SetFlash(true);

  // flash code adds a space char
  Character(L' ');
}

void Memory::Backspace()
{
  // move back one position, clear cell (character and any attributes)
  _currentCell = GetValidCellPosition(_currentCell - 1);
  Rows[_currentRow].Cells[_currentCell].Clear();

  // if this is the first cell in the row, reset the default attribute
  if (_currentCell == 0)
  {
    Rows[_currentRow].Cells[_currentCell].Attributes.Clear();
    Rows[_currentRow].Cells[_currentCell].Attributes.SetColor(CC608CharColor::White);
  }
}

void Memory::DeleteToEndOfRow()
{
  // clear cells from the current position to the right, for the current row
  for (short i = _currentCell; i < MemorySize::Cells; i++)
  {
    Rows[_currentRow].Cells[i].Clear();
  }
}

void Memory::SetRollUpBaseRow(short baseRow)
{
  // return right away if not changing
  if (_rollUpBaseRow == baseRow)
    return;

  // make sure specified a valid base row
  assert(baseRow >= 1 && baseRow <= MemorySize::Rows);
  if (baseRow < 1 || baseRow > MemorySize::Rows)
    return;

  // move the rollup window to the new base row
  for (short i = 0; i < _rollUpNumberRows; i++)
  {
    if (baseRow - i > 0 && baseRow - i <= MemorySize::Rows &&
      _rollUpBaseRow - i > 0 && _rollUpBaseRow - i <= MemorySize::Rows)
    {
      Rows[baseRow - i - 1] = std::move(Rows[_rollUpBaseRow - i - 1]);
    }
  }  

  // make sure all rows outside of the rollup window are cleared
  for (short i = 0; i < MemorySize::Rows; i++)
  {
    if (i > baseRow || i < (baseRow - _rollUpNumberRows))
    {
      Rows[i].Clear();
    }
  }  

  _rollUpBaseRow = baseRow;
}

void Memory::SetRollUpRowNumber(short numberRows)
{
  // make sure specified a valid number of rows,
  // 608 can have rollup number of rows 2, 3, or 4
  assert(numberRows >= 2 && numberRows <= 4);
  if (numberRows < 2 || numberRows > 4)
    return;

  _rollUpNumberRows = numberRows;
}

void Memory::CarriageReturn()
{
  for (short i = 0; i < _rollUpBaseRow; i++)
  {
    if (i >= (_rollUpBaseRow - _rollUpNumberRows) && 
        i < (_rollUpBaseRow - 1))
    {
      // move row up
      Rows[i] = std::move(Rows[i+1]);
    }
    else
    {
      // clear any rows outside of the rollup window
      Rows[i].Clear();
    }
  }  

  // cursor position set to base row
  _currentRow = GetValidRowPosition(_rollUpBaseRow-1);
  _currentCell = 0;
}

void Memory::TabOffset(const short columns)
{
  // move over x (columns) number of cells
  _currentCell = GetValidCellPosition(_currentCell + columns);
}

// don't want invalid commands to break the model, make sure
// the position is a valid row and clip to bounds if necessary
short Memory::GetValidRowPosition(short position)
{
  position = std::min(position, static_cast<short>(MemorySize::Rows - 1));
  position = std::max(position, static_cast<short>(0));
  return position;
}

// don't want invalid commands to break the model, make sure
// the position is a valid cell and clip to bounds if necessary
short Memory::GetValidCellPosition(short position)
{
  position = std::min(position, static_cast<short>(MemorySize::Cells - 1));
  position = std::max(position, static_cast<short>(0));
  return position;
}

void Memory::Dump()
{
  for (int i = 0; i < MemorySize::Rows; i++)
  {
    // new row
    DebugWrite(L"row " << i << ": ");

    // each cell for row
    for (auto cell : Rows[i].Cells)
    {
      wchar_t character = cell.Character == L'' ? L'.' : cell.Character;
      DebugWrite(character);
    }

    DebugWrite("\n");
  }
}

