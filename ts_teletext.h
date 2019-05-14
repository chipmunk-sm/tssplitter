#ifndef __ts_teletext_h__
#define __ts_teletext_h__

#include "tsstream.h"

class Teletext : public TsStream
{
public:
    Teletext(quint16 pid);
    virtual ~Teletext();
    virtual void parse(STREAM_PKG* pkg);
};

#endif // __ts_teletext_h__
