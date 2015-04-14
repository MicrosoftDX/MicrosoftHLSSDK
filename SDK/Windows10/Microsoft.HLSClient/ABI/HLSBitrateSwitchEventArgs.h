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

#include "Interfaces.h"

using namespace Microsoft::HLSClient;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      [Windows::Foundation::Metadata::Threading(Windows::Foundation::Metadata::ThreadingModel::Both)]
      [Windows::Foundation::Metadata::MarshalingBehavior(Windows::Foundation::Metadata::MarshalingType::Agile)]
      public ref class HLSBitrateSwitchEventArgs sealed : public IHLSBitrateSwitchEventArgs
      {
      private:
        unsigned int _fromBitrate;
        unsigned int _toBitrate;
        unsigned int _measuredBandwidth;
        TrackType _forTrack;
        bool _cancelled;
      internal:
        HLSBitrateSwitchEventArgs(unsigned int from, unsigned int to, unsigned int lastmeasured, TrackType tracktype) :
          _fromBitrate(from), _toBitrate(to), _measuredBandwidth(lastmeasured), _forTrack(tracktype), _cancelled(false)
        {
        }
      public:
       

        property unsigned int LastMeasuredBandwidth
        {
          virtual unsigned int get()
          {
            return _measuredBandwidth;
          }
        }

        property TrackType ForTrackType
        {
          virtual TrackType get()
          {
            return _forTrack;
          }
        }

        property unsigned int FromBitrate
        {
          virtual unsigned int get()
          {
            return _fromBitrate;
          }
        }

        property unsigned int ToBitrate
        {
          virtual unsigned int get()
          {
            return _toBitrate;
          }
        }

        property bool Cancel
        {
          virtual bool get()
          {
            return _cancelled;
          }
          virtual void set(bool val)
          {
            _cancelled = val;
          }
        }
      };
    }
  }
} 