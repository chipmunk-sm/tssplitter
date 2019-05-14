#include "bitstream.h"

BitStream::BitStream(quint8* data, qint32 bits)
{
    data_   = data;
    offset_ = 0;
    len_    = bits;
    error_  = false;
}

void BitStream::setBitStream(quint8* data, qint32 bits)
{
    data_   = data;
    offset_ = 0;
    len_    = bits;
    error_  = false;
}

quint32 BitStream::readBits(qint32 num)
{
    quint32 r = 0;
    while( num > 0 )
    {
        if( offset_ >= len_ ) {
            error_ = true;
            return 0;
        }

        num--;
        if( data_[offset_/8] & (1 << (7 - (offset_&7))) )
            r |= 1 << num;

        offset_++;
    }
    return r;
}

qint32 BitStream::showBits(qint32 num)
{
    qint32 r = 0, offs = offset_;
    while( num > 0 ) 
    {
        if( offs >= len_ ) {
            error_ = true;
            return 0;
        }

        num--;
        if( data_[offs/8] & (1 << (7 - (offs&7))) )
            r |= 1 << num;

        offs++;
    }
    return r;
}

qint32 BitStream::readGolombUE(qint32 maxbits)
{
  qint32 lzb = -1, bits = 0;
  for( qint32 b = 0; !b; lzb++, bits++ )
  {
        if( bits > maxbits )
            return 0;
        b = readBits1();
  }
  return (1 << lzb) - 1 + readBits(lzb);
}

qint32 BitStream::readGolombSE()
{
    qint32 pos, v = readGolombUE();
    if( v == 0 )
        return 0;

    pos = (v & 1);
    v = (v + 1) >> 1;
    return pos ? v : -v;
}


void BitStream::putBits(qint32 val, qint32 num)
{
    while( num > 0 ) 
    {
        if( offset_ >= len_ ) {
            error_ = true;
            return;
        }

        num--;
        if( val & (1 << num) )
            data_[offset_/8] |= 1 << (7 - (offset_&7));
        else
            data_[offset_/8] &= ~(1 << (7 - (offset_&7)));
        offset_++;
    }
}
