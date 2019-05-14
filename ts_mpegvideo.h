#ifndef TS_MPEGVIDEO_H
#define TS_MPEGVIDEO_H

#include "tsstream.h"

//////////////////////////////////////////////////////
class MPEG2Video : public TsStream
{
private:
    uint32_t startCode_;
    bool    needIFrame_;
    bool    needSPS_;
    int32_t  frameDuration_;
    int32_t  vbvDelay_;       // -1 if CBR
    int32_t  vbvSize_;        // Video buffer size (in bytes)
    int32_t  width_;
    int32_t  height_;
    float   dar_;
    int64_t  DTS_, PTS_;
    int64_t  auDTS_, auPTS_, auPrevDTS_;
    int32_t  temporalReference_;
    int32_t  trLastTime_;
    int32_t  picNumber_;

    int32_t parse_MPEG2Video(uint32_t startcode, int32_t bufPtr, bool& complete);
    bool parse_MPEG2Video_SeqStart(uint8_t* buf);
    bool parse_MPEG2Video_PicStart(uint8_t* buf);

public:
    MPEG2Video(uint16_t pid);
    virtual ~MPEG2Video();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};

#endif // TS_MPEGVIDEO_H
