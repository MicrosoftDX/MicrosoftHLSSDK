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
#include "pch.h"
#include "DecodeLogic.h"

using namespace Microsoft::CC608;

DecodeLogic::DecodeLogic(void) : _pacToRow()
{
  _pacToRow.push_back(11);
  _pacToRow.push_back(1);
  _pacToRow.push_back(3);
  _pacToRow.push_back(12);
  _pacToRow.push_back(14);
  _pacToRow.push_back(5);
  _pacToRow.push_back(7);
  _pacToRow.push_back(9);
}


DecodeLogic::~DecodeLogic(void)
{
}


DecodedPac DecodeLogic::DecodePac(const byte_t first, const byte_t second) const
{
  DecodedPac p;

  byte_t diff;
  short group = 0;

  if (second >= 0x40 && second <= 0x5F)
  {
    group = 0;
    diff = second - 0x40;
  }
  else if (second >= 0x60 && second <= 0x7F)
  {
    group = 1;
    diff = second - 0x60;
  }
  else   // invalid 2nd byte for PAC
  {
    p.IsValid = false;
    return p;
  }

  if (first < 0x10 || first > 0x1F)
  {
    // invalid 1st byte for PAC
    p.IsValid = false;
    return p;
  }

  // row is one higher if second byte is 0x60 to 0x7F (aka group)
  // (bit #4 (0x08) is the channel bit--force this to zero with && 0xF7)
  p.Row = _pacToRow[(first - 0x10) & 0xF7] + group;

  p.Underline = (diff & 0x01) == 0x01;

  if (diff <= 0x0D)
  {
    p.Color = static_cast<CC608CharColor>(diff >> 1);
  }
  else if (diff <= 0x0F)
  {
    p.Italics = true;
  }
  else // indent (0x10 to 0x1F)
  {
    // least significant bit is underline (already processed), so get rid of it with 0xFE
    // (then need to shift bits one place to the left)
    p.Indent = ((diff - 0x10) & 0xFE) << 1; 
  }
  
  return p;
}

bool DecodeLogic::ParityValid(byte_t b) const
{
  // want to check for odd parity (b is valid if and only if it has an odd number of bits set)
  b ^= (b >> 4);
  b ^= (b >> 2);
  return (0 != (0x01 & (b ^ (b >> 1))));
}
