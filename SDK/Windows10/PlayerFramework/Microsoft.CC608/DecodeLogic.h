#pragma once

#include "pch.h"
#include <vector>
#include "DecodedPac.h"

namespace Microsoft { namespace CC608 {

// unit-testable decoding logic
class DecodeLogic
{
public:
  DecodeLogic(void);
  ~DecodeLogic(void);

  DecodedPac DecodePac(const byte_t first, const byte_t second) const;
  bool ParityValid(byte_t b) const;

private:
  std::vector<short> _pacToRow;
};

}}