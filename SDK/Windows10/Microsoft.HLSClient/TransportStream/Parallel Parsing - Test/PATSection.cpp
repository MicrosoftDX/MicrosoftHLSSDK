
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
