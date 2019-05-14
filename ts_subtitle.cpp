#include "ts_subtitle.h"

Subtitle::Subtitle(quint16 pid)
    : TsStream(pid)
{
  esAllocInit_ = 4000;
  hasStreamInfo_ = true; // doesn't provide stream info
}

Subtitle::~Subtitle()
{}

void Subtitle::parse(STREAM_PKG* pkg)
{
    qint32 c = esLen_ - esParsed_;

    if( c > 0 )
    {
        if( c < 2 || esBuf_[0] != 0x20 || esBuf_[1] != 0x00 ) {
            reset();
            return;
        }

        if( esBuf_[c-1] == 0xff )
        {
            pkg->pid          = pid_;
            pkg->data         = esBuf_+2;
            pkg->size         = c-3;
            pkg->duration     = 0;
            pkg->dts          = curDts_;
            pkg->pts          = curPts_;
            pkg->streamChange = false;
        }
        esParsed_ = esConsumed_ = esLen_;
    }
}
