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
#include <memory>
#include <vector>
#include <wtypes.h>
#include "BitOp.h"


using namespace std;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {


      class SampleData;


      enum NALUType : short
      {
        NALUTYPE_NOTRELEVANT = 0, NALUTYPE_CODEDSLICEIDR = 5, NALUTYPE_SPS = 7, NALUTYPE_PPS = 8, NALUTYPE_SEI = 6, NALUTYPE_AUD = 9
      };

      enum SEIType : short
      {
        SEITYPE_ITUT_T35 = 4, SEITYPE_NOTRELEVANT = 0
      };

      class NALUnitBase
      {
      public:
        const BYTE *naluData;
        unique_ptr<BYTE[]> naluDataEmulPrevBytesRemoved;
        unsigned int length;
        NALUType nalutype;
        NALUnitBase(NALUType type, const BYTE *data, unsigned int len) :
          nalutype(type), naluData(data), length(len), naluDataEmulPrevBytesRemoved(nullptr)
        {
        }

        NALUnitBase(const BYTE *data, unsigned int len) :
          nalutype(NALUType::NALUTYPE_NOTRELEVANT), naluData(data), length(len), naluDataEmulPrevBytesRemoved(nullptr)
        {
        }
        virtual void Parse();

        void CleanRBSPOfEmulPreventionBytes()
        {
          std::vector<BYTE> vecnaluDataEmulByteRemoved;
          unsigned int readctr = 0;
          while (naluData[readctr++] != 0x01); //skip prefix
          readctr++;//skip NALU header;
          bool foundEmulPreventByte = false;
          while (readctr < length)
          {
            if (readctr + 2 < length && BitOp::ToInteger<unsigned int>(naluData + readctr, 3) == 0x000003)//emulation prevention
            {
              vecnaluDataEmulByteRemoved.push_back(naluData[readctr++]);
              vecnaluDataEmulByteRemoved.push_back(naluData[readctr++]);
              readctr++;//emulation prevention byte skipped;
              length--;
              foundEmulPreventByte = true;
            }
            else
              vecnaluDataEmulByteRemoved.push_back(naluData[readctr++]);
          }
          if (!foundEmulPreventByte)
            vecnaluDataEmulByteRemoved.clear();
          else
          {
            naluDataEmulPrevBytesRemoved.reset(new BYTE[vecnaluDataEmulByteRemoved.size()]);
            memcpy_s(naluDataEmulPrevBytesRemoved.get(), vecnaluDataEmulByteRemoved.size(), &(*(vecnaluDataEmulByteRemoved.begin())), vecnaluDataEmulByteRemoved.size());
          }
        }
      };


      class SequenceParameterSet : public NALUnitBase
      {
      private:
        unsigned int HRes;
        unsigned int VRes;
      public:
        SequenceParameterSet(const BYTE *data, unsigned int len) : HRes(0), VRes(0), NALUnitBase(NALUType::NALUTYPE_SPS, data, len)
        {
          CleanRBSPOfEmulPreventionBytes();
        }
        void Parse() override;

      };

      class PictureParameterSet : public NALUnitBase
      {
      public:
        PictureParameterSet(const BYTE *data, unsigned int len) : NALUnitBase(NALUType::NALUTYPE_PPS, data, len)
        {
        }

      };

      class AccessUnitDelimiter : public NALUnitBase
      {
      public:
        AccessUnitDelimiter(const BYTE *data, unsigned int len) : NALUnitBase(NALUType::NALUTYPE_AUD, data, len)
        {
        }
      };

      class CodedSliceIDR : public NALUnitBase
      {
      public:
        CodedSliceIDR(const BYTE *data, unsigned int len) : NALUnitBase(NALUType::NALUTYPE_CODEDSLICEIDR, data, len)
        {

        }
      };

      class SEIMessageITUT_T35
      {
      public:
        BYTE CountryCode, CountryCodeExtension;
        unique_ptr<BYTE[]> Payload;
        unsigned int PayloadSize;
        SEIMessageITUT_T35(const BYTE *data, unsigned int len) : CountryCode(0), CountryCodeExtension(0)
        {
          if (data[0] == 0xFF)
          {
            CountryCode = data[0];
            CountryCodeExtension = data[1];
            Payload.reset(new BYTE[len - 2]);
            memcpy_s(Payload.get(), len - 2, data + 2, len - 2);
            PayloadSize = len - 2;
          }
          else
          {
            CountryCode = data[0];
            Payload.reset(new BYTE[len - 1]);
            memcpy_s(Payload.get(), len - 1, data + 1, len - 1);
            PayloadSize = len - 1;
          }
        }
      };

      class SupplementalInfo : public NALUnitBase
      {
      public:
        SEIType PayloadType;
        shared_ptr<SEIMessageITUT_T35> spMsgITUT_T35;
        SupplementalInfo(const BYTE *data, unsigned int len) : NALUnitBase(NALUType::NALUTYPE_SEI, data, len), PayloadType(SEIType::SEITYPE_NOTRELEVANT)
        {
          CleanRBSPOfEmulPreventionBytes();
        }

        void Parse() override;
      };




      class AVCParser
      {
      public:
        //std::vector<shared_ptr<NALUnitBase>> vecnalu;
        void Parse(shared_ptr<SampleData> spsd);

        static unsigned int ReverseExpGolombOrderZero(const BYTE *code, unsigned int& codenum)
        {
          //find the number of leading zero bits
          unsigned short StartAt = 0;
          unsigned short LeadingZeroBitCount = 0;
          unsigned short ByteCtr = 0;

          while (BitOp::ExtractBits<BYTE>(code[ByteCtr], StartAt++, 1) == 0)
          {
            LeadingZeroBitCount++;
            if (StartAt % 8 == 0)
            {
              ByteCtr++;
              StartAt = 0;
            }
          }

          if (LeadingZeroBitCount == 0)
            codenum = 1;
          else
          {
            unsigned int RetVal = 0;
            unsigned short ReadCount = 0;
            while (ReadCount < LeadingZeroBitCount + 1)
            {
              RetVal = (RetVal | BitOp::ExtractBits<BYTE>(code[ByteCtr], StartAt++, 1));
              if (++ReadCount < LeadingZeroBitCount + 1)
              {
                //make room for next bit
                RetVal = RetVal << 1;

                if (StartAt % 8 == 0)
                {
                  ByteCtr++;
                  StartAt = 0;
                }
              }
            }
            codenum = RetVal - 1;
          }
          return (2 * LeadingZeroBitCount) + 1;
        }
        bool FindNextMatchingBitSequence(unsigned int sequencevalue, unsigned short numbits, unsigned int startat, const BYTE *data, unsigned int size, unsigned int& MatchPos)
        {
          MatchPos = size;
          if (size < numbits) return false;

          while (startat + numbits <= size)
          {
            if (BitOp::ToInteger<unsigned int>(data + startat, numbits) != sequencevalue)
              ++startat;
            else
            {
              MatchPos = startat;
              break;
            }
          }

          return (startat + numbits <= size);
        }
      };
    }
  }
} 