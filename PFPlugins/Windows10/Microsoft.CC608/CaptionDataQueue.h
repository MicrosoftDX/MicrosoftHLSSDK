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