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
#include "TransportStreamParser.h"   

using namespace Microsoft::HLSClient::Private;

TransportStreamParser::TransportStreamParser() : HasPCR(false), PCRPID(0)
{
  //::InitializeCriticalSectionEx(&csSample,0,0);
}


bool TransportStreamParser::IsTransportStream(const BYTE *tsd) { 
  return TransportPacket::IsTransportPacket(tsd); 
}

HRESULT TransportStreamParser::BuildSample(unsigned short PID,
  std::map<ContentType, unsigned short>& MediaTypePIDMap,
  std::vector<unsigned short>& MetadataStreams,
  std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
  std::vector<shared_ptr<SampleData>>& CCSamples)
{

  HRESULT hr = S_OK;

  auto sd = std::make_shared<SampleData>();
  //sequence has to start with PaylodUnitStart = 1 - drop any bad frames that do not match
  auto startfrom = SampleBuilder[PID][0]->PayloadUnitStartIndicator == 1 ? SampleBuilder[PID].begin() : std::find_if(SampleBuilder[PID].begin(), SampleBuilder[PID].end(), [](shared_ptr<TransportPacket> tsp) { return tsp->PayloadUnitStartIndicator == 1; });
  if (startfrom == SampleBuilder[PID].end())
  {
    LOG("Dropping ALL " << SampleBuilder[PID].size() << " frames of PID " << PID);
    return S_OK;
  }
  else if (startfrom != SampleBuilder[PID].begin())
  {
    LOG("Dropping " << startfrom - SampleBuilder[PID].begin() << " frames of PID " << PID);
  }
  sd->SamplePTS = (*startfrom)->spPESPacket->PresentationTimestamp;

  for (auto itr = startfrom; itr != SampleBuilder[PID].end(); itr++)
  {
    sd->elemData.push_back(tuple<const BYTE*, unsigned int>((*itr)->spPESPacket->upPacketData, (*itr)->spPESPacket->PacketDataLength));
    sd->TotalLen += (*itr)->spPESPacket->PacketDataLength;
  }

  if (MediaTypePIDMap.find(VIDEO) != MediaTypePIDMap.end() && MediaTypePIDMap[VIDEO] == PID)
  {
    avcparser.Parse(sd);
    if (sd->spInBandCC != nullptr)
      CCSamples.push_back(sd);
  }
  UnreadQueues[PID].push_back(sd);
  sd->Index = (unsigned int)UnreadQueues[PID].size() - 1;
  return S_OK;
}
void TransportStreamParser::Parse(const BYTE *tsd, ULONG size,
  std::map<ContentType, unsigned short>& MediaTypePIDMap,
  std::map<ContentType, unsigned short> PIDFilter,
  std::vector<unsigned short>& MetadataStreams,
  std::map<unsigned short, std::deque<std::shared_ptr<SampleData>>>& UnreadQueues,
  std::vector<std::shared_ptr<Timestamp>>& Timeline,
  std::vector<shared_ptr<SampleData>>& CCSamples)
{
  const BYTE * tsdata = tsd;
  this->Clear();

  for (ULONG i = 0; i < size; i += 188)
  {
    if (TransportPacket::IsTransportPacket(tsdata + i))
    {

      auto tsp = TransportPacket::Parse(tsdata + i, this, UnreadQueues, MediaTypePIDMap, PIDFilter, MetadataStreams, Timeline, CCSamples);
      if (tsp == nullptr)
        continue;
      if (tsp->IsPATSection) //PAT
      {
        //store the PAT on the stream - the tsp variable will auto destruct at the end of scope
        for_each(tsp->spPATSection->AssociationData.begin(), tsp->spPATSection->AssociationData.end(), [this](std::pair<unsigned short, unsigned short> p)
        {
          if (PAT.find(p.first) == PAT.end())
            PAT.emplace(p);
        });
      }
      else if (tsp->IsPMTSection) //PMT
      {
        //store the PMT on the stream - the tsp variable will auto destruct at the end of scope
        for_each(tsp->spPMTSection->MapData.begin(), tsp->spPMTSection->MapData.end(), [this](std::pair<unsigned short, BYTE> p)
        {
          if (PMT.find(p.first) == PMT.end())
          {
            PMT.emplace(p);
            //SampleBuilder.emplace(std::pair<unsigned short,std::vector<std::shared_ptr<TransportPacket>>>(p.first,std::vector<std::shared_ptr<TransportPacket>>()));
          }
        });
        this->HasPCR = tsp->spPMTSection->HasPCR;
        this->PCRPID = tsp->spPMTSection->PCRPID;

        //found PMT - do we have any straggler TSP's that we encountered before ? 
        if (OutOfOrderTSP.empty() == false)
        {
          for (auto itr = OutOfOrderTSP.begin(); itr != OutOfOrderTSP.end(); itr++)
          {
            TransportPacket::Parse(*itr, this, UnreadQueues, MediaTypePIDMap, PIDFilter, MetadataStreams, Timeline,CCSamples, true);
          }
        }
        OutOfOrderTSP.clear();
      }
    }
  }

  //did not find PMT - process all remaining TSP's
  if (OutOfOrderTSP.empty() == false)
  {
    for (auto itr = OutOfOrderTSP.begin(); itr != OutOfOrderTSP.end(); itr++)
    {
      TransportPacket::Parse(*itr, this, UnreadQueues, MediaTypePIDMap, PIDFilter, MetadataStreams, Timeline,CCSamples, true);
    }
  }
  for (auto itr = MediaTypePIDMap.begin(); itr != MediaTypePIDMap.end(); itr++)
  {
    if (SampleBuilder[itr->second].empty() == false)
    {
      //build and store sample
      BuildSample(itr->second, MediaTypePIDMap, MetadataStreams, UnreadQueues,CCSamples);
      //clear sample builder
      SampleBuilder[itr->second].clear();
      //sort the sample queue 
    }
  }
  for (auto itr = MetadataStreams.begin(); itr != MetadataStreams.end(); itr++)
  {
    if (SampleBuilder[*itr].empty() == false)
    {
      //build and store sample
      BuildSample(*itr, MediaTypePIDMap, MetadataStreams, UnreadQueues,CCSamples);
      //clear sample builder
      SampleBuilder[*itr].clear();
      //sort the sample queue 
    }
  }
}
