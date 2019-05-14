#include "ts_teletext.h"

Teletext::Teletext(quint16 pid)
    : TsStream(pid)
{
  esAllocInit_ = 4000;
  hasStreamInfo_ = true; // doesn't provide stream info
}

Teletext::~Teletext()
{}

void Teletext::parse(STREAM_PKG* pkg)
{
    qint32 c = esLen_ - esParsed_;
    if( c < 1 )
        return;

    if( esBuf_[0] < 0x10 || esBuf_[0] > 0x1F ) {
        reset();
        return;
    }

    pkg->pid          = pid_;
    pkg->data         = esBuf_;
    pkg->size         = c;
    pkg->duration     = 0;
    pkg->dts          = curDts_;
    pkg->pts          = curPts_;
    pkg->streamChange = false;

    esParsed_ = esConsumed_ = esLen_;
}
