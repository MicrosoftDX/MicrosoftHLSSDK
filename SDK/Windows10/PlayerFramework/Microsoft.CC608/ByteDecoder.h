﻿#pragma once

#include <memory>
#include <vector>

#include "pch.h"
#include "Model.h"
#include "DecodeLogic.h"
#include "DecodedPac.h"

namespace Microsoft { namespace CC608 {

  enum class CC608ControlCode
  {
    NotAControlCode = 0,
    PAC,
    MidRow,
    MiscControl
  };

  // Decodes CC 608 bytes
  class ByteDecoder
  {
  public:
    ByteDecoder(std::shared_ptr<Model> model);
    void Initialize();
    void ParseBytes(const std::vector<byte_t>& data);
    
    // processes the user data wrapper and extracts byte codes
    std::vector<byte_t> Extract608ByteCodes(const std::vector<byte_t>& data);

    void SetCaptionTrack(const int captionTrack);

  private:
    void DecodeBytePair(byte_t data1, byte_t data2);
    void ProcessFailedParity() const;
    CC608ControlCode GetControlCode(const byte_t first, const byte_t second);
    
    bool IsPac(const byte_t first, const byte_t second);
    bool IsMidRowCode(const byte_t first, const byte_t second);
    bool IsMiscControlCode(const byte_t first, const byte_t second);
    
    void ProcessControlCode(const CC608ControlCode code, const byte_t first, const byte_t second);

    void DecodePac(const byte_t first, const byte_t second) const;
    void DecodeMiscControlCode(const byte_t first, const byte_t second) const;
    void ProcessMiscControlCodeFor_x14_x1C(byte_t second) const;
    void DecodeMidRow(const byte_t second) const;

    bool IsSpecialChar(const byte_t first, const byte_t second);
    void ProcessSpecialChar(const byte_t second) const;

    bool IsExtendedChar(const byte_t first, const byte_t second);
    void ProcessExtendedChar(const byte_t first, const byte_t second);
    void ProcessExtendedSpanishMiscOrFrench(const byte_t second);
    void ProcessExtendedPortugueseGermanDanish(const byte_t second);

    bool IsStandardChar(const byte_t ch) const;
    void ProcessPrintableCharacters(const byte_t first, const byte_t second) const;
    void ProcessPrintableChar(byte_t ch) const;

    byte_t _previousFirst;
    byte_t _previousSecond;
    bool _expectRepeat;

    std::shared_ptr<Model> _spModel;
    DecodeLogic _decodeLogic;

    // information on which track we are processing (field 1 or 2, and channel 1 or 2)
    short _desiredField;
    short _desiredChannel;
    bool _captionsActive;  // only process captions if true

    // the 608 data channel we last heard about from a control code (so all subsequent data is interpreted as part of this channel until a new code comes in)
    short _currentChannel;

    // header bytes ("GA94") used to locate the starting point of the 608 caption data
    static const std::vector<byte_t> _header;
    static std::vector<byte_t> PopulateHeader();
  };

}}