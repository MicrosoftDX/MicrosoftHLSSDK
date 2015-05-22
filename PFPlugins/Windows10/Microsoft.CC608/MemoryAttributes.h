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
#include "CC608CharColor.h"

namespace Microsoft { namespace CC608 {

  // attributes for a cell, this is very memory efficient, using one byte to 
  // store all of the attributes for a cell, might not be necessary but wanted
  // to use the least amount of memory possible for each cell
  class MemoryAttributes
  {
  public:
    MemoryAttributes(void);
    ~MemoryAttributes(void);

    void Clear();
    bool ContainsAttributes() const;

    void SetUnderline(bool value);
    bool IsUnderline() const;

    void SetItalics(bool value);
    bool IsItalics() const;

    void SetFlash(bool value);
    bool IsFlash() const;

    void SetColor(CC608CharColor value);
    CC608CharColor GetColor() const;

  private:
    void SetBit(unsigned char mask, bool value);
    bool GetBit(unsigned char mask) const;

  private: 
    unsigned char _attributes;
  };

}}

