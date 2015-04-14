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

#include <wtypes.h>
#include <memory>
#include <vector>
#include <assert.h>

namespace Microsoft {
  namespace HLSClient {
    namespace Private {

      ///<summary>Bit manipulation operations</summary>
      class BitOp
      {
      public:
        ///<summary>Converts a byte array to a specific integral type T</summary>    
        ///<param name='data'>Byte array</param>
        ///<param name='size'>array length</param>
        ///<returns>A value of type T</returns>
        template<typename T>
        static T ToInteger(const BYTE* data, unsigned short size)
        {
          T ret = 0;
          for (unsigned short i = 0; i < size; i++)
          {
            if (i > 0 && ret != 0) ret <<= 8;
            memcpy_s(&ret, 1, data + i, 1);
          }
          return ret;
        }



        ///<summary>Pads a byte array with leading zero's</summary>
        ///<param name='data'>Byte array</param>
        ///<param name='numBytes'>Array size</param>
        ///<param name='targetNumBytes'>Desired array size after padding</param>
        ///<returns>A vector of BYTE</returns>
        static std::shared_ptr<std::vector<BYTE>> PadZeroLeading(BYTE *data, unsigned int numBytes, unsigned int targetNumBytes)
        {
          auto ret = std::make_shared<std::vector<BYTE>>();
          for (unsigned int num = 0; num < targetNumBytes; num++)
          {
            if (num < targetNumBytes - numBytes)
              ret->push_back(0);
            else
              ret->push_back(data[num - (targetNumBytes - numBytes)]);
          }

          return ret;
        }

        ///<summary>Extracts a specified number of bits from a value of integral type T starting at a specified position</summary>
        ///<param name='in'>Value of type T</param>
        ///<param name='startat'>Position to start extraction at</param>
        ///<param name='numbits'>Number of bits to extract</param>
        ///<returns>The resultant bits as a value of the source type</returns>
        template<typename T>
        static T ExtractBits(T in, unsigned short startat, unsigned short numbits)
        {
          unsigned short size = sizeof(in) * 8;

          assert((numbits > 0) && (startat >= 0) && (startat + numbits <= size));

          if (startat == 0)
          {
            if (startat + numbits == size) //need the whole thing
              return in;
            else
              return in >> (size - numbits); //need the first numbits bits
          }
          else
          {
            if (startat + numbits == size) //we need last numbits bits
              return in ^ ((in >> numbits) << numbits);
            else
            {
              T temp = in >> (size - (startat + numbits));
              return temp ^ ((temp >> numbits) << numbits);
            }
          }
        }
      };
    }
  }
} 