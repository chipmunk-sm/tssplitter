#ifndef __ts_mpegaudio_h__
#define __ts_mpegaudio_h__

#include "tsstream.h"

///////////////////////////////////////////////////////////
class MPEG2Audio : public TsStream
{
private:
    qint32  sampleRate_;
    qint32  channels_;
    qint32  bitRate_;
    qint32  frameSize_;
    qint64  PTS_, DTS_;

    qint32 findHeaders(quint8* buf, qint32 bufSize);

public:
    MPEG2Audio(quint16 pid);
    virtual ~MPEG2Audio();

    virtual void parse(STREAM_PKG* pkg);
};

#endif // __ts_mpegaudio_h__
