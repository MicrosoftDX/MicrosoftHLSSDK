#if !defined _TSConstants_h
#define _TSConstants_h

#pragma once

#define SYNC_BYTE_VALUE 0x47
#define SAMPLE_BUFFER_READY_COUNT 5;


namespace Microsoft {
  namespace HLSClient {
    namespace Private {


      enum PESStreamIDType
      {
        PROGRAM_STREAM_MAP = 0xBC,
        PRIVATE_STREAM_1 = 0xBD,
        PADDING_STREAM = 0xBE,
        PRIVATE_STREAM_2 = 0xBF,
        ECM_STREAM = 0xF0,
        EMM_STREAM = 0xF1,
        ANNEXB_OR_DSMCC = 0xF2,
        PROGRAM_STREAM_DIRECTORY = 0xFF,
        ISO_13522_STREAM = 0xF3,
        H222_TYPE_A = 0xF4,
        H222_TYPE_B = 0xF5,
        H222_TYPE_C = 0xF6,
        H222_TYPE_D = 0xF7,
        H222_TYPE_E = 0xF8,
        ANCILLARY_STREAM = 0xF9
      };

      enum ContentType
      {
        AUDIO, VIDEO, METADATA, UNKNOWN
      };

      enum TimestampType
      {
        NONE, PCR, OPCR, PTS, DTS
      };
    }
  }
}

#endif