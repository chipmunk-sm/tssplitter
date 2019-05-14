
#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <QString>

class BitStream
{
private:
    uint8_t* data_;
    int32_t  offset_;
    int32_t  len_;
    bool    error_;

public:
    BitStream(uint8_t* data, int32_t bits);

    void   setBitStream(uint8_t* data, int32_t bits);
    uint32_t readBits(int32_t num);
    int32_t showBits(int32_t num);
    int32_t readGolombUE(int32_t maxbits = 32);
    int32_t readGolombSE();
    void   putBits(int32_t val, int32_t num);

    void   skipBits(int32_t num)
    {
        offset_ += num;
    }
    int32_t readBits1()
    {
        return readBits(1);
    }
    int32_t remainingBits()
    {
        return len_ - offset_;
    }
    int32_t length()
    {
        return len_;
    }
    bool   isError()
    {
        return error_;
    }
};

#endif // BITSTREAM_H
