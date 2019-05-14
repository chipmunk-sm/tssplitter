#ifndef __ts_mpegvideo_h__
#define __ts_mpegvideo_h__

#include "tsstream.h"

//////////////////////////////////////////////////////
class MPEG2Video : public TsStream
{
private:
    quint32 startCode_;
    bool    needIFrame_;
    bool    needSPS_;
    qint32  frameDuration_;
    qint32  vbvDelay_;       // -1 if CBR
    qint32  vbvSize_;        // Video buffer size (in bytes)
    qint32  width_;
    qint32  height_;
    float   dar_;
    qint64  DTS_, PTS_;
    qint64  auDTS_, auPTS_, auPrevDTS_;
    qint32  temporalReference_;
    qint32  trLastTime_;
    qint32  picNumber_;

    qint32 parse_MPEG2Video(quint32 startcode, qint32 bufPtr, bool& complete);
    bool parse_MPEG2Video_SeqStart(quint8* buf);
    bool parse_MPEG2Video_PicStart(quint8* buf);

  public:
    MPEG2Video(quint16 pid);
    virtual ~MPEG2Video();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};

#endif // __ts_mpegvideo_h__
