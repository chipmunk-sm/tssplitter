#ifndef __tsparser_h__
#define __tsparser_h__

#include "tsstream.h"

#include <QThread>
#include <QMap>
#include <QFile>

#define AV_BUFFER_SIZE       (131072)
#define POSMAP_PTS_INTERVAL  (270000LL)

///////////////////////////////////////////////////////////
class AVContext;

class TsParser : public QThread
{
    Q_OBJECT

public:
    TsParser(const QString& filePath, QObject* parent);
    ~TsParser();

    bool start();
    const quint8* read(const qint64& pos, qint32 n);

Q_SIGNALS:
    void streamFound(const STREAM_INFO& streamInfo);
    void notifyError(const QString& info);
    void notifyStart(qint32 threadId, const TsParser& self);
    void notifyDone(qint32 percent, qint32 threadId);

protected:
    qint32 process();
    void run();

private:
    bool getStreamData(STREAM_PKG* pkg);
    void resetPosmap();
    void registerPmt();
    void writeStreamData(STREAM_PKG* pkg);
    void showStreamInfo(quint16 pid);

private:
    quint8 channels_;

    typedef QMap<quint8,FILE*> FilesT;
    FilesT outfiles_;

    // AV raw buffer
    qint32  avBufSize_;         // size of av buffer
    qint64  avPos_;             // absolute position in av
    quint8* avBuf_;             // buffer
    quint8* avRbs_;             // raw data start in buffer
    quint8* avRbe_;             // raw data end in buffer

    // playback context
    QScopedPointer<AVContext> AVContext_;

    quint16 mainStreamPID_;     // PID of main stream
    qint64  absDTS_;            // absolute decode time of main stream
    qint64  absPTS_;            // absolute presentation time of main stream
    qint64  pinTime_;           // pinned relative position (90Khz)
    qint64  curTime_;           // current relative position (90Khz)
    qint64  endTime_;           // last relative marked position (90Khz))

    struct AV_POSMAP_ITEM {
        qint64 avPts;
        qint64 avPos;
    };
    QMap<qint64,AV_POSMAP_ITEM> posMap_;

    qint32 done_;
    qint64 fileSize_;
    QFile  tsfile_;
};

#endif // __tsparser_h__
