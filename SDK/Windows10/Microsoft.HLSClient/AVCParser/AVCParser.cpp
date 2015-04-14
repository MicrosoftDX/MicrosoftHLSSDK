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
#include "AVCParser.h" 
#include "TransportStream\SampleData.h" 

using namespace Microsoft::HLSClient;
using namespace Microsoft::HLSClient::Private;
//we parse for RBSP
void NALUnitBase::Parse()
{

}

//parse SPS
void SequenceParameterSet::Parse()
{
  /*unsigned int readctr = 0;

  const BYTE *data = nullptr;
  unsigned int len = 0;
  if(naluDataEmulPrevBytesRemoved != nullptr)
  {
  data = naluDataEmulPrevBytesRemoved.get();
  }
  else
  {
  while(naluData[readctr++] != 0x01); //skip prefix
  readctr++;//skip NALU header;
  data = naluData;

  }
  readctr = 0;
  BYTE profile_idc = data[readctr];
  //skip profile_idc + constraintsetflags+level_idc
  readctr += 3;
  unsigned int sps_id = 0;
  readctr += AVCParser::ReverseExpGolombOrderZero(data + readctr,sps_id);
  if(profile_idc == 100 ||
  profile_idc == 110 ||
  profile_idc == 122 ||
  profile_idc == 244 ||
  profile_idc == 44 ||
  profile_idc == 83 ||
  profile_idc == 86 ||
  profile_idc == 118 ||
  profile_idc == 128)
  {
  unsigned int chroma_format_idc = 0;
  readctr = AVCParser::ReverseExpGolombOrderZero(data + readctr,chroma_format_idc);

  }*/



}
//parse SEI
void SupplementalInfo::Parse()
{
  unsigned int readctr = 0;

  const BYTE *data = nullptr;
  unsigned int len = 0;
  if (naluDataEmulPrevBytesRemoved != nullptr)
  {
    data = naluDataEmulPrevBytesRemoved.get();
  }
  else
  {
    while (naluData[readctr++] != 0x01); //skip prefix
    readctr++;//skip NALU header;
    data = naluData;

  }


  while (readctr + 3 < length) //we have SEI messages to read (at least 3 bytes - payloadtype, payloadsize plus data)
  {
    auto payloadtype = 0;
    auto payloadsize = 0;

    while (readctr < length && data[readctr] == 0xFF)
    {
      payloadtype += 255;
      readctr++;
    }
    if (readctr >= length) break;

    payloadtype += data[readctr];
    readctr++;

    while (readctr < length && data[readctr] == 0xFF)
    {
      payloadsize += 255;
      readctr++;
    }
    if (readctr >= length) break;

    payloadsize += data[readctr];
    if (readctr + payloadsize >= length) break;

    readctr++;

    switch (payloadtype)
    {
    case SEIType::SEITYPE_ITUT_T35:
      if (payloadsize > 0)
      {
        PayloadType = SEIType::SEITYPE_ITUT_T35;
        spMsgITUT_T35 = make_shared<SEIMessageITUT_T35>(data + readctr, payloadsize);
      }
    default:
      break;
    }

    readctr += payloadsize;
  }
}

void AVCParser::Parse(shared_ptr<SampleData> spsd)
{

  std::vector<BYTE> ElemDataBuff;
  for (auto itr = spsd->elemData.begin(); itr != spsd->elemData.end(); itr++)
  {
    auto oldSize = ElemDataBuff.size();
    if (std::get<1>(*itr) > 0)
    {
      ElemDataBuff.resize(oldSize + std::get<1>(*itr));
      memcpy_s(&(*(ElemDataBuff.begin() + oldSize)), std::get<1>(*itr), std::get<0>(*itr), std::get<1>(*itr));
    }
  }

  const BYTE* elemData = &(*(ElemDataBuff.begin()));


  unsigned int size = (unsigned int) ElemDataBuff.size();
  unsigned int readbytecount = 0;
  bool EOS = false;

  //skip leading zero bytes and position on first prefix
  while (elemData[readbytecount] == 0x00 && BitOp::ToInteger<unsigned int>(elemData + readbytecount, 3) != 0x000001)
    ++readbytecount;

  if (readbytecount + 3 >= size) //nothing to parse
    return;

  while (true)
  {

    unsigned int NumBytesInNALUnit = 0;
    unsigned int BitSequenceMatchPos = 0;
    //find the next prefix match - after this prefix
    if (FindNextMatchingBitSequence(0x000001, 3, readbytecount + 3, elemData, size, BitSequenceMatchPos) == true)
      //if found - find the number of bytes in the NALU
      NumBytesInNALUnit = BitSequenceMatchPos - readbytecount;
    else
      NumBytesInNALUnit = size - readbytecount;
    //process NALU here

    //read a byte (after the prefix) and skip over first 3 bits(forbidden bit(1 bit) + nal_ref_idc(2 bits) to get nal_unit_type
    auto nal_unit_type = BitOp::ExtractBits(elemData[readbytecount + 3], 3, 5);
    shared_ptr<NALUnitBase> spnalu = nullptr;
    switch (nal_unit_type)
    {
    case NALUType::NALUTYPE_CODEDSLICEIDR:
      spnalu = std::make_shared<CodedSliceIDR>(elemData + readbytecount, NumBytesInNALUnit);
      spsd->IsSampleIDR = true;
      break;
    case NALUType::NALUTYPE_SEI:
      spnalu = std::make_shared<SupplementalInfo>(elemData + readbytecount, NumBytesInNALUnit);
      spnalu->Parse();
      if (dynamic_cast<SupplementalInfo*>(spnalu.get())->PayloadType == SEIType::SEITYPE_ITUT_T35 && dynamic_cast<SupplementalInfo*>(spnalu.get())->spMsgITUT_T35 != nullptr)
      {
        spsd->spInBandCC = make_shared<tuple<shared_ptr<BYTE>, unsigned int>>(
          shared_ptr<BYTE>(dynamic_cast<SupplementalInfo*>(spnalu.get())->spMsgITUT_T35->Payload.release(), [](BYTE *data) { delete[] data; }),//provide deleter for shared_ptr
          dynamic_cast<SupplementalInfo*>(spnalu.get())->spMsgITUT_T35->PayloadSize);
      }
      break;
    case NALUType::NALUTYPE_SPS:
      spnalu = std::make_shared<SequenceParameterSet>(elemData + readbytecount, NumBytesInNALUnit);
      break;
    case NALUType::NALUTYPE_PPS:
      spnalu = std::make_shared<PictureParameterSet>(elemData + readbytecount, NumBytesInNALUnit);
      break;
    case NALUType::NALUTYPE_AUD:
      spnalu = std::make_shared<AccessUnitDelimiter>(elemData + readbytecount, NumBytesInNALUnit);
      break;
    default:
      spnalu = std::make_shared<NALUnitBase>(elemData + readbytecount, NumBytesInNALUnit);
      break;
    }


    // vecnalu.push_back(spnalu);

    //done processing
    readbytecount += NumBytesInNALUnit;

    if (BitSequenceMatchPos == size || size - readbytecount <= 3) //we did not find the start of another NALU last time we checked - or nothing else left to parse
      break;
  }
  return;

}

