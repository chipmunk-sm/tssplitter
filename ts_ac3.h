#ifndef TS_AC3_H
#define TS_AC3_H

#include "tsstream.h"

class AC3 : public TsStream
{
private:
    int32_t sampleRate_;
    int32_t channels_;
    int32_t bitRate_;
    int32_t frameSize_;

    int64_t PTS_;     // pts of the current frame
    int64_t DTS_;     // dts of the current frame

    int32_t findHeaders(uint8_t* buf, int32_t bufSize);

public:
    AC3(uint16_t pid);
    virtual ~AC3();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // TS_AC3_H
