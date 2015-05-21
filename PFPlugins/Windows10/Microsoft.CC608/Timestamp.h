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