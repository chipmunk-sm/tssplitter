#ifndef __bitstream_h__
#define __bitstream_h__

#include <QString>

////////////////////////////////////////////////////////
class BitStream
{
private:
    quint8* data_;
    qint32  offset_;
    qint32  len_;
    bool    error_;

public:
    BitStream(quint8* data, qint32 bits);

    void   setBitStream(quint8* data, qint32 bits);
    quint32 readBits(qint32 num);
    qint32 showBits(qint32 num);
    qint32 readGolombUE(qint32 maxbits = 32);
    qint32 readGolombSE();
    void   putBits(qint32 val, qint32 num);

    void   skipBits(qint32 num) { offset_ += num; }
    qint32 readBits1() { return readBits(1); }
    qint32 remainingBits() { return len_ - offset_; }
    qint32 length() { return len_; }
    bool   isError() { return error_; }
};

#endif // __bitstream_h__
