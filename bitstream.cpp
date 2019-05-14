#include "bitstream.h"

BitStream::BitStream(uint8_t* data, int32_t bits)
{
    data_ = data;
    offset_ = 0;
    len_ = bits;
    error_ = false;
}

void BitStream::setBitStream(uint8_t* data, int32_t bits)
{
    data_ = data;
    offset_ = 0;
    len_ = bits;
    error_ = false;
}

uint32_t BitStream::readBits(int32_t num)
{
    uint32_t r = 0;
    while (num > 0)
    {
        if (offset_ >= len_)
        {
            error_ = true;
            return 0;
        }

        num--;
        if (data_[offset_ / 8] & (1 << (7 - (offset_ & 7))))
            r |= 1 << num;

        offset_++;
    }
    return r;
}

int32_t BitStream::showBits(int32_t num)
{
    int32_t r = 0, offs = offset_;
    while (num > 0)
    {
        if (offs >= len_)
        {
            error_ = true;
            return 0;
        }

        num--;
        if (data_[offs / 8] & (1 << (7 - (offs & 7))))
            r |= 1 << num;

        offs++;
    }
    return r;
}

int32_t BitStream::readGolombUE(int32_t maxbits)
{
    int32_t lzb = -1, bits = 0;
    for (int32_t b = 0; !b; lzb++, bits++)
    {
        if (bits > maxbits)
            return 0;
        b = readBits1();
    }
    return static_cast<int32_t>((1 << lzb) - 1 + readBits(lzb));
}

int32_t BitStream::readGolombSE()
{
    int32_t pos, v = readGolombUE();
    if (v == 0)
        return 0;

    pos = (v & 1);
    v = (v + 1) >> 1;
    return pos ? v : -v;
}


void BitStream::putBits(int32_t val, int32_t num)
{
    while (num > 0)
    {
        if (offset_ >= len_)
        {
            error_ = true;
            return;
        }

        num--;
        if (val & (1 << num))
            data_[offset_ / 8] |= 1 << (7 - (offset_ & 7));
        else
            data_[offset_ / 8] &= ~(1 << (7 - (offset_ & 7)));
        offset_++;
    }
}
