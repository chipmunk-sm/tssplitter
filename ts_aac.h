#ifndef __ts_aac_h__
#define __ts_aac_h__

#include "tsstream.h"
#include "bitstream.h"

class AAC : public TsStream
{
private:
    qint32 sampleRate_;
    qint32 channels_;
    qint32 bitRate_;
    qint32 frameSize_;

    qint64 PTS_;    // pts of the current frame
    qint64 DTS_;    // dts of the current frame

    bool   configured_;
    qint32 audioMuxVersion_A;
    qint32 frameLengthType_;

    qint32 findHeaders(quint8* buf, qint32 bufSize);
    bool parseLATMAudioMuxElement(BitStream* bs);
    void readStreamMuxConfig(BitStream* bs);
    void readAudioSpecificConfig(BitStream* bs);
    quint32 LATMGetValue(BitStream* bs) { return bs->readBits(bs->readBits(2)*8); }

public:
    AAC(quint16 pesPid);
    virtual ~AAC();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // __ts_aac_h__
