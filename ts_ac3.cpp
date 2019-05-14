#include "ts_ac3.h"
#include "bitstream.h"

#include <algorithm>      // for max

#define AC3_HEADER_SIZE 7
#include <QDebug>
// Channel mode (audio coding mode)
enum AC3ChannelMode
{
    AC3_CHMODE_DUALMONO = 0,
    AC3_CHMODE_MONO,
    AC3_CHMODE_STEREO,
    AC3_CHMODE_3F,
    AC3_CHMODE_2F1R,
    AC3_CHMODE_3F1R,
    AC3_CHMODE_2F2R,
    AC3_CHMODE_3F2R
};

// possible frequencies
const uint16_t AC3SampleRateTable[3] = { 48000, 44100, 32000 };

// possible bitrates
const uint16_t AC3BitrateTable[19] = {
    32, 40, 48, 56, 64, 80, 96, 112, 128,
    160, 192, 224, 256, 320, 384, 448, 512, 576, 640
};

const uint8_t AC3ChannelsTable[8] = {
    2, 1, 2, 3, 3, 4, 4, 5
};

const uint16_t AC3FrameSizeTable[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
};

const uint8_t EAC3Blocks[4] = { 1, 2, 3, 6 };

enum EAC3FrameType
{
    EAC3_FRAME_TYPE_INDEPENDENT = 0,
    EAC3_FRAME_TYPE_DEPENDENT,
    EAC3_FRAME_TYPE_AC3_CONVERT,
    EAC3_FRAME_TYPE_RESERVED
};

AC3::AC3(uint16_t pid)
    : TsStream(pid)
{
    PTS_ = 0;
    DTS_ = 0;
    frameSize_ = 0;
    sampleRate_ = 0;
    channels_ = 0;
    bitRate_ = 0;
    esAllocInit_ = 1920 * 2;
}

AC3::~AC3()
{
}

void AC3::parse(STREAM_PKG* pkg)
{
    int32_t ptrOffset = esParsed_;
    int32_t remain;
    while ((remain = esLen_ - ptrOffset) > 8)
    {
        if (findHeaders(esBuf_ + ptrOffset, remain) < 0)
            break;
        ptrOffset++;
    }
    esParsed_ = ptrOffset;

    if (esFoundFrame_ && remain >= frameSize_)
    {
        bool streamChange = setAudioInformation(channels_, sampleRate_, bitRate_, 0, 0);
        pkg->pid = pid_;
        pkg->data = &esBuf_[ptrOffset];
        pkg->size = frameSize_;
        pkg->duration = 90000 * 1536 / sampleRate_;
        pkg->dts = DTS_;
        pkg->pts = PTS_;
        pkg->streamChange = streamChange;

        esConsumed_ = ptrOffset + frameSize_;
        esParsed_ = esConsumed_;
        esFoundFrame_ = false;
//        qDebug() << "ptrOffset" << ptrOffset
//                 << "pkg->pid" << pkg->pid
//                 << "pkg->data" << pkg->data
//                 << "pkg->size" << pkg->size
//                 << "pkg->duration" << pkg->duration
//                 << "pkg->dts" << pkg->dts
//                 << "pkg->pts" << pkg->dts
//                 << "pkg->streamChange" << pkg->streamChange
//                    ;
    }
}

int32_t AC3::findHeaders(uint8_t* buf, int32_t bufSize)
{
    if (esFoundFrame_ || bufSize < 9)
        return -1;

    uint8_t* bufPtr = buf;
    if ((bufPtr[0] == 0x0b && bufPtr[1] == 0x77))
    {
        BitStream bs(bufPtr + 2, AC3_HEADER_SIZE * 8);

        // read ahead to bsid to distinguish between AC-3 and E-AC-3
        int32_t bsid = bs.showBits(29) & 0x1F;
        if (bsid > 16)
            return 0;

        if (bsid <= 10)
        {
            // Normal AC-3
            bs.skipBits(16);
            int32_t fscod = bs.readBits(2);
            int32_t frmsizecod = bs.readBits(6);
            bs.skipBits(5);         // skip bsid, already got it
            bs.skipBits(3);         // skip bitstream mode
            int32_t acmod = bs.readBits(3);

            if (fscod == 3 || frmsizecod > 37)
                return 0;

            if (acmod == AC3_CHMODE_STEREO)
            {
                bs.skipBits(2);     // skip dsurmod
            }
            else
            {
                if ((acmod & 1) && acmod != AC3_CHMODE_MONO)
                    bs.skipBits(2);
                if (acmod & 4)
                    bs.skipBits(2);
            }
            int32_t lfeon = bs.readBits(1);
            int32_t srShift = std::max(bsid, 8) - 8;
            sampleRate_ = AC3SampleRateTable[fscod] >> srShift;
            bitRate_ = (AC3BitrateTable[frmsizecod >> 1] * 1000) >> srShift;
            channels_ = AC3ChannelsTable[acmod] + lfeon;
            frameSize_ = AC3FrameSizeTable[frmsizecod][fscod] * 2;
        }
        else
        {
            // Enhanced AC-3
            int32_t frametype = bs.readBits(2);
            if (frametype == EAC3_FRAME_TYPE_RESERVED)
                return 0;

            bs.readBits(3); // int substreamid

            frameSize_ = (bs.readBits(11) + 1) << 1;
            if (frameSize_ < AC3_HEADER_SIZE)
                return 0;

            int32_t numBlocks = 6;
            int32_t sr_code = bs.readBits(2);
            if (sr_code == 3)
            {
                int32_t sr_code2 = bs.readBits(2);
                if (sr_code2 == 3)
                    return 0;
                sampleRate_ = AC3SampleRateTable[sr_code2] / 2;
            }
            else
            {
                numBlocks = EAC3Blocks[bs.readBits(2)];
                sampleRate_ = AC3SampleRateTable[sr_code];
            }

            int32_t channelMode = bs.readBits(3);
            int32_t lfeon = bs.readBits(1);

            bitRate_ = (uint32_t)(8.0 * frameSize_ * sampleRate_ / (numBlocks * 256.0));
            channels_ = AC3ChannelsTable[channelMode] + lfeon;
        }
        esFoundFrame_ = true;
        DTS_ = curPts_;
        PTS_ = curPts_;
        curPts_ += 90000 * 1536 / sampleRate_;
        return -1;
    }
    return 0;
}

void AC3::reset()
{
    TsStream::reset();
}
