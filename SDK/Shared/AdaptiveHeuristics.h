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

#define CHECKBITRATE_MIN_BYTES 1024 * 1024
#include "pch.h"
#include <functional>  
#include <tuple>
#include <unordered_map> 
#include <memory>
#include <mutex>
#include <algorithm>
#include "Configuration.h"
#include "StopWatch.h"  
#include "FileLogger.h"
#include "TaskRegistry.h"

using namespace std;

namespace Microsoft
{
  namespace HLSClient
  {
    namespace Private
    {


      class CHLSMediaSource;

      class DownloadDataPoint
      {
      public:
        long long AtElapsed;
        unsigned long long TotalBytes;
        unsigned long long BytesSinceLastUpdate;

        DownloadDataPoint(long long atElapsed, unsigned long long totalBytes, unsigned long long bytesSinceLastUpdate) :
          AtElapsed(atElapsed), TotalBytes(totalBytes), BytesSinceLastUpdate(bytesSinceLastUpdate)
        {

        }

      };

      class DownloadEntry
      {
      public:
        std::vector<DownloadDataPoint> Data;
        unsigned long long TotalBytes;
        long long MaxElapsed, MinElapsed;
        bool Completed;
        unsigned long long LastCheckedByteThreshold;

        double RunningRate;
        DownloadEntry() {};

        DownloadEntry(long long atElapsed) :
          TotalBytes(0), MaxElapsed(0), Completed(false), RunningRate(0),
          MinElapsed(atElapsed), LastCheckedByteThreshold(0)
        {}

        void AddEntry(long long atElapsed, unsigned long long bytes)
        {
          Data.push_back(DownloadDataPoint(atElapsed, bytes, bytes - TotalBytes));

          LastCheckedByteThreshold += bytes - TotalBytes;
          TotalBytes = bytes;
          MaxElapsed = atElapsed;
        }

        bool IsCheckPoint()
        {
          if (LastCheckedByteThreshold >= CHECKBITRATE_MIN_BYTES)
          {
            LastCheckedByteThreshold = 0;
            return true;
          }
          else
            return false;
        }




        double GetRateInSpan(long long spanStart, long long spanEnd)
        {
          if (MaxElapsed < spanStart || MinElapsed > spanEnd) //no overlap
            return 0;

          double retval = ((double)TotalBytes * 8 / ((double)(MaxElapsed - MinElapsed) / 10000000));
          if (MinElapsed >= spanStart && MaxElapsed <= spanEnd) //full overlap
            return retval;

          //if neither we approximate assuming uniform prorities being given to all parallel downloads i.e. each of them downloading 
          //at 1/x of the total possible rate when there are x downloads going


          if (MinElapsed >= spanStart && MaxElapsed > spanEnd)
          {
            unsigned long long TotBytes = 0;
            for (auto ddp : Data)
            {
              if (ddp.AtElapsed > spanEnd) break;
              TotBytes += ddp.BytesSinceLastUpdate;
            }
            return ((double)TotBytes * 8 / ((double)(spanEnd - MinElapsed) / 10000000));
          }

          else if (MinElapsed < spanStart && MaxElapsed <= spanEnd)
          {
            unsigned long long TotBytes = 0;
            for (auto ddp : Data)
            {
              if (ddp.AtElapsed < spanStart) continue;
              TotBytes += ddp.BytesSinceLastUpdate;
            }

            return ((double)TotBytes * 8 / ((double)(MaxElapsed - spanStart) / 10000000));
          }

          return 0;
        }


      };
      ///<summary>Singleton that encapsulates the logic for network monitoring and bitrate switching</summary>
      class HeuristicsManager
      {

      private:

        //list of available bitrates from a playlist
        std::vector<unsigned int> Bandwidths;

        //bitrate boundaries 
        unsigned int MaxBound, MinBound;
        //last bitrate that was suggested 
        unsigned int LastSuggestedBandwidth;
        unsigned int LastMeasuredBandwidth;
        //history of bitrates from every download measure over the measuring period
        std::vector<double> BitrateHistory;
        //stopwatch to control bitrate switch notifications
        StopWatch tickstopwatch;
        CHLSMediaSource *pms;
        recursive_mutex LockAccess;
        function<void()> OnNotifierTick;
        bool IgnoreDownshiftTolerance;
        TaskRegistry<HRESULT> _notificationtasks;
        shared_ptr<StopWatch> swDownloadMeasure;
        std::unordered_map<std::wstring, DownloadEntry> DownloadMeasureData;

      public:
        //handler for bitrate change notification
        function<void(unsigned int Bandwidth, unsigned int LastMeasured, bool& Cancel)> BitrateSwitchSuggested;

        ///<summary>Constructor</summary>
        HeuristicsManager(CHLSMediaSource *ptrms);

        ///<summary>Destructor</summary>
        ~HeuristicsManager()
        {
          StopNotifier();
          Bandwidths.clear();
          BitrateHistory.clear();
        }

        void SetIgnoreDownshiftTolerance(bool val)
        {
          IgnoreDownshiftTolerance = val;
        }

        bool GetIgnoreDownshiftTolerance()
        {
          return IgnoreDownshiftTolerance;
        }


        ///<summary>Starts measuring a download</summary>
        std::wstring StartDownloadMeasure(bool ActiveVariant = true)
        {

          
          //create a GUID to represent the unique ID for the stopwatch
          WCHAR buff[128];
          GUID id = GUID_NULL;
          CoCreateGuid(&id);
          ::StringFromGUID2(id, buff, 128);
          wstring ret = std::wstring(buff);

          if (!tickstopwatch.IsTicking) return ret;

          std::lock_guard<std::recursive_mutex> lock(LockAccess);
          DownloadMeasureData.emplace(ret, DownloadEntry(swDownloadMeasure->GetElapsed()));


          //return the stopwatch id
          return ret;
        }

        double CalculateRate(wstring measureID)
        {

          unsigned long long TotBytes = DownloadMeasureData[measureID].TotalBytes;
          long long elapsed = DownloadMeasureData[measureID].MaxElapsed - DownloadMeasureData[measureID].MinElapsed;
          double rate = ((double)TotBytes * 8) / ((double)elapsed / 10000000);

          {
            std::lock_guard<std::recursive_mutex> lock(LockAccess);
            if (DownloadMeasureData.size() > 0)
            {
              for (auto download : DownloadMeasureData)
              {
                if (download.first != measureID)
                {
                  //TotBytes += download.second.GetBytesInSpan(DownloadMeasureData[measureID].MinElapsed, DownloadMeasureData[measureID].MaxElapsed);
                  rate += download.second.GetRateInSpan(DownloadMeasureData[measureID].MinElapsed, DownloadMeasureData[measureID].MaxElapsed);
                }
              }
            }
          }

          //calculate bitrate
          return rate;// ((double) TotBytes * 8) / ((double) elapsed / 10000000);
        }

        void UpdateDownloadMeasure(std::wstring measureID, unsigned long long BytesDownloaded, bool CurrentVariant = true)
        {
          if (!tickstopwatch.IsTicking) return;

          std::lock_guard<std::recursive_mutex> lock(LockAccess);

          swDownloadMeasure->Pause();
          DownloadMeasureData[measureID].AddEntry(swDownloadMeasure->GetElapsed(), BytesDownloaded);
          swDownloadMeasure->Resume();

          //calculate bitrate
          if (CurrentVariant && DownloadMeasureData[measureID].IsCheckPoint())
          {
            DownloadMeasureData[measureID].RunningRate = CalculateRate(measureID);
            if (IsBitrateChangeNotifierRunning() && DownloadMeasureData[measureID].RunningRate < LastSuggestedBandwidth)
              NotifyBitrateChangeIfNeeded(DownloadMeasureData[measureID].RunningRate, DownloadMeasureData[measureID].RunningRate);
          }


        }


        void CompleteDownloadMeasure(std::wstring measureID, bool Discard = false, bool CurrentVariant = true)
        {

          if (!tickstopwatch.IsTicking) return;
          std::lock_guard<std::recursive_mutex> lock(LockAccess);

          DownloadMeasureData[measureID].Completed = true;

          if (CurrentVariant && !Discard)
          {
            DownloadMeasureData[measureID].RunningRate = CalculateRate(measureID);

            if (IsBitrateChangeNotifierRunning() && !Configuration::GetCurrent()->UseTimeAveragedNetworkMeasure ||
              DownloadMeasureData[measureID].RunningRate < LastSuggestedBandwidth)
            {
              NotifyBitrateChangeIfNeeded(DownloadMeasureData[measureID].RunningRate, DownloadMeasureData[measureID].RunningRate);
            }
            else
            {
              //store bitrate in history 
              BitrateHistory.push_back(DownloadMeasureData[measureID].RunningRate);
              LastMeasuredBandwidth = (unsigned int)BitrateHistory.back();
            }
          }

          CleanupDownloadHistory();

          return;

        }

        void CleanupDownloadHistory()
        {
          if (DownloadMeasureData.size() == 1 ||
            std::find_if(DownloadMeasureData.begin(), DownloadMeasureData.end(), [this](std::pair<std::wstring, DownloadEntry> v)
          {
            return  v.second.Completed == false;
          }) == DownloadMeasureData.end())
          {
            DownloadMeasureData.clear();
            return;
          }

          //get the minimum start point for all active downloads
          auto itrMinMinElapsed =
            std::min_element(DownloadMeasureData.begin(), DownloadMeasureData.end(), [this](std::pair<std::wstring, DownloadEntry> v1, std::pair<std::wstring, DownloadEntry> v2)
          {
            return v1.second.MinElapsed < v2.second.MinElapsed && (v1.second.Completed == false && v2.second.Completed == false);
          });

          if (itrMinMinElapsed == DownloadMeasureData.end())
            return;

          std::vector<wstring> toRemove;
          for (auto p : DownloadMeasureData)
          {
            if (p.second.MaxElapsed < (*itrMinMinElapsed).second.MinElapsed && p.second.Completed)
              toRemove.push_back(p.first);
          }
          for (auto id : toRemove)
            DownloadMeasureData.erase(id);
        }

        ///<summary>Start the bitrate change notifier</summary>
        void StartNotifier()
        {
          if (tickstopwatch.IsTicking) //nothing to do
            return;
          //if bitrate monitoring is turned off
          if (Configuration::GetCurrent()->EnableBitrateMonitor == false)
            return;
          //set handler to handle the tick event

          //tickstopwatch.TickEvent = [this](){ OnNotifierTick(); };
          //set the frequency to the specified notification interval
          tickstopwatch.TickEventFrequency = Configuration::GetCurrent()->BitrateChangeNotificationInterval;
          //start the notifier stopwatch

          if (swDownloadMeasure == nullptr)
          {
            swDownloadMeasure = std::make_shared<StopWatch>();
            swDownloadMeasure->Start();
          }

          tickstopwatch.StartTicking(OnNotifierTick);

          //LOG("Bitrate Notifier Started");
        }


        bool IsBitrateChangeNotifierRunning() { return tickstopwatch.IsTicking; }
        ///<summary>Stop the bitrate change notifier</summary>
        void StopNotifier()
        {

          if (tickstopwatch.IsTicking)
          {
            //stop the stopwatch
            tickstopwatch.StopTicking();
            //LOG("Bitrate Notifier Stopped");

            swDownloadMeasure->Stop();
          }

          _notificationtasks.CancelAll();
          _notificationtasks.WhenAll();

        }

        ///<summary>Returns the last suggested bandwidth</summary>
        unsigned int GetLastSuggestedBandwidth()
        {
          std::lock_guard<std::recursive_mutex> lock(LockAccess);
          return LastSuggestedBandwidth;
        }

        unsigned int GetLastMeasuredBandwidth()
        {
          return LastMeasuredBandwidth;
        }

        ///<summary>Sets the last suggested bandwidth</summary>
        void SetLastSuggestedBandwidth(unsigned int Bandwidth)
        {
          std::lock_guard<std::recursive_mutex> lock(LockAccess);
          LastSuggestedBandwidth = Bandwidth;
        }

        ///<summary>Sets maximum and minimum allowed bandwidth bounds. The supplied parameters do not have 
        //to exactly match bitrates specified by the variants in the playlist. The method clamps them to 
        //appropriate boundary values accordingly.</summary>
        void SetBandwidthBounds(unsigned int Max, unsigned int Min)
        {
          //if the supplied min and max are equal
          if (Min == Max)
          {
            //set the min and max bounds and the last suggested bandwidth to the closest bitrate matching the max
            MinBound = MaxBound = LastSuggestedBandwidth = FindClosestBitrate(Max);
          }
          else
          {
            //if the supplied min is greater than the lowest variant
            if (Min > Bandwidths.front())
            {
              //set the min bound to the smallest variant bitrate that is greater than or equal to the supplied min. 
              //We should never let bitrate travel below the supplied min - so the lowest valid bitrate we can go down to 
              //is one that is immediately greater than or equal to the supplied min.
              MinBound = *(std::find_if(Bandwidths.begin(), Bandwidths.end(), [Min, this](unsigned int val)
              {
                return val >= Min;
              }));
            }
            //if the supplied max is lesser than the lowest variant
            if (Max < Bandwidths.back())
            {
              //set the max bound to the largest variant bitrate that is less than or equal to the supplied max. 
              //We should never let bitrate travel above the supplied max - so the largest valid bitrate we can go up to 
              //is one that is immediately smaller than or equal to the supplied max.
              MaxBound = *(std::find_if(Bandwidths.rbegin(), Bandwidths.rend(), [Max, this](unsigned int val)
              {
                return val <= Max;
              }));
            }
            //set the last suggested bandwidth to the bitrate closest to one midway between the min and max bounds.
            LastSuggestedBandwidth = FindClosestBitrate(LastSuggestedBandwidth);
          }
        }

        ///<summary>Set the bitrate range (from the variant master playlist) that the monitor will work with</summary>
        void SetBandwidthRange(std::vector<unsigned int> bitrates)
        {
          LastSuggestedBandwidth = bitrates.front();; //FindClosestBitrate((unsigned int)ceil((MinBound + MaxBound)/2));
          Bandwidths = bitrates;
          std::sort(Bandwidths.begin(), Bandwidths.end());
          //set the initial min bound value to the smallest in the range
          MinBound = Bandwidths.front();
          //set the initial max bound value to the largest in the range
          MaxBound = Bandwidths.back();
        }

        unsigned int FindNextLowerBitrate(unsigned int Bitrate)
        {
          unsigned int retval = 0;
          //the bounds are equal - we clamp to either one
          if (MinBound == MaxBound)
            retval = MinBound;
          else
          {
            //supplied bitrate less than min bound - min bound is the lowest we can go 
            if (Bitrate <= MinBound)
              retval = MinBound;
            //supplied bitrate larger than max bound - max bound is the highest we can go
            else
            {
              auto found = std::find_if(Bandwidths.rbegin(), Bandwidths.rend(), [Bitrate](unsigned int br) { return br < Bitrate; });
              if (found != Bandwidths.rend())
              {
                retval = __max(MinBound, *found);
              }
            }
            if (retval == 0) //should not happen - but in case it does - pick lowest and start
              retval = MinBound;
          }
          return retval;
        }

        unsigned int FindNextHigherBitrate(unsigned int Bitrate)
        {
          unsigned int retval = 0;
          //the bounds are equal - we clamp to either one
          if (MinBound == MaxBound)
            retval = MinBound;
          else
          {
            //supplied bitrate greater than max bound - max bound is the highest we can go 
            if (Bitrate >= MaxBound)
              retval = MaxBound;
            //supplied bitrate larger than max bound - max bound is the highest we can go
            else
            {
              auto found = std::find_if(Bandwidths.begin(), Bandwidths.end(), [Bitrate](unsigned int br) { return br > Bitrate; });
              if (found != Bandwidths.end())
              {
                retval = *found;
              }
            }
            if (retval == 0) //should not happen - but in case it does - pick lowest and start
              retval = MinBound;
          }
          return retval;
        }

        ///<summary>Find the closest bitrate to the given value. Closest bitrate is calculated as follows:
        /// if given bitrate n falls between x and y then we return x if n is closer to x than it is to y, or else we return y.
        ///</summary>
        unsigned int FindClosestBitrate(unsigned int Bitrate, bool IgnoreBounds = false)
        {
          unsigned int retval = 0;
          if (IgnoreBounds == true)
          {
            //loop over bitrate range starting at min bound and ending at max bound
            for (auto itr = Bandwidths.begin(); itr < Bandwidths.end(); itr++)
            {
              //if we have not reached the end and bitrate falls in between current and next
              if (itr != Bandwidths.end() && (Bitrate >= *itr && Bitrate <= *(itr + 1)))
              {
                //return whichever one the bitrate is closer to
                retval = (Bitrate - *itr) < (*(itr + 1) - Bitrate) ? *itr : *(itr + 1);
                break;
              }
            }
            if (retval == 0) //should not happen - but in case it does - pick lowest and start
              retval = Bandwidths.front();
          }
          else
          {
            //the bounds are equal - we clamp to either one
            if (MinBound == MaxBound)
              retval = MaxBound;
            else
            {
              //supplied bitrate less than min bound - min bound is the lowest we can go 
              if (Bitrate < MinBound)
                retval = MinBound;
              //supplied bitrate larger than max bound - max bound is the highest we can go
              else if (Bitrate > MaxBound)
                retval = MaxBound;
              else
              {
                auto startitr = std::find(Bandwidths.begin(), Bandwidths.end(), MinBound);
                auto enditr = std::find(Bandwidths.begin(), Bandwidths.end(), MaxBound);
                //loop over bitrate range starting at min bound and ending at max bound
                for (auto itr = startitr; itr < enditr; itr++)
                {
                  //if we have not reached the end and bitrate falls in between current and next
                  if (itr != enditr && (Bitrate >= *itr && Bitrate <= *(itr + 1)))
                  {
                    //return whichever one the bitrate is closer to
                    retval = (Bitrate - *itr) < (*(itr + 1) - Bitrate) ? *itr : *(itr + 1);
                    break;
                  }
                }
              }
              if (retval == 0) //should not happen - but in case it does - pick lowest and start
                retval = MinBound;
            }
          }
          return retval;
        }

        ///<summary>Given a bitrate value, finds the appropriate bitrate from the valid range to switch to. 
        ///The rule : Since we should not go over what was suggested by the network monitor as the current download perf value, 
        //we find and return the largest bitrate smaller than the supplied value. 
        ///</summary>
        unsigned int FindBitrateToSwitchTo(double Bitrate)
        {
          unsigned int retval = 0;
          //bounds are equal i.e. only one bitrate valid - return that
          if (MinBound == MaxBound)
            retval = MaxBound;
          else
          {
            //cannot go down below min bound
            if (Bitrate <= MinBound)
              retval = MinBound;
            //cannot go above max bound
            else if (Bitrate >= (MaxBound *(1 + Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift)) && !Configuration::GetCurrent()->UpshiftBitrateInSteps)
              retval = MaxBound;
            else
            {

              //if shifting up
              if (Bitrate > LastSuggestedBandwidth)
              {
                auto startitr = std::find(Bandwidths.begin(), Bandwidths.end(), MinBound);
                auto enditr = std::find(Bandwidths.begin(), Bandwidths.end(), MaxBound);
                if (Configuration::GetCurrent()->UpshiftBitrateInSteps) //if shifting up in steps
                {
                  auto next = std::find_if(startitr, enditr, [this](unsigned int val)
                  {
                    return val > LastSuggestedBandwidth;
                  });
                  if (next != Bandwidths.end() && ((*next) * (1 + Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift) < Bitrate))
                    retval = *next;
                }
                else
                {
                  //find the largest bitrate smaller than the supplied value

                  for (auto itr = enditr; itr > startitr; itr--)
                  {
                    auto bwval = *itr;
                    //switch to the largest bitrate that is less than measured bitrate minus any padding 
                    if (itr != startitr && (bwval * (1 + Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift) < Bitrate))
                    {
                      retval = bwval;
                      break;
                    }
                  }
                }

              }
              else if (Bitrate < LastSuggestedBandwidth * (IgnoreDownshiftTolerance ? 1 : (1 - Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift)))
              {

                auto startitr = std::find(Bandwidths.begin(), Bandwidths.end(), MinBound);
                auto enditr = std::find(Bandwidths.begin(), Bandwidths.end(), MaxBound);


                //find the largest bitrate smaller than the supplied value with any padding accountted for - we let the runtime upshift once - this is done to avoid clogging the network with potential parallel downloads of the src and the target segment when bitate has gone down
                for (auto itr = enditr; itr > startitr; itr--)
                {
                  auto bwval = *itr;
                  if (itr != startitr && bwval * (1 + Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift) <= Bitrate)
                  {
                    retval = *(--itr);;// bwval;
                    break;
                  }
                }

                //find the largest bitrate two steps smaller than the supplied value - we let the runtime upshift once - this is done to avoid clogging the network with potential parallel downloads of the src and the target segment when bitate has gone down
                /*for (auto itr = enditr; itr > startitr; itr--)
                {
                auto bwval = *itr;
                if (itr != startitr && *itr <= Bitrate)
                {
                retval = *(--itr);
                break;
                }
                }*/


              }

            }
            if (retval == 0) //should not happen - but in case it does - stay at last suggested if possible or go to lowest
            {
              retval = (LastSuggestedBandwidth != 0 && LastMeasuredBandwidth > LastSuggestedBandwidth) ? LastSuggestedBandwidth : MinBound;
            }
          }

          return retval;
        }





        ///<summary>Notifies that a bitrate change should be considered</summary>
        ///<param name='bitspersec'>An average current bitrate calculated from download measure history</param>
        void NotifyBitrateChangeIfNeeded(double bitspersec, double LastMeasure, double stddev = 0);
      };

    }
  }
} 
