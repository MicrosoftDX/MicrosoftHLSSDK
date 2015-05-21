#include "pch.h"

#include <exception>
#include "Timestamp.h"

using namespace Microsoft::CC608;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Notes on units and projections
//
//  * C# -> WinRT projection takes a .NET TimeSpan and converts directly to WinRT TimeSpan in 100 ns ticks
//  * JS -> WinRT projection:
//     - ASSUMES that the projected value is in milliseconds (this MUST be done in JS as the fractional portion of the JS value
//       (which WinRT assumes are fractions of a millisecond) are dropped.
//     - Projects the milliseconds into a WinRT TimeSpan with 100 ns ticks
//    
// Examples:
//  * C#: a TimeSpan in .NET with 1 second and 230 ms is projected to a WinRT TimeSpan with a Duration of 12,300,000 ticks.
//  * JS: a float with 1230 ms is projected to a WinRT TimeSpan with a Duration of 12,300,000 ticks.
//
// For more information, see http://msdn.microsoft.com/en-US/library/windows/apps/windows.foundation.timespan, especially the following note:
// 
// Note   In JavaScript, this structure is treated as the number of millisecond intervals, not the number of 100-nanosecond intervals. 
// Therefore, Windows.Foundation.TimeSpan values can lose precision when being ported between languages.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const unsigned long long Timestamp::TicksPerMillisecond = 10000ULL;

// Default constructor (timeSpan of zero)
Timestamp::Timestamp(void) : _ts(0)
{
}

Timestamp::Timestamp(const Windows::Foundation::TimeSpan timeSpan) : _ts(timeSpan.Duration)
{
}

Timestamp::Timestamp(const unsigned long long ticks) : _ts(ticks)
{
}

Timestamp::~Timestamp(void)
{
}

// Returns value in 100 ns ticks
unsigned long long Timestamp::GetTicks() const
{
  return _ts;
}
