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

#define SYNC_BYTE_VALUE 0x47
#define SAMPLE_BUFFER_READY_COUNT 5;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {


      enum PESStreamIDType
      {
        PROGRAM_STREAM_MAP = 0xBC,
        PRIVATE_STREAM_1 = 0xBD,
        PADDING_STREAM = 0xBE,
        PRIVATE_STREAM_2 = 0xBF,
        ECM_STREAM = 0xF0,
        EMM_STREAM = 0xF1,
        ANNEXB_OR_DSMCC = 0xF2,
        PROGRAM_STREAM_DIRECTORY = 0xFF,
        ISO_13522_STREAM = 0xF3,
        H222_TYPE_A = 0xF4,
        H222_TYPE_B = 0xF5,
        H222_TYPE_C = 0xF6,
        H222_TYPE_D = 0xF7,
        H222_TYPE_E = 0xF8,
        ANCILLARY_STREAM = 0xF9
      };

      enum ContentType
      {
        AUDIO, VIDEO, METADATA, UNKNOWN
      };

      enum TimestampType
      {
        NONE, PCR, OPCR, PTS, DTS
      };
    }
  }
} 