#pragma once

#include "pch.h"
#include "CC608CharColor.h"

namespace Microsoft { namespace CC608 {

// Simple struct to hold PAC decode information and help to make it unit testable
struct DecodedPac
{
  bool IsValid;
  bool Italics;
  bool Underline;
  short Row;
  short Indent;
  CC608CharColor Color;

  DecodedPac() : IsValid(true), Italics(false), Underline(false), Row(15), Indent(0), Color(CC608CharColor::White)
  {}
};

}}