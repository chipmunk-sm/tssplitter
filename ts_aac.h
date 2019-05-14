#ifndef TS_AAC_H
#define TS_AAC_H

#include "tsstream.h"
#include "bitstream.h"

class AAC : public TsStream
{
private:
    int32_t sampleRate_;
    int32_t channels_;
    int32_t bitRate_;
    int32_t frameSize_;

    int64_t PTS_;    // pts of the current frame
    int64_t DTS_;    // dts of the current frame

    bool   configured_;
    int32_t audioMuxVersion_A;
    int32_t frameLengthType_;

    int32_t findHeaders(uint8_t* buf, int32_t bufSize);
    bool parseLATMAudioMuxElement(BitStream* bs);
    void readStreamMuxConfig(BitStream* bs);
    void readAudioSpecificConfig(BitStream* bs);
    uint32_t LATMGetValue(BitStream* bs)
    {
        return bs->readBits(bs->readBits(2) * 8);
    }

public:
    AAC(uint16_t pesPid);
    virtual ~AAC();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // TS_AAC_H
