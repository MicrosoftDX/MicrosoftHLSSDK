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
#include "MP3HeaderParser.h"
#include "BitOp.h"

using namespace std;
using namespace Microsoft::HLSClient::Private;

bool MP3HeaderParser::Parse(BYTE* data,unsigned int len, MPEGLAYER3WAVEFORMAT* wfInfo)
{
  BYTE* stream = nullptr;
  unsigned int bitrate = 0;
  unsigned int samplingrate = 0;
  unsigned short channelcount = 0;

  //look for frame sync

  //read 2 bytes at a time - then extract 11 bits and make sure they are all 1

  for (unsigned int i = 0; i < len; i++)
  {
    unsigned short twobytes = BitOp::ToInteger<unsigned short>(data + i, sizeof(unsigned short));
    if (twobytes >> 5 == 0x7FF)//11111111111
    {
      stream = data + i;
      break;
    }
  }
  if (stream == nullptr)
    return false;

  //version - 2nd byte - 4th and 5th bits
  BYTE version = BitOp::ExtractBits(stream[1], 3, 2);
  if (version != 0 && version != 2 && version != 3)
    return false;
  //Layer Desc = 6th and 7th bit in second byte 
  BYTE layerDesc = BitOp::ExtractBits(stream[1], 5,2);
  if (layerDesc != 1) //not Layer III
    return false;
  //Bitrate - 3rd byte - 1st 4 bits
  BYTE bitrateIdx = BitOp::ExtractBits(stream[2], 0, 4);
  
  if (version == 3)//MPEG v1
  {
    switch (bitrateIdx)
    { 
    case 1:
      bitrate = 32 ;
      break;
    case 2:
      bitrate = 40 ;
      break;
    case 3:
      bitrate = 48 ;
      break;
    case 4:
      bitrate = 56 ;
      break;
    case 5:
      bitrate = 64 ;
      break;
    case 6:
      bitrate = 80 ;
      break;
    case 7:
      bitrate = 96 ;
      break;
    case 8:
      bitrate = 112 ;
      break;
    case 9:
      bitrate = 128 ;
      break;
    case 10:
      bitrate = 160 ;
      break;
    case 11:
      bitrate = 192 ;
      break;
    case 12:
      bitrate = 224 ;
      break;
    case 13:
      bitrate = 256 ;
      break;
    case 14:
      bitrate = 320 ;
      break;
    default:
      return false;

    }
  }
  else if (version == 2 || version == 0)
  {
    switch (bitrateIdx)
    {
    case 1:
      bitrate = 8 ;
      break;
    case 2:
      bitrate = 16 ;
      break;
    case 3:
      bitrate = 24 ;
      break;
    case 4:
      bitrate = 32 ;
      break;
    case 5:
      bitrate = 40 ;
      break;
    case 6:
      bitrate = 48 ;
      break;
    case 7:
      bitrate = 56 ;
      break;
    case 8:
      bitrate = 64 ;
      break;
    case 9:
      bitrate = 80 ;
      break;
    case 10:
      bitrate = 96 ;
      break;
    case 11:
      bitrate = 112 ;
      break;
    case 12:
      bitrate = 128 ;
      break;
    case 13:
      bitrate = 144 ;
      break;
    case 14:
      bitrate = 160 ;
      break;
    default:
      return false;
    }
  }
  else
    return false;

  //Sampling Rate - 3rd byte - 5th and 6th bits
  BYTE samplingidx = BitOp::ExtractBits(stream[2], 4, 2);

  switch (samplingidx)
  {
  case 0:
    if (version == 0)
      samplingrate = 11025;
    else if (version == 2)
      samplingrate = 22050;
    else if (version == 3)
      samplingrate = 44100;
    break;
  case 1:
    if (version == 0)
      samplingrate = 12000;
    else if (version == 2)
      samplingrate = 24000;
    else if (version == 3)
      samplingrate = 48000;
    break;
  case 2:
    if (version == 0)
      samplingrate = 8000;
    else if (version == 2)
      samplingrate = 16000;
    else if (version == 3)
      samplingrate = 32000;
    break; 
  default:
    return false;
  }

  //channel mode - 4th byte - 1st 2 bits
  BYTE channelmode = BitOp::ExtractBits(stream[3], 0, 2);
  if (channelmode == 0 || channelmode == 1 || channelmode == 2)
    channelcount = 2;
  else
    channelcount = 1;

  BYTE paddingBit = BitOp::ExtractBits(stream[2], 6, 1);
   
  wfInfo->wID = MPEGLAYER3_ID_MPEG;
  wfInfo->wfx.wFormatTag = WAVE_FORMAT_MPEGLAYER3;
  wfInfo->wfx.wBitsPerSample = 0;
  wfInfo->wfx.nChannels = channelcount;
  wfInfo->wfx.nSamplesPerSec = samplingrate;
  wfInfo->wfx.nAvgBytesPerSec = bitrate * 1024 / 8;
  wfInfo->wfx.nBlockAlign = 1;
  wfInfo->wfx.cbSize = MPEGLAYER3_WFX_EXTRA_BYTES;
  wfInfo->fdwFlags = MPEGLAYER3_FLAG_PADDING_OFF; 
  wfInfo->nFramesPerBlock = 1;
  wfInfo->nBlockSize = ((144 * bitrate * 1024) / samplingrate) * (wfInfo->nFramesPerBlock);
  wfInfo->nCodecDelay = 0;

  return true;
}