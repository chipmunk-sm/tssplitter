#ifndef TS_MPEGAUDIO_H
#define TS_MPEGAUDIO_H

#include "tsstream.h"

///////////////////////////////////////////////////////////
class MPEG2Audio : public TsStream
{
private:
    int32_t  sampleRate_;
    int32_t  channels_;
    int32_t  bitRate_;
    int32_t  frameSize_;
    int64_t  PTS_, DTS_;

    int32_t findHeaders(uint8_t* buf, int32_t bufSize);

public:
    MPEG2Audio(uint16_t pid);
    virtual ~MPEG2Audio();

    virtual void parse(STREAM_PKG* pkg);
};

#endif // TS_MPEGAUDIO_H
