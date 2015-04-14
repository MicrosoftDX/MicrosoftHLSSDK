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
#include "PMTSection.h" 

using namespace Microsoft::HLSClient::Private;

PMTSection::PMTSection() :CurrentSectionNumber(0), MaxSectionNumber(0), HasPCR(false), PCRPID(0)
{
}

PMTSection::~PMTSection()
{
  MapData.clear();
}
bool PMTSection::IsPMTSection(const BYTE *pmtdata)
{
  return pmtdata[0] == 0x02/*table ID*/
    && BitOp::ExtractBits(pmtdata[5], 7, 1) == 0x01; /*current_next _indicator - only interested in what is applicable now i.e. 1*/
}

bool PMTSection::IsHLSTimedMetadata(const BYTE * es_info_loop, unsigned int length)
{
  if (length == 0)
    return false;

  unsigned int ctr = 0;
  while (true)
  {
    //read 2 bytes until we find 0xFFFF
    if (BitOp::ToInteger<unsigned short>(es_info_loop + ctr, 2) == 0xFFFF)
      break;
    ctr += 2;
    if (length - ctr - 1 <= 2)
      return false;
  }
  ctr += 2;
  //next 4 bytes should be 'ID3 '
  BYTE tagid[5];
  ZeroMemory(tagid, 5);
  memcpy_s(tagid, 4, es_info_loop + ctr, 4);
  if (std::string((char *) tagid) != "ID3 ")
    return false;
  ctr += 4;

  //next 1 byte should be 0xFF
  if (es_info_loop[ctr++] != 0xFF)
    return false;


  //next 4 bytes should be 'ID3 '

  ZeroMemory(tagid, 5);
  memcpy_s(tagid, 4, es_info_loop + ctr, 4);
  if (std::string((char *) tagid) != "ID3 ")
    return false;

  return true;
}
std::shared_ptr<PMTSection> PMTSection::Parse(const BYTE *pmtdata)
{
  std::shared_ptr<PMTSection> ret = std::make_shared<PMTSection>();
  ret->CurrentSectionNumber = pmtdata[6];
  ret->MaxSectionNumber = pmtdata[7];

  unsigned short SectionLength = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(pmtdata + 1, 2), 4, 12);
  unsigned short ProgramInfoLength = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(pmtdata + 10, 2), 4, 12);
  unsigned short MapEntryLength = (SectionLength - 13 - ProgramInfoLength);

  ret->PCRPID = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(pmtdata + 8, 2), 3, 13);
  ret->HasPCR = (ret->PCRPID != 0x1FFF);
  unsigned short offset = 12 + ProgramInfoLength;

  for (unsigned short Length = 0; Length < MapEntryLength;)
  {
    BYTE streamType = pmtdata[offset++];

    unsigned short elemStreamPID = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(pmtdata + offset, 2), 3, 13);
    offset += 2;
    unsigned short ESInfoLength = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(pmtdata + offset, 2), 4, 12);
    offset += 2;

    if (streamType == 0x0F || //ADTS audio
      streamType == 0x1B || //AVC Video
      (streamType == 0x15 && IsHLSTimedMetadata(pmtdata + offset, ESInfoLength))) //HLS Timed (ID3) Metadata in PES packets 
    {
      ret->MapData.emplace(std::move(std::pair<unsigned short, BYTE>(elemStreamPID, streamType)));
    }
    offset += ESInfoLength;
    Length += 5 + ESInfoLength;
  }
  return ret;
}
