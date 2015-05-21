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
  // L'' causes error C2317, using L'\0' instead
  Character = L'\0';
  Attributes.Clear();
  IsTransparentSpace = false;
}

bool MemoryCell::IsTransparentSpaceOrNullChar() const
{
  return IsTransparentSpace || Character == 0;
}
