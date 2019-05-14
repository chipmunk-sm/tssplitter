#ifndef TS_SUBTITLE_H
#define TS_SUBTITLE_H

#include "tsstream.h"

class Subtitle : public TsStream
{
public:
    Subtitle(uint16_t pid);
    virtual ~Subtitle();
    virtual void parse(STREAM_PKG* pkg);
};

#endif // TS_SUBTITLE_H
