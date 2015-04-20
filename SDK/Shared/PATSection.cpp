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
#include "PATSection.h" 

using namespace Microsoft::HLSClient::Private;

PATSection::PATSection() :CurrentSectionNumber(0), MaxSectionNumber(0)
{
}

PATSection::~PATSection()
{
  AssociationData.clear();
}
bool PATSection::IsPATSection(const BYTE *patdata)
{
  return patdata[0] == 0x00/*table ID*/
    && BitOp::ExtractBits<BYTE>(patdata[5], 7, 1) == 0x01; /*current_next _indicator - only interested in what is applicable now i.e. 1*/
}
std::shared_ptr<PATSection> PATSection::Parse(const BYTE *patdata)
{
  std::shared_ptr<PATSection> ret = std::make_shared<PATSection>();
  ret->CurrentSectionNumber = patdata[6];
  ret->MaxSectionNumber = patdata[7];
  unsigned short SectionLength = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(patdata + 1, 2), 4, 12);
  unsigned short MapEntryCount = (SectionLength - 9) / 4; //4 bytes per entry 

  for (unsigned short Counter = 0; Counter < MapEntryCount; Counter++)
  {
    unsigned short prognum = BitOp::ToInteger<unsigned short>(patdata + 8 + (Counter * 4), 2);
    if (prognum != 0)
    {
      unsigned short ProgMapPID = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(patdata + 10 + (Counter * 4), 2), 3, 13);
      ret->AssociationData.emplace(std::move(std::pair<unsigned short, unsigned short>(ProgMapPID, prognum)));
    }
  }
  return ret;
}
