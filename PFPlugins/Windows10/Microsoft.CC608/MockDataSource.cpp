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
#include "MockDataSource.h"

using namespace std;
using namespace Microsoft::CC608;

MockDataSource::MockDataSource(void) : _addClock()
{
}

MockDataSource::~MockDataSource(void)
{
}

void MockDataSource::AllRows()
{
  AddResumeDirectCaptioning(10);

  AddPac(1, 0,  CC608CharColor::Magenta, false, false);
  AddNullPaddedString("Top rowy", 500);

  AddPac(2, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 2", 500);

  AddPac(3, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("Rowy 3", 500);

  AddPac(4, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 4", 500);

  AddPac(5, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("Rowq 5", 500);

  AddPac(6, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 6", 500);

  AddPac(7, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("Rowy 7", 500);

  AddPac(8, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 8", 500);

  AddPac(9, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWy 9", 500);

  AddPac(10, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 10", 500);

  AddPac(11, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWg 11", 500);

  AddPac(12, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("ROWW 12", 500);

  AddPac(13, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("Row 13", 500);

  AddPac(14, 0,  CC608CharColor::Green, false, false);
  AddNullPaddedString("14th row", 500);

  AddPac(15, 0,  CC608CharColor::White, false, false);
  AddNullPaddedString("15th row", 500);
}



void MockDataSource::RollUpSample()
{
  AddRollUpCaptions(2);

  AddPac(13, 0, CC608CharColor::White, false, false);
  
  AddStringAndPerhapsANull("Start of roll-up (2 rows)", 1000);
  AddCarriageReturn(1000);
  AddNullPaddedString("Second line of roll-ups", 1000);
  AddCarriageReturn(1000);
  AddNullPaddedString("Third line of roll-ups", 1000);
  AddCarriageReturn(1000);
  AddNullPaddedString("Fourth line of roll-ups", 1000);
  AddCarriageReturn(1000);
  AddNullPaddedString("Fifth line of roll-ups", 1000);
  AddCarriageReturn(1000);
  AddNullPaddedString("Sixth line of roll-ups", 1000);
  AddCarriageReturn(1000);
}

void MockDataSource::PopOnSample()
{
  AddResumeCaptionLoading(1000);

  AddPac(12, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("First line of pop on", 500);

  AddPac(13, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("Second line of pop on", 500);

  AddEndOfCaption(50);

  AddPac(12, 0, CC608CharColor::Blue, false, false);
  AddNullPaddedString("Pop #2: First line of pop on", 500);

  AddPac(13, 0, CC608CharColor::Blue, false, false);
  AddNullPaddedString("Pop #2: Second line of pop on", 500);

  AddEndOfCaption(50);
}

void MockDataSource::PaintOnSample()
{
  AddResumeDirectCaptioning(1000);
  
  AddPac(13, 0, CC608CharColor::White, false, false);

  AddNullPaddedString("First line of paint on", 2000);

  AddPac(14, 0, CC608CharColor::Blue, false, false);

  AddNullPaddedString("Second line of paint on", 2000);

  AddEraseDisplayedMemory(3000);

  AddPac(13, 0, CC608CharColor::Red, false, false);

  AddNullPaddedString("First line after screen cleared", 1000);
}

void MockDataSource::Channel2Data()
{
  AddResumeDirectCaptioning(100, 2);

  AddPac(14, 0, CC608CharColor::Red, false, false, 2);
  AddNullPaddedString("THIS IS CHANNEL 2 DATA!!!!", 1000);

  AddPac(13, 0, CC608CharColor::Red, false, false, 2);
  AddNullPaddedString("THIS IS CHANNEL 2 DATA!!!", 1000);

  AddRollUpCaptions(2, 2);
  AddPac(5, 0, CC608CharColor::Red, false, false, 2);
  
  AddMidRowCode(CC608CharColor::Red, false, 2);
  AddNullPaddedString("THIS IS CHANNEL 2 DATA!!!!", 500);
  AddCarriageReturn(500, 2);

  AddMidRowCode(CC608CharColor::Red, false, 2);
  AddNullPaddedString("THIS IS CHANNEL 2 DATA!!!!", 500);
  AddCarriageReturn(500, 2);
}

void MockDataSource::TestPattern01_Colors()
{
  AddResumeDirectCaptioning(10);

  AddPac(1, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("WHITE TEXT", 100);

  AddPac(2, 0, CC608CharColor::Green, false, false);
  AddNullPaddedString("GREEN TEXT", 100);

  AddPac(3, 0, CC608CharColor::Blue, false, false);
  AddNullPaddedString("BLUE TEXT", 100);

  AddPac(4, 0, CC608CharColor::Cyan, false, false);
  AddNullPaddedString("CYAN TEXT", 100);

  AddPac(5, 0, CC608CharColor::Yellow, false, false);
  AddNullPaddedString("YELLOW TEXT", 100);

  AddPac(6, 0, CC608CharColor::Magenta, false, false);
  AddNullPaddedString("MAGENTA TEXT", 100);

  AddPac(7, 0, CC608CharColor::Red, false, false);
  AddNullPaddedString("RED TEXT", 100);

  // row 8 empty

  AddPac(9, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("Following should end with 14...", 100);

  AddPac(10, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("1234567890123456789012345678901234", 100);

  AddPac(11, 0, CC608CharColor::White, true, false);
  AddNullPaddedString("White underline...", 100);

  AddPac(12, 0, CC608CharColor::White, false, true);
  AddNullPaddedString("White italics...", 100);

  AddPac(13, 0, CC608CharColor::White, true, true);
  AddNullPaddedString("White underline AND italics...", 100);
}

void MockDataSource::TestPattern02_PacIndents()
{
  AddResumeDirectCaptioning(10);

  AddPac(1, 0, CC608CharColor::White, false, false);
  AddNullPaddedString("01234567890123456789012345678901", 100);

  AddPac(2, 4, CC608CharColor::White, false, false);
  AddNullPaddedString("Indent 4", 100);

  AddPac(3, 4, CC608CharColor::White, true, false);
  AddNullPaddedString("Indent 4 (underline)", 100);

  AddPac(4, 8, CC608CharColor::White, false, false);
  AddNullPaddedString("Indent 8", 100);

  AddPac(5, 8, CC608CharColor::White, true, false);
  AddNullPaddedString("Indent 8 (u)", 100);

  AddPac(6, 12, CC608CharColor::White, false, false);
  AddNullPaddedString("Indent 12", 100);

  AddPac(7, 12, CC608CharColor::White, true, false);
  AddNullPaddedString("Indent 12 (u)", 100);

  AddPac(8, 16, CC608CharColor::White, false, false);
  AddNullPaddedString("Indent 16", 100);

  AddPac(9, 16, CC608CharColor::White, true, false);
  AddNullPaddedString("Indent 16 (u)", 100);

  AddPac(10, 20, CC608CharColor::White, false, false);
  AddNullPaddedString("I-20", 100);

  AddPac(11, 20, CC608CharColor::White, true, false);
  AddNullPaddedString("I-20 (u)", 100);

  AddPac(12, 24, CC608CharColor::White, false, false);
  AddNullPaddedString("I-24", 100);

  AddPac(13, 24, CC608CharColor::White, true, false);
  AddNullPaddedString("I-24 (u)", 100);

  AddPac(14, 28, CC608CharColor::White, false, false);
  AddNullPaddedString("I-28", 100);

  AddPac(15, 28, CC608CharColor::White, true, false);
  AddNullPaddedString("28-u", 100);

}

void MockDataSource::TestPattern03_MidRowCodes()
{
  AddResumeDirectCaptioning(10);

  AddPac(1, 0, CC608CharColor::White, false, false);

  AddMidRowCode(true, true);
  AddStringAndPerhapsANull("Italics & under", 100);

  AddTransparentSpace();
  AddTransparentSpace();

  AddMidRowCode(false, true);
  AddStringAndPerhapsANull("Italics only", 100);

  AddPac(2, 0, CC608CharColor::White, false, false);
  AddTabOffset(1);
  AddStringAndPerhapsANull("Tab offset 1", 100);
  
  AddPac(3, 0, CC608CharColor::White, false, false);
  AddTabOffset(2);
  AddStringAndPerhapsANull("Tab offset 2", 100);
  
  AddPac(4, 0, CC608CharColor::White, false, false);
  AddTabOffset(3);
  AddStringAndPerhapsANull("Tab offset 3", 100);

  AddPac(5, 0, CC608CharColor::White, false, false);
  
}

void MockDataSource::ClockIt(const unsigned long long delayInMilliseconds)
{
  assert(delayInMilliseconds >= 0);

  // make all delays in msec
  unsigned long long currentClock = _addClock.GetTicks();
  unsigned long long delayInTicks = delayInMilliseconds * Timestamp::TicksPerMillisecond;

  _addClock = Timestamp(currentClock + delayInTicks);
}

void MockDataSource::AddStringAndPerhapsANull(string s, const unsigned long long delayInMilliseconds)
{
  ClockIt(delayInMilliseconds);

  // force s to have an even number of characters
  if (s.length() % 2 == 1)
    s.append("\0");

  size_t len = s.length();

  for(size_t i = 0; i < len; i += 2)
  {
    SaveBytePair(static_cast<byte_t>(s[i]), static_cast<byte_t>(s[i+1]));
    ClockIt();
  }
}

void MockDataSource::AddNullPaddedString(string s, const unsigned long long delayInMilliseconds)
{
  ClockIt(delayInMilliseconds);

  for(auto c: s)
  {
    SaveBytePair(static_cast<byte_t>(c), 0);
    ClockIt();
  }
}

void MockDataSource::AddNulls()
{
  ClockIt();
  SaveBytePair(0, 0);
}

void MockDataSource::AddTransparentSpace()
{
  ClockIt();

  SaveBytePair(0x11, 0x39);
}

byte_t MockDataSource::GetFirstByteMiscCommand(const int channel)
{
  assert((channel == 1) || (channel == 2));

  return (channel == 1) ? 0x14 : 0x1C;
}

void MockDataSource::AddResumeCaptionLoading(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);
  
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x20);
}

void MockDataSource::AddRollUpCaptions(short rows, const int channel)
{
  ClockIt();

  assert((rows > 1) && (rows < 5));

  SaveBytePair(GetFirstByteMiscCommand(channel), static_cast<byte_t>(0x23 + rows));
}

void MockDataSource::AddBackspace(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);

  SaveBytePair(GetFirstByteMiscCommand(channel), 0x21);
}

void MockDataSource::AddPac(const short row, const BYTE indent, const CC608CharColor color, const bool underline, const bool italics, const int channel)
{
  ClockIt();

  assert((row > 0) && (row < 16));

  assert(indent % 4 == 0);

  assert((indent >= 0) && (indent < 29));

  // must have white for indent greater than zero
  assert(!((color != CC608CharColor::White) && (indent != 0)));
  
  // must have white for italics--can set color separately with a mid-row code
  assert(!(italics && (color != CC608CharColor::White)));

  // must have zero indent for italics
  assert(!(italics && (indent != 0)));

  assert((channel == 1) || (channel == 2));

  bool isChannel1 = (channel == 1);

  byte_t b1 = 0x11;

  switch(row)
  {
    case 1:
    case 2:
      b1 = isChannel1 ? 0x11 : 0x19;
      break;

    case 3:
    case 4:
      b1 = isChannel1 ? 0x12 : 0x1A;
      break;

    case 5:
    case 6:
      b1 = isChannel1 ? 0x15 : 0x1D;
      break;

    case 7:
    case 8:
      b1 = isChannel1 ? 0x16 : 0x1E;
      break;

    case 9:
    case 10:
      b1 = isChannel1 ? 0x17 : 0x1F;
      break;

    case 11:
      b1 = isChannel1 ? 0x10 : 0x18;
      break;

    case 12:
    case 13:
      b1 = isChannel1 ? 0x13 : 0x1B;
      break;

    case 14:
    case 15:
      b1 = isChannel1 ? 0x14 : 0x1C;
      break;

    default:
      assert(false);
  }

  byte_t b2 = 0x40 + static_cast<byte_t>(color) * 2;

  if (underline)
    ++b2;

  if (indent)
    b2 += 0x10;

  if (italics)
    b2 += 0x0E;

  // certain rows have their row value partially encoded in the second byte
  switch(row)
  {
    case 2:
    case 4:
    case 6:
    case 8:
    case 10:
    case 13:
    case 15:
      b2 += 0x20;
  }

  b2 += indent / 2;

  SaveBytePair(b1, b2);
}

byte_t MockDataSource::GetFirstByteMidRowCode(const int channel)
{
  assert((channel == 1) || (channel == 2));

  bool isChannel1 = (channel == 1);

  byte_t b1 = isChannel1 ? 0x11 : 0x19;

  return b1;
}

void MockDataSource::AddMidRowCode(const CC608CharColor color, const bool underline, const int channel)
{
  ClockIt();

  byte_t b2 = static_cast<byte_t>(color) * 2;

  if (underline) ++b2;

  b2 += 0x20;

  SaveBytePair(GetFirstByteMidRowCode(channel), b2);
}

void MockDataSource::AddMidRowCode(const bool underline, const bool italics, const int channel)
{
  ClockIt();

  // it's a bit messy, but for this overload, italics must be true
  assert(italics);

  byte_t b2 = underline ? 0x2F : 0x2E;

  SaveBytePair(GetFirstByteMidRowCode(channel), b2);
}

void MockDataSource::AddDeleteToEndOfRow(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);

  SaveBytePair(GetFirstByteMiscCommand(channel), 0x23);
}

void MockDataSource::AddFlashOn(const int channel)
{
  ClockIt();

  SaveBytePair(GetFirstByteMiscCommand(channel), 0x28);
}

void MockDataSource::AddResumeDirectCaptioning(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x29);
}

void MockDataSource::AddEraseDisplayedMemory(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x2C);
}

void MockDataSource::AddEraseNonDisplayedMemory(const int channel)
{
  ClockIt();
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x2E);
}
void MockDataSource::AddCarriageReturn(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x2D);
}

void MockDataSource::AddEndOfCaption(const unsigned long long delayInMilliseconds, const int channel)
{
  ClockIt(delayInMilliseconds);
  SaveBytePair(GetFirstByteMiscCommand(channel), 0x2F);
}

void MockDataSource::AddTabOffset(const BYTE columns, const int channel)
{
  ClockIt();

  assert((columns > 0) && (columns < 4));
  assert((channel == 1) || (channel == 2));

  byte_t first = (channel == 1) ? 0x17 : 0x1F;

  SaveBytePair(first, 0x20 + columns);
}

void MockDataSource::SaveBytePair(const byte_t byte1, const byte_t byte2)
{
  // save the current running add clock as the timestamp for this data
  auto p = BytePair(AddOddParityBit(byte1), AddOddParityBit(byte2), _addClock);
  _byteCodes.push(p);
}

BytePair MockDataSource::GetNextPair()
{
  if (_byteCodes.empty())
  {
    auto p = BytePair(0, 0, _addClock);
    p.valid = false;
    return p;
  }

  auto p = _byteCodes.front();
  _byteCodes.pop();

  return p;
}

byte_t MockDataSource::AddOddParityBit(byte_t b) const
{
  b &= 0x7F;  // ensure no parity bit starting out

  byte_t temp = b; // save the original value (before we shift it to zero)

  byte_t sum = 0;  // count the bits!

  while (b != 0)
  {
    if ((b & 0x01) == 1)
      ++sum;

    b = b >> 1;
  }

  if (sum % 2 == 0)  // if an even number of bits...
    temp += 0x80;    // set the parity bit to make the number of bits odd

  return temp;
}