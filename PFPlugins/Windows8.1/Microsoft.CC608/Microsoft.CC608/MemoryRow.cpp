#include "pch.h"
#include <algorithm>
#include "MemorySize.h"
#include "MemoryRow.h"

using namespace Microsoft::CC608;

MemoryRow::MemoryRow(void)
{
  Cells.resize(MemorySize::Cells);
}

MemoryRow::~MemoryRow(void)
{
}

void MemoryRow::Clear()
{
  for (MemoryCell& cell : Cells)
  {
    cell.Clear();
  }
}

bool MemoryRow::ContainsText() const
{
  for (const MemoryCell& cell : Cells)
  {
    if (cell.Character != L'')
      return true;
  }

  return false;
}

