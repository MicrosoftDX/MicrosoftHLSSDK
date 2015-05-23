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

namespace Microsoft { namespace CC608 {

// Simple class to represent a media timestamp--a point in the media playback
// All ticks in this class are 100 nanosecond (10^-7 s) ticks
class Timestamp
{
public:
  Timestamp(void);
  Timestamp(const Windows::Foundation::TimeSpan timeSpan);
  Timestamp(const unsigned long long ticks);

  unsigned long long GetTicks() const;

  ~Timestamp(void);

  static const unsigned long long TicksPerMillisecond;

private:
  unsigned long long _ts;
};



// Non-member comparison operators:

inline bool operator==(const Timestamp& lhs, const Timestamp& rhs)
{
  return (lhs.GetTicks() == rhs.GetTicks());
}

inline bool operator!=(const Timestamp& lhs, const Timestamp& rhs)
{
  return !operator==(lhs, rhs);
}

inline bool operator<(const Timestamp& lhs, const Timestamp& rhs)
{
  return (lhs.GetTicks() < rhs.GetTicks());
}

inline bool operator>(const Timestamp& lhs, const Timestamp& rhs)
{
  return operator<(rhs, lhs);
}

inline bool operator<=(const Timestamp& lhs, const Timestamp& rhs)
{
  return !operator>(lhs, rhs);
}

inline bool operator>=(const Timestamp& lhs, const Timestamp& rhs)
{
  return !operator<(lhs, rhs);
}


}}