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
#include "AdaptationField.h" 
#include "Timestamp.h"
#include "TransportPacket.h"

using namespace Microsoft::HLSClient::Private;

AdaptationField::AdaptationField(TransportPacket *parent) :AdaptationFieldLength(0), PCR(nullptr), OPCR(nullptr), pParent(parent), DiscontinuityIndicator(0)
{
}

AdaptationField::~AdaptationField()
{
  PCR.reset();
  OPCR.reset();
}


std::shared_ptr<AdaptationField> AdaptationField::Parse(const BYTE* data, TransportPacket *pParent)
{
  std::shared_ptr<AdaptationField> ret = std::make_shared<AdaptationField>(pParent);
  ret->AdaptationFieldLength = data[0];
  if (ret->AdaptationFieldLength == 0)
    return ret;

  ret->DiscontinuityIndicator = BitOp::ExtractBits(data[1], 0, 1);


  bool HasProgramClockReference = (BitOp::ExtractBits(data[1], 3, 1) == 0x01);
  bool HasOriginalProgramClockReference = (BitOp::ExtractBits(data[1], 4, 1) == 0x01);


  if (HasProgramClockReference)
    ret->PCR = Timestamp::Parse(data + 2, TimestampType::PCR);

  if (HasOriginalProgramClockReference)
  {
    ret->OPCR = Timestamp::Parse(data + 8, TimestampType::OPCR);
  }

  return ret;
}
