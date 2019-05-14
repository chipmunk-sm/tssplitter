#ifndef __tsstream_h
#define __tsstream_h

#include <QString>

#define ES_INIT_BUFFER_SIZE     64000
#define ES_MAX_BUFFER_SIZE      1048576
#define PTS_MASK                0x1ffffffffLL
#define PTS_UNSET               0x1ffffffffLL
#define PTS_TIME_BASE           90000LL
#define RESCALE_TIME_BASE       1000000LL

enum STREAM_TYPE
{
    STREAM_TYPE_UNKNOWN = 0,
    STREAM_TYPE_VIDEO_MPEG1,
    STREAM_TYPE_VIDEO_MPEG2,
    STREAM_TYPE_AUDIO_MPEG1,
    STREAM_TYPE_AUDIO_MPEG2,
    STREAM_TYPE_DVB_TELETEXT,
    STREAM_TYPE_DVB_SUBTITLE,
    STREAM_TYPE_AUDIO_AAC,
    STREAM_TYPE_AUDIO_AAC_ADTS,
    STREAM_TYPE_AUDIO_AAC_LATM,
    STREAM_TYPE_VIDEO_H264,
    STREAM_TYPE_AUDIO_AC3,
    STREAM_TYPE_AUDIO_EAC3,
    STREAM_TYPE_VIDEO_MPEG4,
    STREAM_TYPE_VIDEO_VC1,
    STREAM_TYPE_AUDIO_LPCM,
    STREAM_TYPE_AUDIO_DTS,
    STREAM_TYPE_PRIVATE_DATA
};

struct STREAM_INFO
{
    quint16 channel;
    quint16 pid;
    char    codecName[12];
    char    language[4];
    qint32  compositionId;
    qint32  ancillaryId;
    qint32  fpsScale;
    qint32  fpsRate;
    qint32  height;
    qint32  width;
    float   aspect;
    qint32  channels;
    qint32  sampleRate;
    qint32  blockAlign;
    qint32  bitRate;
    qint32  bitsPerSample;
    bool    interlaced;
};

struct STREAM_PKG
{
    quint16         pid;
    qint32          size;
    const quint8*   data;
    qint64          dts;
    qint64          pts;
    qint64          duration;
    bool            streamChange;
};

/////////////////////////////////////////////////////////////////////
class TsStream
{
public:
    STREAM_TYPE streamType_;
    STREAM_INFO streamInfo_;
    bool hasStreamInfo_;

    quint16 pid_;
    qint64  curDts_;
    qint64  curPts_;
    qint64  prevDts_;
    qint64  prevPts_;

public:
    TsStream(quint16 pes_pid);
    virtual ~TsStream();

    virtual void reset();
    void clearBuffer();
    int append(const quint8* buf, qint32 len, bool newPts = false);
    virtual void parse(STREAM_PKG* pkg);
    static QString getStreamCodecName(STREAM_TYPE streamType);
    static QString getFileExtension(STREAM_TYPE streamType);

    inline QString getStreamCodec() const 
    { return getStreamCodecName(streamType_); }

    inline bool getStreamPackage(STREAM_PKG* pkg) {
        resetStreamPackage(pkg);
        parse(pkg);
        return (pkg->data != NULL);
    }

protected:
    void resetStreamPackage(STREAM_PKG* pkg);
    qint64 rescale(const qint64& a, const qint64& b, const qint64& c);
    bool setVideoInformation(qint32 fpsScale, qint32 fpsRate, qint32 height, qint32 width, float aspect, bool Interlaced);
    bool setAudioInformation(qint32 channels, qint32 sampleRate, qint32 bitRate, qint32 bitsPerSample, int blockAlign);

protected:
    qint32  esAllocInit_;   // initial allocation of memory for buffer
    quint8* esBuf_;         // pointer to buffer
    qint32  esAlloc_;       // allocated size of memory for buffer
    qint32  esLen_;         // size of data in buffer
    qint32  esConsumed_;    // consumed payload. Will be erased on next append
    qint32 esPtsPointer_;  // position in buffer where current PTS becomes applicable
    qint32  esParsed_;      // parser: last processed position in buffer
    bool    esFoundFrame_;  // parser: found frame

};

#endif // __tsstream_h
