#ifndef __ts_subtitle_h__
#define __ts_subtitle_h__

#include "tsstream.h"

class Subtitle : public TsStream
{
public:
    Subtitle(quint16 pid);
    virtual ~Subtitle();
    virtual void parse(STREAM_PKG* pkg);
};

#endif // __ts_subtitle_h__
