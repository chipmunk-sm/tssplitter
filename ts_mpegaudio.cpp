#include "ts_mpegaudio.h"
#include "bitstream.h"

const uint16_t frequencyTable[3] = { 44100, 48000, 32000 };
const uint16_t bitrateTable[2][3][15] =
{
    {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
        {0, 32, 48, 56, 64,  80,  96,  112, 128, 160, 192, 224, 256, 320, 384 },
        {0, 32, 40, 48, 56,  64,  80,  96,  112, 128, 160, 192, 224, 256, 320 }
    },
    {
        {0, 32, 48, 56, 64,  80,  96,  112, 128, 144, 160, 176, 192, 224, 256},
        {0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160},
        {0, 8,  16, 24, 32,  40,  48,  56,  64,  80,  96,  112, 128, 144, 160}
    }
};

MPEG2Audio::MPEG2Audio(uint16_t pid)
    : TsStream(pid)
{
    PTS_ = 0;
    DTS_ = 0;
    frameSize_ = 0;
    sampleRate_ = 0;
    channels_ = 0;
    bitRate_ = 0;
    esAllocInit_ = 2048;
}

MPEG2Audio::~MPEG2Audio()
{
}

void MPEG2Audio::parse(STREAM_PKG* pkg)
{
    int32_t p = esParsed_, c;
    while ((c = esLen_ - p) > 3)
    {
        if (findHeaders(esBuf_ + p, c) < 0)
            break;
        p++;
    }
    esParsed_ = p;

    if (esFoundFrame_ && c >= frameSize_)
    {
        bool streamChange = setAudioInformation(channels_, sampleRate_, bitRate_, 0, 0);
        pkg->pid = pid_;
        pkg->data = &esBuf_[p];
        pkg->size = frameSize_;
        pkg->duration = 90000 * 1152 / sampleRate_;
        pkg->dts = DTS_;
        pkg->pts = PTS_;
        pkg->streamChange = streamChange;

        esConsumed_ = p + frameSize_;
        esParsed_ = esConsumed_;
        esFoundFrame_ = false;
    }
}

int32_t MPEG2Audio::findHeaders(uint8_t* buf, int32_t bufSize)
{
    if (esFoundFrame_)
        return -1;

    if (bufSize < 4)
        return -1;

    uint8_t* bufPtr = buf;

    if ((bufPtr[0] == 0xFF && (bufPtr[1] & 0xE0) == 0xE0))
    {
        BitStream bs(bufPtr, 4 * 8);
        bs.skipBits(11); // syncword

        int32_t audioVersion = bs.readBits(2);
        if (audioVersion == 1)
            return 0;
        int32_t mpeg2 = !(audioVersion & 1);
        int32_t mpeg25 = !(audioVersion & 3);

        int32_t layer = bs.readBits(2);
        if (layer == 0)
            return 0;
        layer = 4 - layer;

        bs.skipBits(1); // protetion bit
        int32_t bitrateIndex = bs.readBits(4);
        if (bitrateIndex == 15 || bitrateIndex == 0)
            return 0;
        bitRate_ = bitrateTable[mpeg2][layer - 1][bitrateIndex] * 1000;

        int32_t sampleRateIndex = bs.readBits(2);
        if (sampleRateIndex == 3)
            return 0;
        sampleRate_ = frequencyTable[sampleRateIndex] >> (mpeg2 + mpeg25);

        int32_t padding = bs.readBits1();
        bs.skipBits(1); // private bit
        int32_t channelMode = bs.readBits(2);
        channels_ = (channelMode == 11 ? 1 : 2);

        if (layer == 1)
            frameSize_ = (12 * bitRate_ / sampleRate_ + padding) * 4;
        else
            frameSize_ = 144 * bitRate_ / sampleRate_ + padding;

        esFoundFrame_ = true;
        DTS_ = curPts_;
        PTS_ = curPts_;
        curPts_ += 90000 * 1152 / sampleRate_;
        return -1;
    }
    return 0;
}
