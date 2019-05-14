#ifndef TSSTREAM_H
#define TSSTREAM_H

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
    uint16_t channel;
    uint16_t pid;
    char    codecName[12];
    char    language[4];
    int32_t  compositionId;
    int32_t  ancillaryId;
    int32_t  fpsScale;
    int32_t  fpsRate;
    int32_t  height;
    int32_t  width;
    double   aspect;
    int32_t  channels;
    int32_t  sampleRate;
    int32_t  blockAlign;
    int32_t  bitRate;
    int32_t  bitsPerSample;
    bool    interlaced;
};

struct STREAM_PKG
{
    uint16_t         pid;
    int32_t          size;
    const uint8_t*   data;
    int64_t          dts;
    int64_t          pts;
    int64_t          duration;
    bool            streamChange;
};

/////////////////////////////////////////////////////////////////////
class TsStream
{
public:
    STREAM_TYPE streamType_;
    STREAM_INFO streamInfo_;
    bool hasStreamInfo_;

    uint16_t pid_;
    int64_t  curDts_;
    int64_t  curPts_;
    int64_t  prevDts_;
    int64_t  prevPts_;

public:
    TsStream(uint16_t pes_pid);
    virtual ~TsStream();

    virtual void reset();
    void clearBuffer();
    int append(const uint8_t* buf, int32_t len, bool newPts = false);
    virtual void parse(STREAM_PKG* pkg);
    static QString getStreamCodecName(STREAM_TYPE streamType);
    static QString getFileExtension(STREAM_TYPE streamType);

    inline QString getStreamCodec() const
    {
        return getStreamCodecName(streamType_);
    }

    inline bool getStreamPackage(STREAM_PKG* pkg)
    {
        resetStreamPackage(pkg);
        parse(pkg);
        return (pkg->data != nullptr);
    }

protected:
    void resetStreamPackage(STREAM_PKG* pkg);
    int64_t rescale(const int64_t& a, const int64_t& b, const int64_t& c);
    bool setVideoInformation(int32_t fpsScale, int32_t fpsRate, int32_t height, int32_t width, float aspect, bool Interlaced);
    bool setAudioInformation(int32_t channels, int32_t sampleRate, int32_t bitRate, int32_t bitsPerSample, int blockAlign);

protected:
    int32_t  esAllocInit_;   // initial allocation of memory for buffer
    uint8_t* esBuf_;         // pointer to buffer
    int32_t  esAlloc_;       // allocated size of memory for buffer
    int32_t  esLen_;         // size of data in buffer
    int32_t  esConsumed_;    // consumed payload. Will be erased on next append
    int32_t esPtsPointer_;  // position in buffer where current PTS becomes applicable
    int32_t  esParsed_;      // parser: last processed position in buffer
    bool    esFoundFrame_;  // parser: found frame

};

#endif // TSSTREAM_H
