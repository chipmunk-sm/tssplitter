#ifndef TS_TELETEXT_H
#define TS_TELETEXT_H

#include "tsstream.h"

class Teletext : public TsStream
{
public:
    Teletext(uint16_t pid);
    virtual ~Teletext();
    virtual void parse(STREAM_PKG* pkg);
};

#endif // TS_TELETEXT_H
