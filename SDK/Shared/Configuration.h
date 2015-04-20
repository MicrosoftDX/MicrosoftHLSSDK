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


#define SEGMENT_DNLD_BUFFSIZE 10 * 1024

#include "pch.h"
#include <memory>

#include "Interfaces.h"
#include "TSConstants.h"

namespace Microsoft {
  namespace HLSClient {
    namespace Private {
      ///<summary>Singleton that holds configuration information</summary>
      class Configuration
      {
      public:
        //Singleton instance
        static std::shared_ptr<Configuration> _current;
        unsigned long long PreFetchLengthInTicks;
        //length of the look ahead buffer in ticks
        unsigned long long LABLengthInTicks; 
        unsigned long long MinimumLiveLatency;
        //bitrate change notification interval in milliseconds
        unsigned long long BitrateChangeNotificationInterval;
        //bitrate monitor enablement flag
        bool EnableBitrateMonitor;  
        unsigned int SegmentTryLimitOnBitrateSwitch; 
        float MinimumPaddingForBitrateUpshift;
        float MaximumToleranceForBitrateDownshift;
        bool ForceKeyFrameMatchOnSeek;  
        //bool ForceKeyFrameMatchOnBitrateSwitch;
        bool AllowSegmentSkipOnSegmentFailure;
        bool AutoAdjustScrubbingBitrate;
        bool AutoAdjustTrickPlayBitrate;
        bool UseTimeAveragedNetworkMeasure;
        bool UpshiftBitrateInSteps;
        bool ResumeLiveFromPausedOrEarliest;
        Microsoft::HLSClient::SegmentMatchCriterion MatchSegmentsUsing;
        ContentType ContentTypeFilter; 
        bool TryEnsureSeamlessBitrateSwitch;
        static std::shared_ptr<Configuration> GetCurrent()
        {
          if (_current == nullptr)
            _current = std::make_shared<Configuration>();
          return _current;
        }


        Configuration() :
          PreFetchLengthInTicks(0),
          MinimumLiveLatency(0),
          LABLengthInTicks(0), 
          SegmentTryLimitOnBitrateSwitch(2),
          BitrateChangeNotificationInterval(15 * (unsigned long long)10000000),
          EnableBitrateMonitor(true), 

#if WINVER < 0x0A00 && WINAPI_FAMILY==WINAPI_FAMILY_PC_APP //Win 8.1 Non phone
            MinimumPaddingForBitrateUpshift(0.35f),
#elif WINVER >= 0x0A00 && WINAPI_FAMILY!=WINAPI_FAMILY_PHONE_APP //Win 10 non Phone
            MinimumPaddingForBitrateUpshift(0.35f),
#else
            MinimumPaddingForBitrateUpshift(0.75f),
#endif 

 
          TryEnsureSeamlessBitrateSwitch(true), 
          MaximumToleranceForBitrateDownshift(0.0f),
          AllowSegmentSkipOnSegmentFailure(true),
          ForceKeyFrameMatchOnSeek(true),  
          AutoAdjustScrubbingBitrate(false),
          AutoAdjustTrickPlayBitrate(true),
          UseTimeAveragedNetworkMeasure(false),

#if WINVER < 0x0A00 && WINAPI_FAMILY==WINAPI_FAMILY_PC_APP //win 8.1 Non phone
            UpshiftBitrateInSteps(false),
#elif WINVER >= 0x0A00 && WINAPI_FAMILY!=WINAPI_FAMILY_PHONE_APP //win 10 non phone
            UpshiftBitrateInSteps(false),
#else
            UpshiftBitrateInSteps(true),
#endif  
          ResumeLiveFromPausedOrEarliest(true),
          ContentTypeFilter(ContentType::UNKNOWN),
          MatchSegmentsUsing(SegmentMatchCriterion::SEQUENCENUMBER)
        {
           
        }

        void SetLABLengthInTicks(unsigned long long ticks)
        {
          LABLengthInTicks = ticks;
           
        }
        
        unsigned long long GetRateAdjustedLABThreshold(float PlaybackRate)
        {
          if (PlaybackRate >= 0.0F && PlaybackRate <= 1.0F)
            return LABLengthInTicks;
          else
          {
            auto abspr = abs(PlaybackRate);
            return (unsigned long long)(abspr) * LABLengthInTicks;
          }
        }

      };


    }
  }
} 