#ifndef TSPARSER_H
#define TSPARSER_H

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
    const uint8_t* read(const int64_t& position, int32_t sizeToRead, bool &bEof);
    inline const QString getSourceName()
    {
        return m_file.fileName();
    }

Q_SIGNALS:
    void streamFound(const STREAM_INFO& streamInfo, TsParser* self);
    void notifyError(const QString& info);
    void notifyStart(Qt::HANDLE threadId, TsParser* self);
    void notifyDone(int32_t percent, Qt::HANDLE threadId);

protected:
    int32_t process();
    void run();

private:
    bool getStreamData(STREAM_PKG* pkg);
    void resetPosmap();
    void registerPmt();
    void writeStreamData(STREAM_PKG* pkg);
    void showStreamInfo(uint16_t pid);

private:
    uint8_t channels_;

    std::map<uint16_t, QFile> outfiles_;

    // AV raw buffer
    std::vector<uint8_t> m_buffer;// buffer
    int64_t  avPos_;             // absolute position in av
    uint8_t* avRbs_;             // raw data start in buffer
    uint8_t* avRbe_;             // raw data end in buffer

    // playback context
    QScopedPointer<AVContext> AVContext_;

    uint16_t mainStreamPID_;     // PID of main stream
    int64_t  absDTS_;            // absolute decode time of main stream
    int64_t  absPTS_;            // absolute presentation time of main stream
    int64_t  pinTime_;           // pinned relative position (90Khz)
    int64_t  curTime_;           // current relative position (90Khz)
    int64_t  endTime_;           // last relative marked position (90Khz))

    struct AV_POSMAP_ITEM
    {
        int64_t avPts;
        int64_t avPos;
    };

    QMap<int64_t, AV_POSMAP_ITEM> m_positionMap;

    int32_t m_progress = 0;
    QFile   m_file;
};

#endif // TSPARSER_H
