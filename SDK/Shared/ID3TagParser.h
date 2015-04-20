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

#include "pch.h"
#include <vector>
#include <memory>
#include <wtypes.h>
#include "BitOp.h"

using namespace std;

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      class ID3TagParser
      {
      private:
        ULONG readctr;

        static  bool IsID3Tag(BYTE* data, ULONG size, ULONG& readctr, BYTE& flags, unsigned int& tagsize, unsigned short& majorversion)
        {

          //read 3 bytes at a time and carry on until you find "ID3"
          for (; readctr < size; readctr++)
          {
            BYTE tagid[4];
            ZeroMemory(tagid, 4);
            memcpy_s(tagid, 3, data + readctr, 3);
            if (string((char *) tagid) == "ID3")
            {
              readctr += 3;
              break;
            }
          }

          if (readctr + 2 >= size) //2 bytes left to read for version ? 
            return false;

          //read off two bytes for version
          majorversion = data[readctr];
          readctr += 2;
          if (readctr + 1 >= size) //1 byte left to read for flags ? 
            return false;
          //read off one byte for flags
          flags = data[readctr++];
          if (readctr + 4 >= size) //4 bytes left to read for size ? 
            return false;


          ////we read the next 4 bytes - they are "synchsafe" - take 7 rightmost bits from each byte to form a 32 bit numeric (with only 28 bits)
          tagsize = data[readctr + 3];
          tagsize |= (data[readctr + 2] << 7);
          tagsize |= (data[readctr + 1] << 14);
          tagsize |= (data[readctr] << 21);

          readctr += 4;


          //for(int i = 0;i < 4; i++,readctr++)
          //{
          //  auto val = BitOp::ExtractBits<BYTE>(data[readctr],1,7);
          //  val =  val << 1;//introduce extra zero at the tail - but rid the MSB zero bit
          //  memcpy_s((void*)(&tagsize),4,&val,1);
          //  tagsize = tagsize >> 1;//rid the exra zero now
          //  if(i != 3) 
          //    tagsize = tagsize << 8; //create room for next copy
          //}

          return (readctr >= size) ? false : true;
        }

        static void StripExtendedHeader(BYTE* data, ULONG size, ULONG& readctr)
        {
          //read extened header size 
          auto exthdrsize = BitOp::ToInteger<unsigned int>(data, 4);
          readctr += exthdrsize;
        }

        static bool ExtractFrame(BYTE* data, ULONG size, ULONG& readctr, string& FrameID, unsigned int& FrameSize, BYTE& FrameFlags, std::vector<BYTE>& framedata, unsigned short majorversion)
        {

          //frame header begins with frame id string = 4 bytes
          BYTE tagid[5];
          ZeroMemory(tagid, 5);
          memcpy_s(tagid, 4, data + readctr, 4);
          FrameID = (char*) tagid;
          readctr += 4;
          ////get the size
          //if (majorversion <= 3) //for ID3v2.3 and lower -frame size is not synchsafe - for higher versions it is
          //{
          //  FrameSize = data[readctr + 3];
          //  FrameSize |= (data[readctr + 2] << 8);
          //  FrameSize |= (data[readctr + 1] << 16);
          //  FrameSize |= (data[readctr] << 24);
          //}
          //else       ////we read the next 4 bytes - they are "synchsafe" - take 7 rightmost bits from each byte to form a 32 bit numeric (with only 28 bits)
          //{
            FrameSize = data[readctr + 3];
            FrameSize |= (data[readctr + 2] << 7);
            FrameSize |= (data[readctr + 1] << 14);
            FrameSize |= (data[readctr] << 21);
          //}

          readctr += 4;
          //skip the flags
          readctr += 2;

          //look for a null character

          framedata.resize(FrameSize);
          memcpy_s(&(*(framedata.begin())), FrameSize, data + readctr, FrameSize);
          readctr += FrameSize;
          return true;
        }

      public:

        static bool AdjustPayloadToID3Tag(BYTE* data, ULONG size, ULONG& startat, ULONG& correctpayloadsize)
        {
          ULONG readctr = 0;
          //read 3 bytes at a time and carry on until you find "ID3"
          for (; readctr < size; readctr++, startat++)
          {
            BYTE tagid[4];
            ZeroMemory(tagid, 4);
            memcpy_s(tagid, 3, data + readctr, 3);
            if (string((char *) tagid) == "ID3")
            {
              readctr += 3;
              break;
            }
          }


          if (readctr + 2 >= size) //2 bytes left to read for version ? 
            return false;

          //read off two bytes for version
          auto majorversion = data[readctr];
          readctr += 2;
          if (readctr + 1 >= size) //1 byte left to read for flags ? 
            return false;
          //read off one byte for flags
          auto flags = data[readctr++];
          if (readctr + 4 >= size) //4 bytes left to read for size ? 
            return false;


          bool hasfooter = false;
          if (majorversion >= 0x04 && (flags & 0x20) == 0x20)
            hasfooter = true;



          ////we read the next 4 bytes - they are "synchsafe" - take 7 rightmost bits from each byte to form a 32 bit numeric (with only 28 bits)
          correctpayloadsize = data[readctr + 3];
          correctpayloadsize |= (data[readctr + 2] << 7);
          correctpayloadsize |= (data[readctr + 1] << 14);
          correctpayloadsize |= (data[readctr] << 21);

          readctr += 4;

          correctpayloadsize += ((hasfooter) ? 20 : 10);

          //for(int i = 0;i < 4; i++,readctr++)
          //{
          //  auto val = BitOp::ExtractBits<BYTE>(data[readctr],1,7);
          //  val =  val << 1;//introduce extra zero at the tail - but rid the MSB zero bit
          //  memcpy_s((void*)(&tagsize),4,&val,1);
          //  tagsize = tagsize >> 1;//rid the exra zero now
          //  if(i != 3) 
          //    tagsize = tagsize << 8; //create room for next copy
          //}

          return (readctr >= size) ? false : true;
        }
        static vector<shared_ptr<tuple<string, std::vector<BYTE>>>> Parse(BYTE* data, ULONG size)
        {
          ULONG readctr = 0;
          BYTE hdrflags = 0;
          unsigned int tagsize = 0;
          unsigned short majorversion = 0;
          vector<shared_ptr<tuple<string, std::vector<BYTE>>>> ret;
          if (!IsID3Tag(data, size, readctr, hdrflags, tagsize, majorversion))
            return ret;

          if ((hdrflags & 0x40) == 0x40) //we have extended header
            StripExtendedHeader(data, size, readctr);

          while (readctr < tagsize - 1)
          {
            unsigned int framesize = 0;
            BYTE frameflags = 0;
            string frameid;
            std::vector<BYTE> framedata;
            auto gotframe = ExtractFrame(data, size, readctr, frameid, framesize, frameflags, framedata, majorversion);
            if (gotframe)
              ret.push_back(make_shared<tuple<string, vector<BYTE>>>(frameid, framedata));

          }



          return ret;

        }
      };
    }
  }
} 