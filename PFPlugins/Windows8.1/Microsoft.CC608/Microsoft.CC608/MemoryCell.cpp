#include "pch.h"
#include "MemoryCell.h"

using namespace Microsoft::CC608;

MemoryCell::MemoryCell()
{
  Clear();
}

MemoryCell::~MemoryCell()
{
}

void MemoryCell::Clear()
{
  Character = L'';
  Attributes.Clear();
  IsTransparentSpace = false;
}

bool MemoryCell::IsTransparentSpaceOrNullChar() const
{
  return IsTransparentSpace || Character == 0;
}
