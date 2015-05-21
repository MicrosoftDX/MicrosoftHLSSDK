#include "pch.h"
#include <iterator>
#include <algorithm>
#include "CaptionDataQueue.h"

using namespace Microsoft::CC608;
using namespace std;

CaptionDataQueue::CaptionDataQueue(void) : _mtx(), _map()
{
}

CaptionDataQueue::~CaptionDataQueue(void)
{
}

// add a single timestamp of caption data
void CaptionDataQueue::AddCaptionData(const Timestamp ts, const std::vector<byte_t>& captionByteData)
{
  lock_guard<mutex> lock(_mtx);
  _map.emplace(ts, captionByteData);
}

// adds a range of caption data, deleting all existing data in the supplied Timestamp range
void CaptionDataQueue::AddCaptionData(const vector<pair<Timestamp, vector<byte_t>>>& data)
{
  if (data.size() == 0)
  {
    // no data to process
    return;
  }

  // try to get the range of Timestamps to delete
  auto minMax = std::minmax_element(data.begin(), data.end(), 
    [](pair<Timestamp, vector<byte_t>> a, pair<Timestamp, vector<byte_t>> b) 
    {
      return a.first < b.first;
    });

  // note: minmax_element returns valid iterators even if all the elements have the same value
  // (in that case, the iterators would point to the first element and the last element)

  Timestamp min = (*minMax.first).first;
  Timestamp max = (*minMax.second).first;

  // lock the map before changing it
  lock_guard<mutex> lock(_mtx);

  // clear range in existing data
  _map.erase(_map.lower_bound(min), _map.upper_bound(max));

  // add data
  for(auto& e : data)
  {
    _map.emplace(e.first, e.second);
  }
}

// clear all caption data
void CaptionDataQueue::Clear()
{
  lock_guard<mutex> lock(_mtx);
  _map.clear();
}

// return caption data for the specified timestamp range
std::vector<byte_t> CaptionDataQueue::GetSortedCaptionData(const Timestamp startTs, const Timestamp endTs)
{
  lock_guard<mutex> lock(_mtx);
  std::vector<byte_t> result;

  if (endTs >= startTs)
  {
    // find all timestamps in the range
    for (captiondataqueuemap_t::iterator i = _map.lower_bound(startTs); i != _map.upper_bound(endTs); ++i)
    {
      // add caption data to the result
      result.reserve(result.size() + i->second.size());
      copy(i->second.begin(), i->second.end(), std::back_inserter(result));
    }

    // remove the result range from the queue
    _map.erase(_map.lower_bound(startTs), _map.upper_bound(endTs));
  }

  return result;
}
