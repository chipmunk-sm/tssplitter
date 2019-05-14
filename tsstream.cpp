#include "tsstream.h"

TsStream::TsStream(quint16 pes_pid)
  : pid_(pes_pid), 
  streamType_(STREAM_TYPE_UNKNOWN), 
  curDts_(PTS_UNSET), 
  curPts_(PTS_UNSET), 
  prevDts_(PTS_UNSET), 
  prevPts_(PTS_UNSET), 
  hasStreamInfo_(false), 
  esAllocInit_(ES_INIT_BUFFER_SIZE), 
  esBuf_(NULL), 
  esAlloc_(0), 
  esLen_(0), 
  esConsumed_(0), 
  esPtsPointer_(0), 
  esParsed_(0), 
  esFoundFrame_(false)
{
  memset(&streamInfo_, 0, sizeof(STREAM_INFO));
}

TsStream::~TsStream()
{
    if (esBuf_) {
        free(esBuf_);
        esBuf_ = NULL;
    }
}

void TsStream::reset()
{
  clearBuffer();
  esFoundFrame_ = false;
}

void TsStream::clearBuffer()
{
  esLen_ = esConsumed_ = esPtsPointer_ = esParsed_ = 0;
}

int TsStream::append(const quint8* buf, qint32 len, bool newPts)
{
    // mark position where current pts become applicable
    if( newPts )
        esPtsPointer_ = esLen_;

    if( esBuf_ && esConsumed_ )
    {
        if( esConsumed_ < esLen_ )
        {
            memmove(esBuf_, esBuf_+esConsumed_, esLen_-esConsumed_);
            esLen_ -= esConsumed_;
            esParsed_ -= esConsumed_;
            if (esPtsPointer_ > esConsumed_)
                esPtsPointer_ -= esConsumed_;
            else
                esPtsPointer_ = 0;
            esConsumed_ = 0;
        }
        else
            clearBuffer();
    }

    if( esLen_ + len > esAlloc_)
    {
        if( esAlloc_ >= ES_MAX_BUFFER_SIZE )
            return -ENOMEM;

        qint32 n = (esAlloc_ ? (esAlloc_ + len)*2 : esAllocInit_);
        if( n > ES_MAX_BUFFER_SIZE )
            n = ES_MAX_BUFFER_SIZE;

        // realloc buffer size to n for stream with pid
        quint8* storeptr = esBuf_;
        if( NULL == (esBuf_ = (quint8*)realloc(esBuf_, n*sizeof(*esBuf_))) ) {
            free(storeptr);
            esAlloc_ = esLen_ = 0;
            return -ENOMEM;
        }
        esAlloc_ = n;
    }

    if( esBuf_ == NULL )
        return -ENOMEM;

    memcpy(esBuf_ + esLen_, buf, len);
    esLen_ += len;
    return 0;
}

QString TsStream::getStreamCodecName(STREAM_TYPE streamType)
{
    switch (streamType)
    {
    case STREAM_TYPE_VIDEO_MPEG1:
        return "mpeg1video";
    case STREAM_TYPE_VIDEO_MPEG2:
        return "mpeg2video";
    case STREAM_TYPE_AUDIO_MPEG1:
        return "mp1";
    case STREAM_TYPE_AUDIO_MPEG2:
        return "mp2";
    case STREAM_TYPE_DVB_TELETEXT:
        return "teletext";
    case STREAM_TYPE_DVB_SUBTITLE:
        return "dvbsub";
    case STREAM_TYPE_AUDIO_AAC:
        return "aac";
    case STREAM_TYPE_AUDIO_AAC_ADTS:
        return "aac";
    case STREAM_TYPE_AUDIO_AAC_LATM:
        return "aac_latm";
    case STREAM_TYPE_VIDEO_H264:
        return "h264";
    case STREAM_TYPE_AUDIO_AC3:
        return "ac3";
    case STREAM_TYPE_AUDIO_EAC3:
        return "eac3";
    case STREAM_TYPE_VIDEO_MPEG4:
        return "mpeg4video";
    case STREAM_TYPE_VIDEO_VC1:
        return "vc1";
    case STREAM_TYPE_AUDIO_LPCM:
        return "lpcm";
    case STREAM_TYPE_AUDIO_DTS:
        return "dts";
    case STREAM_TYPE_PRIVATE_DATA:
        break;
    }
    return "data";
}

QString TsStream::getFileExtension(STREAM_TYPE streamType)
{
    switch (streamType)
    {
    case STREAM_TYPE_VIDEO_MPEG1:
        return ".mpg";
    case STREAM_TYPE_VIDEO_MPEG2:
        return ".mpeg";
    case STREAM_TYPE_AUDIO_MPEG1:
        return ".mpa";
    case STREAM_TYPE_AUDIO_MPEG2:
        return ".mpa";
    case STREAM_TYPE_DVB_SUBTITLE:
        return ".srt";
    case STREAM_TYPE_AUDIO_AAC:
        return ".aac";
    case STREAM_TYPE_AUDIO_AAC_ADTS:
        return ".m4p";
    case STREAM_TYPE_AUDIO_AAC_LATM:
        return ".m4a";
    case STREAM_TYPE_VIDEO_H264:
        return ".h264";
    case STREAM_TYPE_AUDIO_AC3:
        return ".ac3";
    case STREAM_TYPE_AUDIO_EAC3:
        return ".eac3";
    case STREAM_TYPE_VIDEO_MPEG4:
        return ".mp4";
    case STREAM_TYPE_VIDEO_VC1:
        return ".vc1";
    case STREAM_TYPE_AUDIO_LPCM:
        return ".lpcm";
    case STREAM_TYPE_AUDIO_DTS:
        return ".dts";
    }
    return "";
}

void TsStream::parse(STREAM_PKG* pkg)
{
    // no parser: pass-through
    if( esConsumed_ < esLen_ )
    {
        esConsumed_ = esParsed_ = esLen_;
        pkg->pid              = pid_;
        pkg->size             = esConsumed_;
        pkg->data             = esBuf_;
        pkg->dts              = curDts_;
        pkg->pts              = curPts_;
        pkg->streamChange     = false;
        pkg->duration = (curDts_==PTS_UNSET || prevDts_==PTS_UNSET ? 0 : curDts_-prevDts_);
    }
}

void TsStream::resetStreamPackage(STREAM_PKG* pkg)
{
    pkg->pid            = 0xffff;
    pkg->size           = 0;
    pkg->data           = NULL;
    pkg->dts            = PTS_UNSET;
    pkg->pts            = PTS_UNSET;
    pkg->duration       = 0;
    pkg->streamChange   = false;
}

qint64 TsStream::rescale(const qint64& a, const qint64& b, const qint64& c)
{
    quint64 r = c/2;
    quint64 res = (a <= INT_MAX ? ((a*b+r)/c) : (a/c*b + ((a%c)*b + r)/c));

    if( b > INT_MAX || c > INT_MAX )
    {
        quint64 a0 = a & 0xFFFFFFFF;
        quint64 a1 = a >> 32;
        quint64 b0 = b & 0xFFFFFFFF;
        quint64 b1 = b >> 32;

        res = a0 * b1 + a1 * b0;
        quint64 t1 = res << 32;

        a0 = a0 * b0 + t1;
        a1 = a1 * b1 + (res >> 32) + (a0 < t1);
        a0 += r;
        a1 += a0 < r;

        for(qint32 i = 63; i >= 0; i--) {
            a1 += a1 + ((a0 >> i) & 1);
            res += res;
            if( (quint64)c <= a1 ) {
                a1 -= c;
                res++;
            }
        }
    }
    return res;
}

bool TsStream::setVideoInformation(qint32 fpsScale, qint32 fpsRate, qint32 height, qint32 width, float aspect, bool interlaced)
{
    bool ret = false;
    if( (streamInfo_.fpsScale != fpsScale) ||
        (streamInfo_.fpsRate != fpsRate) ||
        (streamInfo_.height != height) ||
        (streamInfo_.width != width) ||
        (streamInfo_.aspect != aspect) ||
        (streamInfo_.interlaced != interlaced))
    {
        ret = true;
    }

    streamInfo_.fpsScale   = fpsScale;
    streamInfo_.fpsRate    = fpsRate;
    streamInfo_.height     = height;
    streamInfo_.width      = width;
    streamInfo_.aspect     = aspect;
    streamInfo_.interlaced = interlaced;

    hasStreamInfo_ = true;
    return ret;
}

bool TsStream::setAudioInformation(qint32 channels, qint32 sampleRate, qint32 bitRate, qint32 bitsPerSample, qint32 blockAlign)
{
    bool ret = false;
    if( (streamInfo_.channels != channels) ||
        (streamInfo_.sampleRate != sampleRate) ||
        (streamInfo_.blockAlign != blockAlign) ||
        (streamInfo_.bitRate != bitRate) ||
        (streamInfo_.bitsPerSample != bitsPerSample))
    {
        ret = true;
    }

    streamInfo_.channels       = channels;
    streamInfo_.sampleRate     = sampleRate;
    streamInfo_.blockAlign     = blockAlign;
    streamInfo_.bitRate        = bitRate;
    streamInfo_.bitsPerSample  = bitsPerSample;

    hasStreamInfo_ = true;
    return ret;
}
