
#include "Timestamp.h" 

using namespace Microsoft::HLSClient::Private;

Timestamp::Timestamp() :ValueInTicks(0), Type(TimestampType::NONE)
{
}


std::shared_ptr<Timestamp> Timestamp::Parse(const BYTE* pcrdata, TimestampType type)
{
  std::shared_ptr<Timestamp> ret = std::make_shared<Timestamp>();
  if (type == TimestampType::PCR || type == TimestampType::OPCR)
  {
    unsigned long long _pcrbase = BitOp::ExtractBits(BitOp::ToInteger<unsigned long long>(pcrdata, 6), 16, 33);
    unsigned long long _pcrext = BitOp::ExtractBits(BitOp::ToInteger<unsigned long long>(pcrdata, 6), 16 + 39, 9);
    ret->ValueInTicks = ((_pcrbase * 300 + _pcrext) * 10000000 / 27000000);
  }
  else if (type == TimestampType::PTS || type == TimestampType::DTS)
  {
    unsigned long long _all = BitOp::ToInteger<unsigned long long>(pcrdata, 5);
    BYTE _ts_1 = static_cast<BYTE>(BitOp::ExtractBits(_all, 28, 3)); //why 28 ? Answer : unsigned long long is 64 bits - we just used 5*8 = 40 bits to create it. So (64 - 40) = 24 bits of offset from LTR + 4 bits for the flag field
    unsigned short _ts_2 = static_cast<unsigned short>(BitOp::ExtractBits(_all, 32, 15)); //why 32 ? see above comment
    unsigned short _ts_3 = static_cast<unsigned short>(BitOp::ExtractBits(_all, 48, 15)); //why 48 ? see above comment

    unsigned long long _ts = ((((unsigned long long)_ts_1) << 30) | (((unsigned long long)_ts_2) << 15)) | ((unsigned long long)(_ts_3));
    ret->ValueInTicks = _ts * 10000000 / 90000;
  }
  ret->Type = type;
  return ret;
}

 
