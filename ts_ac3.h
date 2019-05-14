#ifndef __ts_ac3_h__
#define __ts_ac3_h__

#include "tsstream.h"

class AC3 : public TsStream
{
private:
    qint32 sampleRate_;
    qint32 channels_;
    qint32 bitRate_;
    qint32 frameSize_;

    qint64 PTS_;     // pts of the current frame
    qint64 DTS_;     // dts of the current frame

    qint32 findHeaders(quint8* buf, qint32 bufSize);

public:
    AC3(quint16 pid);
    virtual ~AC3();

    virtual void parse(STREAM_PKG* pkg);
    virtual void reset();
};


#endif // __ts_ac3_h__
