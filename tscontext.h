#ifndef TSCONTEXT_H
#define TSCONTEXT_H

#include "tspackage.h"
#include "tsparser.h"

#include <QMutex>
#include <QMap>
#include <QVector>

#define FLUTS_NORMAL_TS_PACKAGESIZE     188
#define FLUTS_M2TS_TS_PACKAGESIZE       192
#define FLUTS_DVB_ASI_TS_PACKAGESIZE    204
#define FLUTS_ATSC_TS_PACKAGESIZE       208

#define AV_CONTEXT_PACKAGESIZE          208
#define TS_CHECK_MIN_SCORE              2
#define TS_CHECK_MAX_SCORE              10

///////////////////////////////////////////////////////////
enum
{
    AVCONTEXT_IO_ERROR_3 = -6,
    AVCONTEXT_IO_ERROR_2 = -5,
    AVCONTEXT_IO_ERROR_1 = -4,

    AVCONTEXT_TS_ERROR = -3,

    AVCONTEXT_TS_NOSYNC = -1,
    AVCONTEXT_CONTINUE = 0,
    AVCONTEXT_PROGRAM_CHANGE = 1,
    AVCONTEXT_STREAM_PID_DATA = 2,
    AVCONTEXT_DISCONTINUITY = 3,
    AVCONTEXT_EOF_1 = 4,
    AVCONTEXT_EOF_2 = 5,
    AVCONTEXT_EOF_3 = 6
};

///////////////////////////////////////////////////////////
class AVContext
{
public:
    AVContext(TsParser& parser, const int64_t& pos, uint16_t channel);
    ~AVContext();
    void reset();

    QVector<TsStream*> getStreams() const;
    void startStreaming(uint16_t pid);
    void stopStreaming(uint16_t pid);

    // TS parser
    int32_t TSResync();
    int32_t processTSPackage();
    int32_t processTSPayload();

    inline uint16_t getPID() const;
    inline PACKAGE_TYPE getPIDType() const;
    inline uint16_t getPIDChannel() const;
    inline bool hasPIDStreamData() const;
    inline bool hasPIDPayload() const;
    inline TsStream* getPIDStream() const;
    inline TsStream* getStream(uint16_t pid) const;
    inline uint16_t getChannel(uint16_t pid) const;
    inline void resetPackages();

    inline int64_t goNext();
    inline int64_t shift();
    inline void goPosition(const int64_t& pos);
    inline int64_t getPosition() const;

private:
    AVContext(const AVContext&);
    AVContext& operator=(const AVContext&);

    inline uint8_t  avRb8(const uint8_t* p) const;
    inline uint16_t avRb16(const uint8_t* p) const;
    inline uint32_t avRb32(const uint8_t* p) const;
    inline int64_t  decodePts(const uint8_t* p) const;

    int32_t configureTs();
    static STREAM_TYPE getStreamType(uint8_t pesType);

    STREAM_INFO parsePesDescriptor(const uint8_t* p, int32_t len, STREAM_TYPE* st);
    void    clearPmt();
    void    clearPes(uint16_t channel);
    int32_t  parseTsPsi();
    int32_t  parseTsPes();

private:
    // critical section
    QMutex* csMutex_;

    // AV stream owner
    TsParser& parser_;

    // Raw package buffer
    int64_t avPos_;
    int32_t avDataLen_;
    int32_t avPkgSize_;
    uint8_t avBuf_[AV_CONTEXT_PACKAGESIZE];

    // TS Streams context
    bool isConfigured_;
    uint16_t channel_;
    QMap<uint16_t, TsPackage> packages_;

    // Package context
    uint16_t         pid_;
    bool            transportError_;
    bool            hasPayload_;
    bool            payloadUnitStart_;
    bool            discontinuity_;
    const uint8_t*   payload_;
    int32_t          payloadLen_;
    TsPackage*      package_;
};

inline uint16_t AVContext::getPID() const
{
    return pid_;
}

inline PACKAGE_TYPE AVContext::getPIDType() const
{
    QMutexLocker lock(csMutex_);
    return (package_ == nullptr ? PACKAGE_TYPE_UNKNOWN : package_->packageType);
}

inline uint16_t AVContext::getPIDChannel() const
{
    QMutexLocker lock(csMutex_);
    return (package_ == nullptr ? 0xffff : package_->channel);
}

// PES packets append frame buffer of elementary stream until next start of unit
// On new unit start, flag is held
inline bool AVContext::hasPIDStreamData() const
{
    QMutexLocker lock(csMutex_);
    return (package_ != nullptr && package_->hasStreamData);
}

inline bool AVContext::hasPIDPayload() const
{
    return hasPayload_;
}

inline TsStream* AVContext::getPIDStream() const
{
    QMutexLocker lock(csMutex_);
    return (package_ != nullptr && package_->packageType == PACKAGE_TYPE_PES ? package_->pStream : nullptr);
}

inline TsStream* AVContext::getStream(uint16_t pid) const
{
    QMutexLocker lock(csMutex_);
    QMap<uint16_t, TsPackage>::const_iterator It = packages_.find(pid);
    return (It == packages_.end() ? nullptr : It->pStream);
}

inline uint16_t AVContext::getChannel(uint16_t pid) const
{
    QMutexLocker lock(csMutex_);
    QMap<uint16_t, TsPackage>::const_iterator It = packages_.find(pid);
    return (It == packages_.end() ? 0xffff : It->channel);
}

inline void AVContext::resetPackages()
{
    QMutexLocker lock(csMutex_);
    QMap<uint16_t, TsPackage>::iterator It = packages_.begin();
    for (; It != packages_.end(); ++It) It->reset();
}

inline uint8_t AVContext::avRb8(const uint8_t* p) const
{
    return *p;
}

inline uint16_t AVContext::avRb16(const uint8_t* p) const
{
    uint16_t val = avRb8(p) << 8;
    return (val | avRb8(p + 1));
}

inline uint32_t AVContext::avRb32(const uint8_t* p) const
{
    uint32_t val = avRb16(p) << 16;
    return (val | avRb16(p + 2));
}

inline int64_t AVContext::decodePts(const uint8_t* p) const
{
    int64_t pts = (quint64)(avRb8(p) & 0x0e) << 29 | (avRb16(p + 1) >> 1) << 15 | avRb16(p + 3) >> 1;
    return pts;
}

inline int64_t AVContext::goNext()
{
    avPos_ += avPkgSize_;
    reset();
    return avPos_;
}

inline int64_t AVContext::shift()
{
    avPos_++;
    reset();
    return avPos_;
}

inline void AVContext::goPosition(const int64_t& pos)
{
    avPos_ = pos;
    reset();
}

inline int64_t AVContext::getPosition() const
{
    return avPos_;
}

#endif // TSCONTEXT_H
