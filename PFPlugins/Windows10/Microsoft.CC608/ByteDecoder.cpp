#include "pch.h"
#include <algorithm>
#include <iostream>
#include "ByteDecoder.h"
#include "Model.h"
#include "Logger.h"

using namespace Microsoft::CC608;
using namespace std;

const std::vector<byte_t> ByteDecoder::_header = PopulateHeader();

// Populates the header bytes for the 608 data location logic
std::vector<byte_t> ByteDecoder::PopulateHeader()
{
  auto v = vector<byte_t>();

  // for debugging data alignment issues, this is 0x47413934 in hex
  v.push_back('G');
  v.push_back('A');
  v.push_back('9');
  v.push_back('4');
  
  return v;
}

ByteDecoder::ByteDecoder(shared_ptr<Model> model) : _spModel(model), _decodeLogic(), _desiredField(1), _desiredChannel(1), _currentChannel(1), _captionsActive(false)
{
  Initialize();
}

void ByteDecoder::Initialize()
{
  _previousFirst = 0x00;
  _previousSecond = 0x00;
  _expectRepeat = false;

  SetCaptionTrack(0);
}

std::vector<byte_t> ByteDecoder::Extract608ByteCodes(const std::vector<byte_t>& data)
{
  // user data envelope format:
  // 
  // Picture Header:
  //
  // user_data_start_code (4 bytes) 0x000001B2 (LIKELY NOT INCLUDED IN WHAT WE GET HERE)
  // user_identifier (4 bytes) "GA94" (THIS IS THE KEY DATA WE SEARCH FOR TO IDENTIFY OUR STARTING POINT FOR PROCESSING)
  // user_data_type_code (1 byte) 0x03
  // 
  // user_data_type_structure:
  //
  // reserved (1 bit) IGNORE (probably 1)
  // process_cc_data_flag (1 bit) IF ZERO, IGNORE ALL DATA IN ARRAY!
  // zero_bit (1 bit) (probably 0)
  // cc_count (5 bits) how many packets of CC data will be expected
  // reserved (8 bits = 1 byte) 0xFF
  // 
  // CC Packets: (expect cc_count of them!)
  //
  // one_bit (1 bit) should be set (1)
  // reserved (4 bits) 1111
  // cc_valid (1 bit) ignore this packet if not set!
  // cc_type (2 bits) 00 = 608 field 1, 01 = 608 field 2, 10 = DTVCC Channel Packet Data (708), 11 = DTVCC Channel Packet Start (708)
  // 

  vector<byte_t> result;

  // find the "GA94" header
  auto iter = std::search(stdExt::cbegin(data), stdExt::cend(data), stdExt::cbegin(_header), stdExt::cend(_header));

  if (iter == end(data))
  {
    // correct header not found in data
    DebugWrite("WARNING: Correct 'GA94' header not found! (Skipping entire payload)");
    return result;
  }

  // skip the header bytes we just found
  iter += 4;

  auto size = end(data) - iter;

  if (size < 6)
  {
    DebugWrite("WARNING: Insufficient data found! (Skipping entire payload)");
    return result;
  }
  
  if (*iter++ != 0x03)
  {
    // we didn't get the correct user_data_type_code
    DebugWrite("WARNING: Invalid user_data_type code encountered! (Skipping entire payload)");
    return result;
  }

  // check the "process_cc_data_flag" bit and don't process anything if zero
  byte_t user_data_type_first_byte = *iter++;

  byte_t process_cc_data_flag = user_data_type_first_byte & 0x40;
  if (process_cc_data_flag == 0)
  {
    DebugWrite("WARNING: Skipping entire payload as process_cc_data_flag is not set!");
    return result;
  }

  // lower 5 bits of header are count
  int packetCount = user_data_type_first_byte & 0x1F;

  if ((packetCount * 3 + 4) > size)
  {
    // we don't have enough data to process!
    // (each packet is 3 bytes, there are 3 bytes of header, and we expect 0xFF after the cc data packets, so that's 3 + 1 = 4 additional bytes)
    DebugWrite("WARNING: Skipping processing of captions payload due to insufficient data! (Skipping entire payload)");
    return result;
  }

  // skip the second user_data_type byte (usually 0xFF) to move to the start of the data
  ++iter;

  for(int i = 0; i != packetCount; ++i)
  {
    byte_t ccFlag = *iter++;
    byte_t data1 = *iter++;
    byte_t data2 = *iter++;

    // this is the "cc_valid" bit
    bool isValidPair = ((ccFlag & 0x04) == 0x04);
    if (!isValidPair)
    {
      DebugWrite("INFO: Skipping processing of 608 byte pair due to cc_valid bit not set");
      continue;
    }

    // cc_type is two bits: 00 = field 1 608, 01 = field 2 608, anything else is non-608
    byte_t ccType = ccFlag & 0x03;
    switch(ccType)
    {
    case 0x00:
      if (_desiredField != 1)
      {
        // this is field 1 data, but we aren't processing field 1
        continue;
      }
      break;

    case 0x01:
      if (_desiredField != 2)
      {
        // this is field 2 data, but we aren't processing field 2
        continue;
      }
      break;

    default:
      // this is not 608 data
      continue;
    }

    // if we got here, add the data to the result--these are valid byte codes (but not yet checked for parity)
    //auto d1 = data1 & 0x7F;
    //auto d2 = data2 & 0x7F;
    //DebugWrite("BYTE PAIR: " << std::hex << d1 << "  " << d2);

    result.push_back(data1);
    result.push_back(data2);
  }

  return result;
}

void ByteDecoder::SetCaptionTrack(const int captionTrack)
{
  auto oldField = _desiredField;

  switch(captionTrack)
  {
  case 0:
    _captionsActive = false;
    break;

  case 1:
    _captionsActive = true;
    _desiredField = 1;
    _desiredChannel = 1;
    break;

  case 2:
    _captionsActive = true;
    _desiredField = 1;
    _desiredChannel = 2;
    break;

  case 3:
    _captionsActive = true;
    _desiredField = 2;
    _desiredChannel = 1;
    break;

  case 4:
    _captionsActive = true;
    _desiredField = 2;
    _desiredChannel = 2;
    break;

  default:
    assert(false);
  }

  if (oldField != _desiredField)
  {
    // if we switched fields, then the current channel info is out of date--reset it to one (not necessarily correct but we don't have any better data)
    _currentChannel = 1;
  }
}

void ByteDecoder::ParseBytes(const std::vector<byte_t>& data)
{
  if (!_captionsActive)
  {
    // not currently displaying any captions--do nothing
    return;
  }

  for (size_t i = 0; i < data.size(); i += 2)
  {
    DecodeBytePair(data[i], data[i + 1]);
  }
}

void ByteDecoder::DecodeBytePair(byte_t first, byte_t second)
{
  if ((first == 0x80) && (second == 0x80))
  {
    // both null characters--skip processing, AND do NOT change the repeat expectations (as these nulls may have been inserted between two repeated control codes)
    return;
  }

  // check parity before we do any further processing
  if (!_decodeLogic.ParityValid(first) || !_decodeLogic.ParityValid(second))
  {
    DebugWrite("WARNING: Parity check failed on " << std::hex << first << "  " << second);
    ProcessFailedParity();
    return;
  }

  // now get rid of parity bits
  first &= 0x7f;
  second &= 0x7f;

  CC608ControlCode codeType = GetControlCode(first, second);

  if (CC608ControlCode::NotAControlCode != codeType)
  {
    ProcessControlCode(codeType, first, second);
    return;
  }

  // since the current character is not a control code, we should not be expecting repeats...
  _expectRepeat = false;

  if (IsSpecialChar(first, second))
  {
    ProcessSpecialChar(second);
    return;
  }

  if (IsExtendedChar(first, second))
  {
    ProcessExtendedChar(first, second);
    return;
  }

  ProcessPrintableCharacters(first, second);
}

void ByteDecoder::ProcessFailedParity() const
{
  // as per the spec, for parity failures when a character or space would have been emitted, emit a solid block instead
  const wchar_t solid_block = L'\x2588';

  // just emit a solid block for an invalid pair (otherwise we need to duplicate all the logic to decode without setting _currentChannel)
  _spModel->Character(solid_block);
}

CC608ControlCode ByteDecoder::GetControlCode(const byte_t first, const byte_t second)
{
  if (IsPac(first, second))
    return CC608ControlCode::PAC;

  if (IsMidRowCode(first, second))
    return CC608ControlCode::MidRow;

  if (IsMiscControlCode(first, second))
    return CC608ControlCode::MiscControl;

  return CC608ControlCode::NotAControlCode;
}

bool ByteDecoder::IsPac(const byte_t first, const byte_t second)
{
  // data channel 1
  if ((0x10 <= first && 0x17 >= first) && (0x40 <= second && 0x7F >= second))
  {
    _currentChannel = 1;
    return true;
  }

  // data channel 2
  if ((0x18 <= first && 0x1F >= first) && (0x40 <= second && 0x7F >= second))
  {
    _currentChannel = 2;
    return true;
  }

  return false;
}

bool ByteDecoder::IsMidRowCode(const byte_t first, const byte_t second)
{
  // data channel 1
  if ((0x11 == first) && (0x20 <= second && 0x2F >= second))
  {
    _currentChannel = 1;
    return true;
  }

  // data channel 2
  if ((0x19 == first) && (0x20 <= second && 0x2F >= second))
  {
    _currentChannel = 2;
    return true;
  }

  return false;
}

bool ByteDecoder::IsMiscControlCode(const byte_t first, const byte_t second)
{
  // tab offsets (0x17 is data channel 1)
  if ((0x21 <= second && 0x23 >= second) && (0x17 == first))
  {
    _currentChannel = 1;
    return true;
  }

  // tab offsets (0x1F is data channel 2)
  if ((0x21 <= second && 0x23 >= second) && (0x1F == first))
  {
    _currentChannel = 2;
    return true;
  }

  // data channel 1 misc control code
  if ((0x14 == first) && (0x20 <= second && 0x2F >= second))
  {
    _currentChannel = 1;
    return true;
  }

  // data channel 2 misc control code
  if ((0x1C == first) && (0x20 <= second && 0x2F >= second))
  {
    _currentChannel = 2;
    return true;
  }

  return false;
}

void ByteDecoder::ProcessControlCode(const CC608ControlCode code, const byte_t first, const byte_t second)
{
  // skip processing if wrong channel
  if (_currentChannel != _desiredChannel)
  {
    return;
  }

  if (_expectRepeat && (_previousFirst == first) && (_previousSecond == second))
  {
    // yes, was exact repeat--ignore but don't expect more repeats
    _expectRepeat = false;
    return;
  }

  // we have a control code that is not a repeat--we may get another one!
  _expectRepeat = true;
  _previousFirst = first;
  _previousSecond = second;
  
  switch(code)
  {
    case CC608ControlCode::PAC:
      DecodePac(first, second);
      break;

    case CC608ControlCode::MidRow:
      DecodeMidRow(second);
      break;

    case CC608ControlCode::MiscControl:
      DecodeMiscControlCode(first, second);
      break;
  }
}

void ByteDecoder::DecodePac(const byte_t first, const byte_t second) const
{
  auto p = _decodeLogic.DecodePac(first, second);

  if (p.IsValid)
  {
    _spModel->Pac(p.Row, p.Indent, p.Color, p.Underline, p.Italics);
  }
}

void ByteDecoder::DecodeMidRow(const byte_t second) const
{
  switch(second)
  {
    case 0x20:  // White
      _spModel->MidRowCode(CC608CharColor::White, false);
      break;

    case 0x21: 
      _spModel->MidRowCode(CC608CharColor::White, true);
      break;

    case 0x22: // Green
      _spModel->MidRowCode(CC608CharColor::Green, false);
      break;

    case 0x23: 
      _spModel->MidRowCode(CC608CharColor::Green, true);
      break;

    case 0x24: // Blue
      _spModel->MidRowCode(CC608CharColor::Blue, false);
      break;

    case 0x25:
      _spModel->MidRowCode(CC608CharColor::Blue, true);
      break;

    case 0x26: // Cyan
      _spModel->MidRowCode(CC608CharColor::Cyan, false);
      break;

    case 0x27:
      _spModel->MidRowCode(CC608CharColor::Cyan, true);
      break;

    case 0x28: // Red
      _spModel->MidRowCode(CC608CharColor::Red, false);
      break;

    case 0x29:
      _spModel->MidRowCode(CC608CharColor::Red, true);
      break;

    case 0x2A: // Yellow
      _spModel->MidRowCode(CC608CharColor::Yellow, false);
      break;

    case 0x2B:
      _spModel->MidRowCode(CC608CharColor::Yellow, true);
      break;

    case 0x2C: // Magenta
      _spModel->MidRowCode(CC608CharColor::Magenta, false);
      break;

    case 0x2D:
      _spModel->MidRowCode(CC608CharColor::Magenta, true);
      break;

    case 0x2E: // Italics (special--different call)
      _spModel->MidRowCode(false, true);
      break;

    case 0x2F:
      _spModel->MidRowCode(true, true);
      break;
  }
}

void ByteDecoder::DecodeMiscControlCode(const byte_t first, const byte_t second) const
{
  switch(first)
  {
    case 0x14:  // Data Channel 1
    case 0x1C:  // Data Channel 2
      ProcessMiscControlCodeFor_x14_x1C(second);
      break;

    case 0x17: // Data Channel 1
    case 0x1F: // Data Channel 2
      switch(second)
      {
        case 0x21: // TO1
          _spModel->TabOffset(1);
          break;

        case 0x22: // TO2
          _spModel->TabOffset(2);
          break;

        case 0x23: // TO3
          _spModel->TabOffset(3);
          break;
      }
      break;
  }
}

void ByteDecoder::ProcessMiscControlCodeFor_x14_x1C(byte_t second) const
{
    switch(second)
    {
      case 0x20:  // RCL
        _spModel->ResumeCaptionLoading();
        break;

      case 0x21: // BS
        _spModel->Backspace();
        break;

      case 0x22: // AOF (Reserved)
      case 0x23: // AON (Reserved)
        break;

      case 0x24: // DER
        _spModel->DeleteToEndOfRow();
        break;

      case 0x25: // RU2
        _spModel->RollUpCaptions(2);
        break;

      case 0x26: // RU3
        _spModel->RollUpCaptions(3);
        break;

      case 0x27: // RU4
        _spModel->RollUpCaptions(4);
        break;

      case 0x28: // FON
        _spModel->FlashOn();
        break;

      case 0x29: // RDC
        _spModel->ResumeDirectCaptioning();
        break;

      case 0x2A: // TR (Text Restart--not captioning related)
      case 0x2B: // RTD (Resume Text Display--not captioning related)
        break;

      case 0x2C: // EDM
        _spModel->EraseDisplayedMemory();
        break;

      case 0x2D: // CR
        _spModel->CarriageReturn();
        break;

      case 0x2E: // ENM
        _spModel->EraseNonDisplayedMemory();
        break;

      case 0x2F: // EOC (End of Caption--flip memories)
        _spModel->EndOfCaption();
        break;
    }
}

bool ByteDecoder::IsSpecialChar(const byte_t first, const byte_t second)
{
  // data channel 1
  if (0x11 == first && (0x30 <= second && 0x3F >= second))
  {
    _currentChannel = 1;
    return true;
  }

  // data channel 2
  if (0x19 == first && (0x30 <= second && 0x3F >= second))
  {
    _currentChannel = 2;
    return true;
  }

  return false;
}

bool ByteDecoder::IsExtendedChar(const byte_t first, const byte_t second)
{
  if (first == 0x12 && (second >= 0x20 && second <= 0x3F))
  {
    // Spanish, misc, and French
    _currentChannel = 1;
    return true;
  }

  if (first == 0x1A && (second >= 0x20 && second <= 0x3F))
  {
    // Spanish, misc, and French
    _currentChannel = 2;
    return true;
  }

  if (first == 0x13 && (second >= 0x20 && second <= 0x3F))
  {
    // Portuguese, German, and Danish
    _currentChannel = 1;
    return true;
  }

  if (first == 0x1B && (second >= 0x20 && second <= 0x3F))
  {
    // Portuguese, German, and Danish
    _currentChannel = 1;
    return true;
  }

  return false;
}

void ByteDecoder::ProcessExtendedChar(const byte_t first, const byte_t second)
{
  // skip processing if wrong channel
  if (_currentChannel != _desiredChannel)
  {
    return;
  }

  if (first == 0x12 || first == 0x1A)
  {
    ProcessExtendedSpanishMiscOrFrench(second);
  }
  else if (first == 0x13 || first == 0x1B)
  {
    ProcessExtendedPortugueseGermanDanish(second);
  }
}

void ByteDecoder::ProcessExtendedSpanishMiscOrFrench(const byte_t second)
{
  assert(second >= 0x20 && second <=0x3F);

  // backspace over the character which is the poor stand-in for the real character
  // (as some decoders do not support the extended characters, they send "u", then they send extended "u with circumflex" which might not be recognized
  // therefore--we need to delete the plain "u")
  _spModel->Backspace();

  switch(second)
  {
  // SPANISH EXTENDED CHARACTER SET 

  case 0x20: // capital A with acute accent (Á)
    _spModel->Character(L'\x00C1');
    break;

  case 0x21: // capital E with acute accent (É)
    _spModel->Character(L'\x00C9');
    break;

  case 0x22: // capital O with acute accent (Ó)
    _spModel->Character(L'\x00D3');
    break;

  case 0x23: // capital U with acute accent (Ú)
    _spModel->Character(L'\x00DA');
    break;

  case 0x24: // capital U with diaeresis or umlaut (Ü)
    _spModel->Character(L'\x00DC');
    break;
     
  case 0x25: // small u with diaeresis or umlaut (ü)
    _spModel->Character(L'\x00FC');
    break;

  case 0x26: // opening single quote (‘)
    _spModel->Character(L'\x2018');
    break;

  case 0x27: // inverted exclamation mark (¡)
    _spModel->Character(L'\x00A1');
    break;

  // MISCELLANEOUS EXTENDED CHARACTER SET
  
  case 0x28: // asterisk
    _spModel->Character(L'*');
    break;

  case 0x29: // plain single quote (')--the one that doesn't curl
    _spModel->Character(L'\x0027');
    break;

  case 0x2A: // em dash (—)
    _spModel->Character(L'\x2014');
    break;

  case 0x2B: // copyright (©)
    _spModel->Character(L'\x00A9');
    break;

  case 0x2C: // Servicemark (SM)
    _spModel->Character(L'\x2120');
    break;

  case 0x2D: // round bullet (●)
    _spModel->Character(L'\x2022');
    break;

  case 0x2E: // opening double quotes (“)
    _spModel->Character(L'\x201C');
    break;

  case 0x2F: // closing double quotes (”)
    _spModel->Character(L'\x201D');
    break;

  // FRENCH EXTENDED CHARACTER SET

  case 0x30: // capital A with grave accent (À)
    _spModel->Character(L'\x00C0');
    break;

  case 0x31: // capital A with circumflex accent (Â)
    _spModel->Character(L'\x00C2');
    break;

  case 0x32: // capital C with cedilla (Ç)
    _spModel->Character(L'\x00C7');
    break;

  case 0x33: // capital E with grave accent (È)
    _spModel->Character(L'\x00C8');
    break;

  case 0x34: // capital E with circumflex accent (Ê)
    _spModel->Character(L'\x00CA');
    break;

  case 0x35: // capital E with diaeresis or umlaut (Ë)
    _spModel->Character(L'\x00CB');
    break;

  case 0x36: // small e with diaeresis or umlaut (ë)
    _spModel->Character(L'\x00EB');
    break;

  case 0x37: // capital I with circumflex accent (Î)
    _spModel->Character(L'\x00CE');
    break;

  case 0x38: // capital I with diaeresis or umlaut (Ï)
    _spModel->Character(L'\x00CF');
    break;

  case 0x39: // small i with diaeresis or umlaut (ï)
    _spModel->Character(L'\x00EF');
    break;

  case 0x3A: // capital O with circumflex (Ô)
    _spModel->Character(L'\x00D4');
    break;

  case 0x3B: // capital U with grave accent (Ù)
    _spModel->Character(L'\x00D9');
    break;

  case 0x3C: // small u with grave accent (ù)
    _spModel->Character(L'\x00F9');
    break;

  case 0x3D: // capital U with circumflex accent (Û)
    _spModel->Character(L'\x00DB');
    break;

  case 0x3E: // opening guillemets («)
    _spModel->Character(L'\x00AB');
    break;

  case 0x3F: // closing guillemets (»)
    _spModel->Character(L'\x00BB');
    break;

  default:
    assert(false);
  }
}

void ByteDecoder::ProcessExtendedPortugueseGermanDanish(const byte_t second)
{
  assert(second >= 0x20 && second <= 0x3F);

  // backspace over the character which is the poor stand-in for the real character
  // (as some decoders do not support the extended characters, they send "u", then they send extended "u with circumflex" which might not be recognized
  // therefore--we need to delete the plain "u")
  _spModel->Backspace();

  switch(second)
  {
  // PORTUGUESE EXTENDED CHARACTER SET

  case 0x20: // capital A with tilde (Ã)
    _spModel->Character(L'\x00C3');
    break;

  case 0x21: // small a with tilde (ã)
    _spModel->Character(L'\x00E3');
    break;

  case 0x22: // capital I with acute accent (Í)
    _spModel->Character(L'\x00CD');
    break;

  case 0x23: // capital I with grave accent (Ì)
    _spModel->Character(L'\x00CC');
    break;

  case 0x24: // small i with grave accent (ì)
    _spModel->Character(L'\x00EC');
    break;

  case 0x25: // capital O with grave accent (Ò)
    _spModel->Character(L'\x00D2');
    break;

  case 0x26: // small o with grave accent (ò)
    _spModel->Character(L'\x00F2');
    break;

  case 0x27: // capital O with tilde (Õ)
    _spModel->Character(L'\x00D5');
    break;

  case 0x28: // small o with tilde (õ)
    _spModel->Character(L'\x00F5');
    break;

  case 0x29: // opening brace ({)
    _spModel->Character(L'\x007B');
    break;

  case 0x2A: // closing brace (})
    _spModel->Character(L'\x007D');
    break;

  case 0x2B: // backslash (\)
    _spModel->Character(L'\x005C');
    break;

  case 0x2C: // caret (^)
    _spModel->Character(L'^');
    break;

  case 0x2D: // underbar
    _spModel->Character(L'_');
    break;

  case 0x2E: // pipe 
    _spModel->Character(L'|');
    break;

  case 0x2F: // tilde
    _spModel->Character(L'~');
    break;

  // GERMAN EXTENDED CHARACTER SET

  case 0x30: // capital A with diaeresis or umlaut (Ä)
    _spModel->Character(L'\x00C4');
    break;

  case 0x31: // small a with diaeresis or umlaut (ä)
    _spModel->Character(L'\x00E4');
    break;

  case 0x32: // capital O with diaeresis or umlaut (Ö)
    _spModel->Character(L'\x00D6');
    break;

  case 0x33: // small o with diaeresis or umlaut (ö)
    _spModel->Character(L'\x00F6');
    break;

  case 0x34: // eszett (mall sharp s) (ß)
    _spModel->Character(L'\x00DF');
    break;

  case 0x35: // yen (¥)
    _spModel->Character(L'\x00A5');
    break;

  case 0x36: // non-specific currency sign (¤)
    _spModel->Character(L'\x00A4');
    break;

  case 0x37: // vertical bar (⏐)
    _spModel->Character(L'\x007C');
    break;

  // DANISH EXTENDED CHARACTER SET

  case 0x38: // capital A with ring (Å)
    _spModel->Character(L'\x00C5');
    break;

  case 0x39: // small a with ring (å)
    _spModel->Character(L'\x00E5');
    break;

  case 0x3A: // capital O with slash (Ø)
    _spModel->Character(L'\x00D8');
    break;

  case 0x3B: // small o with slash (ø)
    _spModel->Character(L'\x00F8');
    break;

  case 0x3C: // upper left corner (⎡)
    _spModel->Character(L'\x231C');
    break;

  case 0x3D: // upper right corner (⎤)
    _spModel->Character(L'\x231D');
    break;

  case 0x3E: // lower left corner (⎣)
    _spModel->Character(L'\x231E');
    break;

  case 0x3F: // lower right corner (⎦)
    _spModel->Character(L'\x231F');
    break;

  default:
    assert(false);
  }
}

void ByteDecoder::ProcessSpecialChar(const byte_t second) const
{
  // skip processing if wrong channel
  if (_currentChannel != _desiredChannel)
  {
    return;
  }

  switch(second)
  {
    case 0x30: // Registered trade mark
      _spModel->Character(L'\x00AE');
      break;

    case 0x31: // degree sign
      _spModel->Character(L'\x00B0');
      break;

    case 0x32: // 1/2
      _spModel->Character(L'\x00BD');
      break;

    case 0x33: // Inverse query (upside-down question mark)
      _spModel->Character(L'\x00BF');
      break;

    case 0x34:  // Trademark symbol
      _spModel->Character(L'\x2122');
      break;

    case 0x35: // cents
      _spModel->Character(L'\x00A2');
      break;

    case 0x36: // pounds sterling symbol
      _spModel->Character(L'\x00A3');
      break;

    case 0x37: // musical note
      _spModel->Character(L'\x266A');
      break;
    
    case 0x38: // lower case a with grave accent
      _spModel->Character(L'\x00E0');
      break;

    case 0x39: // transparent space
      _spModel->TransparentSpace();
      break;

    case 0x3A: // lower case e with grave accent
      _spModel->Character(L'\x00E8');
      break;

    case 0x3B: // lower case a with circumflex
      _spModel->Character(L'\x00E2');
      break;

    case 0x3C: // lower case e with circumflex
      _spModel->Character(L'\x00EA');
      break;

    case 0x3D: // lower case i with circumflex
      _spModel->Character(L'\x00EE');
      break;

    case 0x3E: // lower case o with circumflex
      _spModel->Character(L'\x00F4');
      break;

    case 0x3F: // lower case u with circumflex
      _spModel->Character(L'\x00FB');
      break;
  }
}

void ByteDecoder::ProcessPrintableCharacters(const byte_t first, const byte_t second) const
{
  // skip processing if wrong channel
  if (_currentChannel != _desiredChannel)
  {
    return;
  }

  if (!(first >= 0x00 && first <= 0x0F))
  {
    ProcessPrintableChar(first);
  }

  ProcessPrintableChar(second);
}

void ByteDecoder::ProcessPrintableChar(byte_t ch) const
{
  // if it's the null character, then do not process
  if (ch == 0x00)
  {
    return;
  }

  if (!IsStandardChar(ch)) 
  {
    return;
  }

  wchar_t c = ' ';

  switch(ch)
  {
    case 0x2A: // lower case a with acute accent
      c = L'\x00E1';
      break;

    case 0x5C: // lower case e with acute accent
      c = L'\x00E9';
      break;

    case 0x5E: // lower case i with acute accent
      c = L'\x00ED';
      break;

    case 0x5F: // lower case o with acute accent
      c = L'\x00F3';
      break;

    case 0x60: // lower case u with acute accent
      c = L'\x00FA';
      break;

    case 0x7B: // lower case c with cedilla
      c = L'\x00E7';
      break;

    case 0x7C: // division sign
      c = L'\x00F7';
      break;

    case 0x7D:  // upper case N with tilde
      c = L'\x00D1';
      break;

    case 0x7E: // lower case n with tilde
      c = L'\x00F1';
      break;

    case 0x7F: // solid block
      c = L'\x2588';
      break;

    case 0x27: // apostrophe (as per the spec, we want it to be the curved one)
      c = L'\x2019';
      break;

    default:
      c = wchar_t(ch);
  }

  _spModel->Character(c);
  return;
}

bool ByteDecoder::IsStandardChar(const byte_t ch) const
{
  return (0x20 <= ch && ch <= 0x7F);
}

