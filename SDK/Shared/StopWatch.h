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


#include <string>
#include <functional>
#include <ppltasks.h> 
#include <windows.system.threading.h> 

using namespace concurrency;
using namespace std;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      ///<summary>Implements a timer as well as a stopwatch API for general purpose elapsed time measurement</summary>
      class StopWatch
      {
      private:
        long long hiresctr_freq;
        long long hiresctr_startcount;
        long long hiresctr_pausestartcount;
        long long _startticks;
        long long _endticks;
        long long _pausestartticks;
        long long _totalpauseticks;
        bool usinghires_ctr;
        std::wstring _id;
        bool Paused;
        Windows::System::Threading::IThreadPoolTimer^ cpTimer;

      public:
        ///<summary>Handler for a timer tick event</summary>
        // function<void()> TickEvent;
        ///<summary>Interval in milliseconds at which the timer tick event is raised</summary>
        unsigned long long TickEventFrequency;
        ///<summary>Stopwatch state flag</summary>
        bool IsTicking;

        ///<summary>Constructor</summary>
        StopWatch() : usinghires_ctr(true), hiresctr_freq(0), hiresctr_startcount(0), hiresctr_pausestartcount(0),
          _startticks(0), _endticks(0), _pausestartticks(0), _totalpauseticks(0), Paused(false), TickEventFrequency(0), IsTicking(false) {

          //create a GUID to represent the unique ID for the stopwatch
          WCHAR buff[128];
          GUID id = GUID_NULL;
          CoCreateGuid(&id);
          ::StringFromGUID2(id, buff, 128);
          _id = std::wstring(buff);

        };

        ///<summary>Destructor</summary>
        ~StopWatch()
        {
          if (IsTicking)
            StopTicking();
        }

        ///<summary>Return the unique ID</summary>
        std::wstring GetID()
        {
          return _id;
        }

        ///<summary>Starts the timer</summary>
        HRESULT StartTicking(function<void()> tickEvent)
        {

          Windows::Foundation::TimeSpan tsInterval;
          tsInterval.Duration = (long long) TickEventFrequency;//100 ns ticks
          cpTimer = Windows::System::Threading::ThreadPoolTimer::CreatePeriodicTimer(ref new Windows::System::Threading::TimerElapsedHandler(
            [tickEvent](Windows::System::Threading::IThreadPoolTimer ^timer)
          {
            tickEvent();
            return;
          }), tsInterval);
          if (cpTimer == nullptr) return E_FAIL;
          //set state
          IsTicking = true;
          return S_OK;
        }

        ///<summary>Stop the timer</summary>
        void StopTicking()
        {
          //set state
          IsTicking = false;
          if (cpTimer)
          {
            cpTimer->Cancel();
          }
        }

        HRESULT StartTickingOnce(function<void()> tickEvent)
        {

          Windows::Foundation::TimeSpan tsInterval;
          tsInterval.Duration = (long long) TickEventFrequency;//100 ns ticks
          cpTimer = Windows::System::Threading::ThreadPoolTimer::CreateTimer(ref new Windows::System::Threading::TimerElapsedHandler(
            [tickEvent](Windows::System::Threading::IThreadPoolTimer ^timer)
          {
            tickEvent();
            return;
          }), 
          tsInterval,
          ref new Windows::System::Threading::TimerDestroyedHandler([this](Windows::System::Threading::IThreadPoolTimer ^timer)
          {
            IsTicking = false;
          }));
          if (cpTimer == nullptr) return E_FAIL;
          //set state
          IsTicking = true;
          return S_OK;
        }

        

        ///<summary>Starts the stopwatch</summary>
        ///<returns>Returns stopwatch ID</returns>
        std::wstring Start()
        {
          LARGE_INTEGER lifreq;
          //does the platform support the hires perf counter ?
          if (QueryPerformanceFrequency(&lifreq))
          {
            //note frequency
            hiresctr_freq = lifreq.QuadPart;
            LARGE_INTEGER licount;
            //query counter initially
            if (QueryPerformanceCounter(&licount))
              //store start count
              hiresctr_startcount = licount.QuadPart;
            else //if query was unsuccessful
              //fall back to non hi res mechansim
              return NonHiRes_Start();

            //return the unique id
            return _id;
          }
          else //hi res counter not supported
            //use non hi res method
            return  NonHiRes_Start();
        }

        ///<summary>Pause the stop watch</summary>
        void Pause()
        {
          //if not already paused
          if (!Paused)
          {
            //are we using hi res ?
            if (usinghires_ctr)
            {
              LARGE_INTEGER licount;
              //query and store current count before pausing
              QueryPerformanceCounter(&licount);
              hiresctr_pausestartcount = licount.QuadPart;
            }
            else //non hi res 
            {
              FILETIME filetime;
              //get system time
              GetSystemTimeAsFileTime(&filetime);
              ULARGE_INTEGER tmp;
              tmp.LowPart = filetime.dwLowDateTime;
              tmp.HighPart = filetime.dwHighDateTime;
              //store
              _pausestartticks = tmp.QuadPart;
            }
            //set state
            Paused = true;
          }
        }

        ///<summary>Resume a paused stop watch</summary>
        void Resume()
        {
          //if correct state
          if (Paused)
          {
            //using hi res
            if (usinghires_ctr)
            {
              LARGE_INTEGER licount;
              //get counter value
              QueryPerformanceCounter(&licount);
              //add to cumulative pause counter
              _totalpauseticks += (licount.QuadPart - hiresctr_pausestartcount);
              //reset pause count
              hiresctr_pausestartcount = 0;
            }
            else //non hi res
            {
              FILETIME filetime;
              //get system time
              GetSystemTimeAsFileTime(&filetime);
              ULARGE_INTEGER tmp;
              tmp.LowPart = filetime.dwLowDateTime;
              tmp.HighPart = filetime.dwHighDateTime;
              //add to cumulative pause counter
              _totalpauseticks += (tmp.QuadPart - _pausestartticks);
              //reset pause count
              _pausestartticks = 0;
            }
            //set state flag
            Paused = false;
          }
        }

        ///<summary>Stops a stop watch</summary>
        ///<returns>The elapsed time in ticks</returns>
        long long Stop()
        {
          //if we are paused - resume
          if (Paused)
            Resume();

          //using hi res
          if (usinghires_ctr)
          {
            return GetElapsed();
          }
          else //use non hi res method
            return NonHiRes_Stop();
        }

        long long GetElapsed()
        {
          if (usinghires_ctr)
          {
            LARGE_INTEGER licount;
            //query
            QueryPerformanceCounter(&licount);
            //return the difference between the last count and the start count (subtracting any pause time) - converted to ticks 
            return ((licount.QuadPart - hiresctr_startcount - _totalpauseticks) * 10000000 / hiresctr_freq);
          }
          else //use non hi res method
          {
            FILETIME filetime;
            //get system time
            GetSystemTimeAsFileTime(&filetime);
            ULARGE_INTEGER tmp;
            tmp.LowPart = filetime.dwLowDateTime;
            tmp.HighPart = filetime.dwHighDateTime;

            //return the difference between the last count and the start count (subtracting any pause time)
            return tmp.QuadPart - _startticks - _totalpauseticks;
          }
        }
      private:
        ///<summary>Start a non hi res method to track elapsed time</summary>
        std::wstring NonHiRes_Start()
        {
          //set state flag
          usinghires_ctr = false;

          FILETIME filetime;
          //get system time
          GetSystemTimeAsFileTime(&filetime);
          ULARGE_INTEGER tmp;
          tmp.LowPart = filetime.dwLowDateTime;
          tmp.HighPart = filetime.dwHighDateTime;
          //store start time
          _startticks = tmp.QuadPart;
          //return stopwatch ID
          return _id;
        }
        ///<summary>Stops a stop watch using a non hi res method of tracking</summary>
        ///<returns>The elapsed time in ticks</returns>
        long long NonHiRes_Stop()
        {
          auto ret = GetElapsed();
          //rest stopwatch back to using hi res if possible
          usinghires_ctr = true;
          return ret;

        }
      };
    }
  }
} 
