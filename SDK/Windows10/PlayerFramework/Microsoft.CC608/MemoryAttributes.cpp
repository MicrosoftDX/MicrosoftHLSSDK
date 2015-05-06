#include "pch.h"
#include "MemoryAttributes.h"

using namespace Microsoft::CC608;

// bitmasks used to store / retrieve data from attibute bit data
const unsigned char AttributeMask = 0x01;
const unsigned char UnderlineMask = 0x02;
const unsigned char ItalicsMask = 0x04;
const unsigned char FlashMask = 0x08;
const unsigned char ColorMask = 0x70;

MemoryAttributes::MemoryAttributes()
{
  Clear();
}

MemoryAttributes::~MemoryAttributes()
{
}

void MemoryAttributes::Clear()
{
  _attributes = 0;
}

bool MemoryAttributes::ContainsAttributes() const
{
  return (_attributes > 0) ? true : false;
}

void MemoryAttributes::SetUnderline(bool value)
{
  SetBit(AttributeMask, true);
  SetBit(UnderlineMask, value);
}

bool MemoryAttributes::IsUnderline() const
{
  return GetBit(UnderlineMask);
}

void MemoryAttributes::SetItalics(bool value)
{
  SetBit(AttributeMask, true);
  SetBit(ItalicsMask, value);
}

bool MemoryAttributes::IsItalics() const
{
  return GetBit(ItalicsMask);
}

void MemoryAttributes::SetFlash(bool value)
{
  SetBit(AttributeMask, true);
  SetBit(FlashMask, value);
}

bool MemoryAttributes::IsFlash() const
{
  return GetBit(FlashMask);
}

void MemoryAttributes::SetColor(CC608CharColor value)
{
  SetBit(AttributeMask, true);

  // store color bits
  unsigned char color = static_cast<unsigned char>(value) << 4;
  _attributes &= ~ColorMask;
  _attributes |= color;
}

CC608CharColor MemoryAttributes::GetColor() const
{
  // get color bits, convert to color enum
  unsigned char color = _attributes & ColorMask;
  return static_cast<CC608CharColor>(color >> 4);
}

void MemoryAttributes::SetBit(unsigned char mask, bool value)
{
  if (value)
  {
    _attributes |= mask;
  }
  else
  {
    _attributes &= ~mask;
  }
}

bool MemoryAttributes::GetBit(unsigned char mask) const
{
  return (_attributes & mask) ? true : false;
}
