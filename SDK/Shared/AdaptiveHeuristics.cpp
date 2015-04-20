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
#include <mutex>
#include "AdaptiveHeuristics.h"
#include "HLSMediaSource.h" 
#include <windows.networking.h>
#include <windows.networking.connectivity.h> 

using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;

///<summary>Constructor</summary>
HeuristicsManager::HeuristicsManager(CHLSMediaSource *ptrms) : MaxBound(UINT32_MAX), MinBound(0),
LastSuggestedBandwidth(0), pms(ptrms), LastMeasuredBandwidth(0), IgnoreDownshiftTolerance(false)
{
  OnNotifierTick = [this]()
  {
    std::lock_guard<std::recursive_mutex> lock(LockAccess);

    size_t size = BitrateHistory.size();
    //if we have  bitrate history of at least 1 samples
    if (size > 0)
    {
      double avg = 0;
      std::wostringstream bravg;
      //average the bitrates in history
      std::for_each(BitrateHistory.begin(), BitrateHistory.end(), [&avg, &bravg, size, this](double rate)
      {

        avg += (rate / size);
        bravg << (double) (rate / 1024) << L" kbps,";
      });

      double stddev = 0;
      if (size >= 3)
      {
        double variance = 0;
        std::for_each(BitrateHistory.begin(), BitrateHistory.end(), [avg, size, &variance, this](double rate)
        {
          variance += pow((rate - avg), 2) / pow(size, 2);
        });

       stddev = sqrt(variance);

      }
      //LOG("** Averaging Bitrate **  = " << avg / 1024 << " kbps, Standard Deviation = " << stddev / 1024 << " kbps");
      auto lastmeasure = BitrateHistory.back();
      BitrateHistory.clear();
      //notify if needed
      if (Configuration::GetCurrent()->UseTimeAveragedNetworkMeasure)
        NotifyBitrateChangeIfNeeded(avg, lastmeasure, stddev);

    }
  };
}

///<summary>Notifies that a bitrate change should be considered</summary>
///<param name='bitspersec'>An average current bitrate calculated from download measure history</param>
void HeuristicsManager::NotifyBitrateChangeIfNeeded(double bitspersec, double lastmeasure, double stddev)
{

  //find a matching bitrate from valid range
  LastMeasuredBandwidth = (unsigned int) lastmeasure;

  if (!BitrateSwitchSuggested || Configuration::GetCurrent()->EnableBitrateMonitor == false || (pms->GetCurrentState() != MSS_STARTED && pms->GetCurrentState() != MSS_BUFFERING))
    return;

  unsigned int suggestion = FindBitrateToSwitchTo(bitspersec);

  if (stddev > 0 && suggestion > LastSuggestedBandwidth && stddev > bitspersec * 0.25 && //going up & standard deviation is more than 25% of the suggested target rate
    bitspersec < 1.5 * suggestion) //not too much wiggle room
  {
    return;
  }


  LOG("NotifyBitrateChangeIfNeeded : Reported - " << bitspersec << ", Last Suggested : " << LastSuggestedBandwidth << ", New Suggestion : " << suggestion << ",Upshift Padding : " << (double) Configuration::GetCurrent()->MinimumPaddingForBitrateUpshift * 100 << " %, Downshift Tolerance : " << (double) Configuration::GetCurrent()->MaximumToleranceForBitrateDownshift * 100 << " %");
  //if this is not the same as the last bitrate we suggested
  if (suggestion != LastSuggestedBandwidth)
  {
    auto oldBandwidth = LastSuggestedBandwidth;
    //set the last suggested
    LastSuggestedBandwidth = suggestion;

    //if anyone is listening
    if (BitrateSwitchSuggested)
    {
      auto mstemp = pms;
      cancellation_token_source cts;
 

      _notificationtasks.Register(task<HRESULT>([mstemp, oldBandwidth, lastmeasure, this]()
      {

        if (mstemp == nullptr || (mstemp != nullptr && mstemp->GetCurrentState() != MSS_STARTED && mstemp->GetCurrentState() != MSS_BUFFERING))
          return S_OK;
        bool Cancel = false;
        LOG("Suggesting bitrate change from " << oldBandwidth << " to " << LastSuggestedBandwidth);
        //notify
        BitrateSwitchSuggested(LastSuggestedBandwidth, (unsigned int) lastmeasure, Cancel);
        //if cancellation was requested
        if (Cancel)
          //go back to the last suggestion
          InterlockedExchange(&LastSuggestedBandwidth, oldBandwidth);

        return S_OK;
      },task_options(cts.get_token(),task_continuation_context::use_arbitrary())),cts);
    }
  }

}
