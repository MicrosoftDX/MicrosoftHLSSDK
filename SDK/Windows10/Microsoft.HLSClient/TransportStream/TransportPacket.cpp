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

#include "TransportPacket.h" 

using namespace Microsoft::HLSClient::Private;

TransportPacket::TransportPacket(TransportStreamParser *parent) :HasAdaptationField(false), spPESPacket(nullptr), IsPATSection(false), HasPayload(false),
PayloadUnitStartIndicator(0), PID(0), ContinuityCounter(0), PayloadLength(0), pParentParser(parent), IsPMTSection(false), MediaType(ContentType::UNKNOWN)
{
}

TransportPacket::~TransportPacket()
{
  spAdaptationField.reset();
  spPESPacket.reset();
  spPATSection.reset();
  spPMTSection.reset();
}

bool TransportPacket::IsTransportPacket(const BYTE* tsPacket)
{
  return (tsPacket[0] == SYNC_BYTE_VALUE);
}

const shared_ptr<TransportPacket> TransportPacket::Parse(const BYTE* tsPacket,
  TransportStreamParser *parent,
  std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
  std::map<ContentType, unsigned short>& MediaTypePIDMap,
  std::map<ContentType, unsigned short> PIDFilter,
  std::vector<unsigned short>& MetadataStreams,
  std::vector<std::shared_ptr<Timestamp>>& Timeline, 
  std::vector<shared_ptr<SampleData>>& CCSamples ,bool ParsingOutOfOrder)
{
  shared_ptr<TransportPacket> ret = std::make_shared<TransportPacket>(parent);

  //get PayloadUnitStartIndicator     
  ret->PayloadUnitStartIndicator = BitOp::ExtractBits(tsPacket[1], 1, 1);
  //get PID
  ret->PID = BitOp::ExtractBits(BitOp::ToInteger<unsigned short>(tsPacket + 1, 2), 3, 13);
  BYTE AdaptationFieldControl = BitOp::ExtractBits(tsPacket[3], 2, 2);
  ret->HasAdaptationField = (AdaptationFieldControl == 0x02 || AdaptationFieldControl == 0x03);
  ret->HasPayload = (AdaptationFieldControl == 0x01 || AdaptationFieldControl == 0x03);

  if (ret->HasAdaptationField)
    ret->spAdaptationField = AdaptationField::Parse(tsPacket + 4, ret.get());

  //get ContinuityCounter
  ret->ContinuityCounter = BitOp::ExtractBits(tsPacket[3], 4, 4);

  if (ret->HasPayload)
  {
    ret->PayloadLength = ret->HasAdaptationField ? 184 - (ret->spAdaptationField->AdaptationFieldLength + 1/*1 byte for the adaptation_field_length field itself*/) : 184;
    unsigned short PayloadStart = ret->HasAdaptationField ? 4 + (ret->spAdaptationField->AdaptationFieldLength + 1/*1 byte for the adaptation_field_length field itself*/) : 4;
    if (ret->PID == 0x0000) //PAT
    {

      if (ret->PayloadUnitStartIndicator == 1) //start of a PAT section - there should be a pointer field (states how many bytes after the pointer field does the PSI table start)
      {
        BYTE pointerfield = tsPacket[PayloadStart]; // pointer field
        //PAT starts at payload start + value of pointer field
        if ((ret->IsPATSection = PATSection::IsPATSection(tsPacket + PayloadStart + 1 + pointerfield)) == true)
          ret->spPATSection = PATSection::Parse(tsPacket + PayloadStart + 1 + pointerfield);
      }
      else //no pointer field
      {
        if ((ret->IsPATSection = PATSection::IsPATSection(tsPacket + PayloadStart)) == true)
          ret->spPATSection = PATSection::Parse(tsPacket + PayloadStart);
      }
    }
    else if (parent->PAT.find(ret->PID) != parent->PAT.end()) //found PID in the PAT stored on the parent stream - PMT packet
    {
      if (ret->PayloadUnitStartIndicator == 1) //start of a PMT section - there should be a pointer field
      {
        BYTE pointerfield = tsPacket[PayloadStart]; // pointer field
        //PMT starts at payload start + value of pointer field
        if ((ret->IsPMTSection = PMTSection::IsPMTSection(tsPacket + PayloadStart + 1 + pointerfield)) == true)
          ret->spPMTSection = PMTSection::Parse(tsPacket + PayloadStart + 1 + pointerfield);
      }
      else //no pointer field
      {
        if ((ret->IsPMTSection = PMTSection::IsPMTSection(tsPacket + PayloadStart)) == true)
          ret->spPMTSection = PMTSection::Parse(tsPacket + PayloadStart);
      }
    }
    else if (ret->PID <= 0x1FFE && ret->PID >= 0x0010) //PES Packet
    {

      if (parent->PMT.empty() && !ParsingOutOfOrder) //we encountered a non PAT/PMT TSP before encountering PAT/PMT
      {
        parent->OutOfOrderTSP.push_back(tsPacket);
      }
      else
      {
        ret->spPESPacket = PESPacket::Parse(tsPacket + PayloadStart, ret->PayloadLength, ret.get(), MediaTypePIDMap, PIDFilter, MetadataStreams, Timeline);


        if (ret->spPESPacket != nullptr) //nullptr = possibly because this is a stream we are not interested in because we found one earlier of the same media type but different PID
        {

          if (ret->PayloadUnitStartIndicator == 1)
          {
            if (ret->pParentParser->SampleBuilder[ret->PID].empty() == false)
            {
              //build and store sample
              ret->pParentParser->BuildSample(ret->PID, MediaTypePIDMap, MetadataStreams, UnreadQueues,CCSamples);
              //clear sample builder
              ret->pParentParser->SampleBuilder[ret->PID].clear();
            }
          }
          //we cannot handle a sample builder state where the first sample does not begin with payload unit indicator == 1

          ret->pParentParser->SampleBuilder[ret->PID].push_back(ret);
        }
      }

    }
    else
      return nullptr; //not of interest to us;
  }


  return ret;
}
