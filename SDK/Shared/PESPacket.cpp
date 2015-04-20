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
#include "PESPacket.h" 
#include "Timestamp.h"
#include "TransportPacket.h"
#include "TransportStreamParser.h"  

using namespace Microsoft::HLSClient::Private;

PESPacket::PESPacket() :StreamID(0), HasElementaryStreamRate(false),//pParentTSP(parent),
ElementaryStreamRate(0), HasHeader(false), upPacketData(nullptr)
{
}

PESPacket::~PESPacket()
{
  PresentationTimestamp.reset();
  DecoderTimestamp.reset();
}
ContentType PESPacket::GetContentType(TransportPacket *pParent,std::map<ContentType, unsigned short> PIDFilter)
{

  if ((this->HasHeader && StreamID >> 5 == 6) || (pParent->pParentParser->PMT.find(pParent->PID) != pParent->pParentParser->PMT.end() && pParent->pParentParser->PMT[pParent->PID] == 0x0F))
    return ContentType::AUDIO;
  else if ((this->HasHeader && StreamID >> 4 == 14) || (pParent->pParentParser->PMT.find(pParent->PID) != pParent->pParentParser->PMT.end() && pParent->pParentParser->PMT[pParent->PID] == 0x1B))
    return ContentType::VIDEO;
  else if ((pParent->pParentParser->PMT.find(pParent->PID) != pParent->pParentParser->PMT.end() && pParent->pParentParser->PMT[pParent->PID] == 0x15) ||
    (this->HasHeader && PIDFilter.find(ContentType::METADATA) != PIDFilter.end() && PIDFilter[ContentType::METADATA] == StreamID)// ||
   // (this->HasHeader && StreamID == 0xBD)//private_stream_1 ||
    )
    return ContentType::METADATA;
  else
    return ContentType::UNKNOWN;

}
const std::shared_ptr<PESPacket> PESPacket::Parse(const BYTE *pesdata,
  unsigned short NumBytes,
  //shared_ptr<TransportPacket>  pParent,
  TransportPacket *pParent,
  std::map<ContentType, unsigned short>& MediaTypePIDMap,
  std::map<ContentType, unsigned short> PIDFilter,
  std::vector<unsigned short>& MetadataStreams,
  std::vector<std::shared_ptr<Timestamp>>& Timeline)
{
  //std::shared_ptr<PESPacket> ret = std::make_shared<PESPacket>(pParent.get());
  std::shared_ptr<PESPacket> ret = std::make_shared<PESPacket>();


  if (pParent->PayloadUnitStartIndicator == 0x01)
  {
    ret->HasHeader = true;
    unsigned long _tmpulong = BitOp::ToInteger<unsigned long>(pesdata, 4);
    unsigned long startcodeprefix = BitOp::ExtractBits(_tmpulong, 0, 24);
    if (startcodeprefix != 0x000001) //wrong start code for PES
      return nullptr;

    ret->StreamID = static_cast<BYTE>(BitOp::ExtractBits(_tmpulong, 24, 8));
    pParent->MediaType = ret->GetContentType(pParent,PIDFilter);
    if (pParent->MediaType == ContentType::UNKNOWN)
      return nullptr; 
    //for audio and video media types
    if (pParent->MediaType == ContentType::AUDIO || pParent->MediaType == ContentType::VIDEO)
    {
      //have we found a PID for this media type already ? 
      auto foundcontenttypeinmap = MediaTypePIDMap.find(pParent->MediaType);
      //is there an entry in the filter for this content type ?
      auto foundcontenttypeinfilter = PIDFilter.find(pParent->MediaType);
      //content type entry in filter and in PID map and this PID is in the filter but we have a different PID  in the map for this content type
      if ((foundcontenttypeinfilter != PIDFilter.end() && foundcontenttypeinmap != MediaTypePIDMap.end() && foundcontenttypeinfilter->second != foundcontenttypeinmap->second && foundcontenttypeinfilter->second == pParent->PID) ||
        //nothing in filter,but we found an existing PID of this type in the PID map that does not match, and this one is less than the PID in the map
        (foundcontenttypeinfilter == PIDFilter.end() && foundcontenttypeinmap != MediaTypePIDMap.end() && foundcontenttypeinmap->second != pParent->PID && pParent->PID < foundcontenttypeinmap->second))
      {
        MediaTypePIDMap[pParent->MediaType] = pParent->PID;
      }
      else if (foundcontenttypeinmap != MediaTypePIDMap.end() && foundcontenttypeinmap->second != pParent->PID)
      {
        return nullptr; //not interested - we already have found a different PID for this media type - we only enable the lowest PID demuxed stream of a specific type or pick one from a supplied filter and ignore others
      }
    }


    if (ret->StreamID != PESStreamIDType::PROGRAM_STREAM_MAP &&
      ret->StreamID != PESStreamIDType::PADDING_STREAM &&
      ret->StreamID != PESStreamIDType::PRIVATE_STREAM_2 &&
      ret->StreamID != PESStreamIDType::ECM_STREAM &&
      ret->StreamID != PESStreamIDType::EMM_STREAM &&
      ret->StreamID != PESStreamIDType::PROGRAM_STREAM_DIRECTORY &&
      ret->StreamID != PESStreamIDType::ANNEXB_OR_DSMCC &&
      ret->StreamID != PESStreamIDType::H222_TYPE_E)
    {

      ret->upPacketData = pesdata + 9 + pesdata[8];
      ret->PacketDataLength = NumBytes - (9 + pesdata[8]);

      if (ret->PacketDataLength > 188)
        return nullptr;

      ret->HasElementaryStreamRate = BitOp::ExtractBits(pesdata[7], 3, 1) == 0x01;
      //first 2 bits is PTS_DTS_Flag
      BYTE PTS_DTS_flag = BitOp::ExtractBits(pesdata[7], 0, 2);
      if (PTS_DTS_flag == 0x02 || PTS_DTS_flag == 0x03)
      {
        ret->PresentationTimestamp = Timestamp::Parse(pesdata + 9, TimestampType::PTS);
        Timeline.push_back(ret->PresentationTimestamp);
      }
      if (PTS_DTS_flag == 0x03)
      {
        ret->DecoderTimestamp = Timestamp::Parse(pesdata + 14, TimestampType::PTS);
      }


      if (ret->HasElementaryStreamRate)
      {
        short StepOver = 8;

        //3rd bit is ESCR flag
        BYTE ESCR_Flag = BitOp::ExtractBits(pesdata[7], 2, 1);
        //4th bit is ES Rate flag
        if (ESCR_Flag == 0x01) StepOver += 8;
        if (PTS_DTS_flag == 0x02)
          StepOver += 5;
        else if (PTS_DTS_flag == 0x03)
          StepOver += 10;

        ret->ElementaryStreamRate = BitOp::ExtractBits(BitOp::ToInteger<unsigned int>(pesdata + StepOver, 3), 9, 22) * 50 * 8;//originally defined in 50bytes/sec units - convert and store as bits per sec

      }
    }
    else if (ret->StreamID == PESStreamIDType::PADDING_STREAM)
    {
      return nullptr;//padding
    }
    else if (ret->StreamID == PESStreamIDType::PROGRAM_STREAM_MAP ||
      ret->StreamID == PESStreamIDType::PRIVATE_STREAM_2 ||
      ret->StreamID == PESStreamIDType::ECM_STREAM ||
      ret->StreamID == PESStreamIDType::EMM_STREAM ||
      ret->StreamID == PESStreamIDType::PROGRAM_STREAM_DIRECTORY ||
      ret->StreamID == PESStreamIDType::ANNEXB_OR_DSMCC ||
      ret->StreamID == PESStreamIDType::H222_TYPE_E)
    {
      ret->upPacketData = pesdata + 6;
      ret->PacketDataLength = NumBytes - 6;
      if (ret->PacketDataLength > 188)
        return nullptr;

    }
   
  }
  else
  {

    ret->upPacketData = pesdata;
    ret->PacketDataLength = NumBytes;

    if (ret->PacketDataLength > 188)
      return nullptr;

  }

  

  ContentType mediaType = ret->GetContentType(pParent,PIDFilter);
  if (mediaType != ContentType::UNKNOWN)
  {

    if (MediaTypePIDMap.find(mediaType) == MediaTypePIDMap.end() && (mediaType == ContentType::AUDIO || mediaType == ContentType::VIDEO))
    {
      MediaTypePIDMap.emplace(std::pair<ContentType, unsigned short>(mediaType, pParent->PID));
    }
    else if (mediaType == ContentType::METADATA && std::find_if(MetadataStreams.begin(), MetadataStreams.end(), [pParent](unsigned short pid) { return pid == pParent->PID; }) == MetadataStreams.end())
    {
      MetadataStreams.push_back(pParent->PID);
      
    }
  }


  return ret;
}
