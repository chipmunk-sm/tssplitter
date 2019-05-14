#include "ts_mpegvideo.h"
#include "bitstream.h"

#define MPEG_PICTURE_START      0x00000100
#define MPEG_SEQUENCE_START     0x000001b3
#define MPEG_SEQUENCE_EXTENSION 0x000001b5
#define MPEG_SLICE_S            0x00000101
#define MPEG_SLICE_E            0x000001af

#define PKT_I_FRAME 1
#define PKT_P_FRAME 2
#define PKT_B_FRAME 3
#define PKT_NTYPES  4

// MPEG2VIDEO frame duration table (in 90kHz clock domain)
const qint32 mpeg2video_framedurations[16] = {
  0,
  3753,
  3750,
  3600,
  3003,
  3000,
  1800,
  1501,
  1500,
};

MPEG2Video::MPEG2Video(quint16 pid)
 : TsStream(pid)
{
  frameDuration_     = 0;
  vbvDelay_          = -1;
  vbvSize_           = 0;
  height_            = 0;
  width_             = 0;
  dar_               = 0.0f;
  DTS_               = 0;
  PTS_               = 0;
  auDTS_             = 0;
  auPTS_             = 0;
  auPrevDTS_         = 0;
  temporalReference_ = 0;
  trLastTime_        = 0;
  picNumber_         = 0;
  esAllocInit_       = 80000;
  reset();
}

MPEG2Video::~MPEG2Video()
{}

void MPEG2Video::parse(STREAM_PKG* pkg)
{
    qint32  framePtr = esConsumed_, p = esParsed_, c;
    quint32 startcode = startCode_;
    bool   frameComplete = false;
    while( (c = esLen_-p) > 3 ) {
        if( (startcode & 0xffffff00) == 0x00000100 )
            if( parse_MPEG2Video(startcode, p, frameComplete) < 0 )
                break;
        startcode = startcode << 8 | esBuf_[p++];
    }
    esParsed_ = p;
    startCode_ = startcode;

    if( frameComplete )
    {
        if( !needSPS_ && !needIFrame_ )
        {
            qint32 fpsScale = static_cast<qint32>(rescale(frameDuration_, RESCALE_TIME_BASE, PTS_TIME_BASE));
            bool streamChange = setVideoInformation(fpsScale, RESCALE_TIME_BASE, height_, width_, dar_, false);
            pkg->pid          = pid_;
            pkg->size         = esConsumed_ - framePtr;
            pkg->data         = &esBuf_[framePtr];
            pkg->dts          = DTS_;
            pkg->pts          = PTS_;
            pkg->duration     = frameDuration_;
            pkg->streamChange = streamChange;
        }
        startCode_ = 0xffffffff;
        esParsed_ = esConsumed_;
        esFoundFrame_ = false;
    }
}

void MPEG2Video::reset()
{
    TsStream::reset();
    startCode_  = 0xffffffff;
    needIFrame_ = true;
    needSPS_    = true;
}

qint32 MPEG2Video::parse_MPEG2Video(quint32 startcode, qint32 bufPtr, bool& complete)
{
    qint32 len = esLen_ - bufPtr;
    quint8* buf = esBuf_ + bufPtr;

    switch(startcode & 0xFF) {
    case 0: // picture start
        if( needSPS_ ) {
            esFoundFrame_ = true;
            return 0;
        }

        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }

        if( len < 4 )
            return -1;

        if( !parse_MPEG2Video_PicStart(buf) )
            return 0;

        if( !esFoundFrame_ )
        {
            auPrevDTS_ = auDTS_;
            if( bufPtr - 4 >= (qint32)esPtsPointer_)
            {
                auDTS_ = curDts_;
                auPTS_ = curPts_;
            }
            else
            {
                auDTS_ = prevDts_;
                auPTS_ = prevPts_;
            }
        }

        if( auPrevDTS_ == auDTS_ )
        {
            DTS_ = auDTS_ + picNumber_ * frameDuration_;
            PTS_ = auPTS_ + (temporalReference_ - trLastTime_) * frameDuration_;
        }
        else
        {
            PTS_ = auPTS_;
            DTS_ = auDTS_;
            picNumber_ = 0;
            trLastTime_ = temporalReference_;
        }

        picNumber_++;
        esFoundFrame_ = true;
        break;

    case 0xb3: // Sequence start code
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr - 4;
            return -1;
        }

        if( len < 8 )
            return -1;
        if( !parse_MPEG2Video_SeqStart(buf) )
            return 0;
        break;

    case 0xb7: // sequence end
        if( esFoundFrame_ ) {
            complete = true;
            esConsumed_ = bufPtr;
            return -1;
        }
        break;
  }
  return 0;
}

bool MPEG2Video::parse_MPEG2Video_SeqStart(quint8* buf)
{
    BitStream bs(buf, 8 * 8);

    width_  = bs.readBits(12);
    height_ = bs.readBits(12);

    // figure out Display Aspect Ratio
    quint8 aspect = bs.readBits(4);

    switch(aspect)
    {
    case 1:
        dar_ = 1.0f;
        break;
    case 2:
        dar_ = 4.0f/3.0f;
        break;
    case 3:
        dar_ = 16.0f/9.0f;
        break;
    case 4:
        dar_ = 2.21f;
        break;
    default:
        return false;
    }

    frameDuration_ = mpeg2video_framedurations[bs.readBits(4)];
    bs.skipBits(18);
    bs.skipBits(1);

    vbvSize_ = bs.readBits(10) * 16 * 1024 / 8;
    needSPS_ = false;
    return true;
}

bool MPEG2Video::parse_MPEG2Video_PicStart(quint8* buf)
{
    BitStream bs(buf, 4 * 8);
    temporalReference_ = bs.readBits(10); // temporal reference

    qint32 pct = bs.readBits(3);
    if( pct < PKT_I_FRAME || pct > PKT_B_FRAME )
        return true;  // Illegal picture_coding_type

    if( pct == PKT_I_FRAME )
        needIFrame_ = false;

    quint32 vbvDelay = bs.readBits(16); // vbv_delay
    vbvDelay_ = (vbvDelay == 0xffff ? -1 : vbvDelay);
    return true;
}
