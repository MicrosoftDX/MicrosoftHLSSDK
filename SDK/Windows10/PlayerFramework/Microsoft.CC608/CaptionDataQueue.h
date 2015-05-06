#pragma once

#include <mutex>
#include <map>
#include <vector>

#include "pch.h"
#include "Timestamp.h"

namespace Microsoft { namespace CC608 {

  // stores incoming raw caption data--external components add data to the queue, and the 
  // processing thread gets the data as needed
  class CaptionDataQueue
  {
  typedef std::map<Timestamp, std::vector<byte_t>> captiondataqueuemap_t;

  public:
    CaptionDataQueue(void);
    ~CaptionDataQueue(void);

    void AddCaptionData(const Timestamp ts, const std::vector<byte_t>& captionByteData);
    void AddCaptionData(const std::vector<std::pair<Timestamp, std::vector<byte_t>>>& data);

    std::vector<byte_t> GetSortedCaptionData(const Timestamp startTs, const Timestamp endTs);

    // clears the data in the queue (used when seeking in a stream or when switching media sources)
    void Clear();

  private:
    std::mutex _mtx;
    captiondataqueuemap_t _map;
  };

}}