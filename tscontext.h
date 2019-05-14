#ifndef __tscontext_h__
#define __tscontext_h__

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
enum {
    AVCONTEXT_TS_ERROR          = -3,
    AVCONTEXT_IO_ERROR          = -2,
    AVCONTEXT_TS_NOSYNC         = -1,
    AVCONTEXT_CONTINUE          = 0,
    AVCONTEXT_PROGRAM_CHANGE    = 1,
    AVCONTEXT_STREAM_PID_DATA   = 2,
    AVCONTEXT_DISCONTINUITY     = 3
};

///////////////////////////////////////////////////////////
class AVContext
{
public:
    AVContext(TsParser& parser, const qint64& pos, quint16 channel);
    ~AVContext();
    void reset();

    QVector<TsStream*> getStreams() const;
    void startStreaming(quint16 pid);
    void stopStreaming(quint16 pid);

    // TS parser
    qint32 TSResync();
    qint32 processTSPackage();
    qint32 processTSPayload();

    inline quint16 getPID() const;
    inline PACKAGE_TYPE getPIDType() const;
    inline quint16 getPIDChannel() const;
    inline bool hasPIDStreamData() const;
    inline bool hasPIDPayload() const;
    inline TsStream* getPIDStream() const;
    inline TsStream* getStream(quint16 pid) const;
    inline quint16 getChannel(quint16 pid) const;
    inline void resetPackages();

    inline qint64 goNext();
    inline qint64 shift();
    inline void goPosition(const qint64& pos);
    inline qint64 getPosition() const;

private:
    AVContext(const AVContext&);
    AVContext& operator=(const AVContext&);

    inline quint8  avRb8(const quint8* p) const;
    inline quint16 avRb16(const quint8* p) const;
    inline quint32 avRb32(const quint8* p) const;
    inline qint64  decodePts(const quint8* p) const;

    qint32 configureTs();
    static STREAM_TYPE getStreamType(quint8 pesType);

    STREAM_INFO parsePesDescriptor(const quint8* p, qint32 len, STREAM_TYPE* st);
    void    clearPmt();
    void    clearPes(quint16 channel);
    qint32  parseTsPsi();
    qint32  parseTsPes();

private:
    // critical section
    QMutex* csMutex_;

    // AV stream owner
    TsParser& parser_;

    // Raw package buffer
    qint64 avPos_;
    qint32 avDataLen_;
    qint32 avPkgSize_;
    quint8 avBuf_[AV_CONTEXT_PACKAGESIZE];

    // TS Streams context
    bool isConfigured_;
    quint16 channel_;
    QMap<quint16,TsPackage> packages_;

    // Package context
    quint16         pid_;
    bool            transportError_;
    bool            hasPayload_;
    bool            payloadUnitStart_;
    bool            discontinuity_;
    const quint8*   payload_;
    qint32          payloadLen_;
    TsPackage*      package_;
};

inline quint16 AVContext::getPID() const
{ return pid_; }

inline PACKAGE_TYPE AVContext::getPIDType() const {
    QMutexLocker lock(csMutex_);
    return (package_ == NULL ? PACKAGE_TYPE_UNKNOWN : package_->packageType);
}

inline quint16 AVContext::getPIDChannel() const {
    QMutexLocker lock(csMutex_);
    return (package_ == NULL ? 0xffff : package_->channel);
}

// PES packets append frame buffer of elementary stream until next start of unit
// On new unit start, flag is held
inline bool AVContext::hasPIDStreamData() const {
    QMutexLocker lock(csMutex_);
    return (package_ != NULL && package_->hasStreamData);
}

inline bool AVContext::hasPIDPayload() const
{ return hasPayload_; }

inline TsStream* AVContext::getPIDStream() const {
    QMutexLocker lock(csMutex_);
    return (package_ != NULL && package_->packageType == PACKAGE_TYPE_PES ? package_->pStream : NULL);
}

inline TsStream* AVContext::getStream(quint16 pid) const {
    QMutexLocker lock(csMutex_);
    QMap<quint16,TsPackage>::const_iterator It = packages_.find(pid);
    return (It == packages_.end() ? NULL : It->pStream);
}

inline quint16 AVContext::getChannel(quint16 pid) const {
    QMutexLocker lock(csMutex_);
    QMap<quint16,TsPackage>::const_iterator It = packages_.find(pid);
    return (It == packages_.end() ? 0xffff : It->channel);
}

inline void AVContext::resetPackages() {
    QMutexLocker lock(csMutex_);
    QMap<quint16,TsPackage>::iterator It = packages_.begin();
    for(; It != packages_.end(); ++It) It->reset();
}

inline quint8 AVContext::avRb8(const quint8* p) const 
{ return *p; }

inline quint16 AVContext::avRb16(const quint8* p) const {
    quint16 val = avRb8(p) << 8;
    return (val|avRb8(p+1));
}

inline quint32 AVContext::avRb32(const quint8* p) const {
  quint32 val = avRb16(p) << 16;
  return (val|avRb16(p+2));
}

inline qint64 AVContext::decodePts(const quint8* p) const {
  qint64 pts = (quint64)(avRb8(p) & 0x0e) << 29 | (avRb16(p+1) >> 1) << 15 | avRb16(p+3) >> 1;
  return pts;
}

inline qint64 AVContext::goNext() {
    avPos_ += avPkgSize_;
    reset();
    return avPos_;
}

inline qint64 AVContext::shift() {
    avPos_++;
    reset();
    return avPos_;
}

inline void AVContext::goPosition(const qint64& pos) {
    avPos_ = pos;
    reset();
}

inline qint64 AVContext::getPosition() const
{ return avPos_; }

#endif // __tscontext_h__
