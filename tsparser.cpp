#include "tsparser.h"
#include "tscontext.h"

#include <QFileInfo>
#include <QDebug>

////////////////////////////////////////////////////////////////////
TsParser::TsParser(const QString& filePath, QObject* parent)
    : QThread(parent),
    mainStreamPID_(0xffff),
    absDTS_(PTS_UNSET),
    absPTS_(PTS_UNSET),
    pinTime_(0),
    curTime_(0),
    endTime_(0),
    m_file(filePath)
{
    m_buffer.resize((static_cast<size_t>(AV_BUFFER_SIZE) + 1));
    avPos_ = 0;
    avRbs_ = m_buffer.data();
    avRbe_ = m_buffer.data();
    AVContext_.reset(new AVContext(*this, 0, 0));
}

TsParser::~TsParser()
{
    exit();
    wait();
}

bool TsParser::start()
{
    if (m_file.open(QFile::ReadOnly))
    {
        QThread::start();
        return true;
    }
    emit notifyError(tr("Cannot open source file: ") + m_file.errorString());
    return false;
}

void TsParser::run()
{
    emit notifyStart(currentThreadId(), this);

    int32_t code = process();
    switch (code)
    {
    case AVCONTEXT_TS_ERROR:
        emit notifyError(tr("*** Error: TS ***"));
        break;
    case AVCONTEXT_IO_ERROR_1:
        emit notifyError(tr("*** Error: IO (1) ***"));
        break;
    case AVCONTEXT_IO_ERROR_2:
        emit notifyError(tr("*** Error: IO (2) ***"));
        break;
    case AVCONTEXT_IO_ERROR_3:
        emit notifyError(tr("*** Error: IO (3) ***"));
        break;
    case AVCONTEXT_TS_NOSYNC:
        emit notifyError(tr("*** Error: TS sync ***"));
        break;
    case AVCONTEXT_EOF_1:
        emit notifyError(tr("*** End (E1) ***"));
        break;
    case AVCONTEXT_EOF_2:
        emit notifyError(tr("*** End (E2) ***"));
        break;
    case AVCONTEXT_EOF_3:
        emit notifyError(tr("*** SUCCESS ***"));
        break;
    }
    emit notifyDone(101, currentThreadId());
    QThread::exec();
}

const uint8_t* TsParser::read(const int64_t& position, int32_t sizeToRead, bool &bEof)
{
    // out of range
    if (sizeToRead > static_cast<int64_t>(m_buffer.size()))
        return nullptr;

    // already read?
    auto sz = avRbe_ - m_buffer.data();
    if (position < avPos_ || position > avPos_ + sz)
    {

        // seek and reset buffer
        int64_t ret = -1;
        if (m_file.seek(position))
            ret = m_file.pos();

        if (ret != 0)
            return nullptr;

        avPos_ = position;
        avRbs_ = avRbe_ = m_buffer.data();

    }
    else
    {
        // move to the desired pos in buffer
        avRbs_ = m_buffer.data() + (position - avPos_);
    }

    m_progress = qRound(qreal(avPos_) * 100.0 / m_file.size());

    emit notifyDone(m_progress, currentThreadId());

    auto dataread = avRbe_ - avRbs_;
    if (dataread >= sizeToRead)
        return avRbs_;

    memmove(m_buffer.data(), avRbs_, static_cast<uint64_t>(dataread));
    avRbs_ = m_buffer.data();
    avRbe_ = avRbs_ + dataread;
    avPos_ = position;

    auto len = (static_cast<int64_t>(m_buffer.size()) - dataread);
    while (len > 0)
    {
        int64_t readResult = m_file.read(reinterpret_cast<char*>(avRbe_), len);

        if(readResult == 0)
            bEof = true;

        if (readResult > 0)
        {
            avRbe_ += readResult;
            dataread += readResult;
            len -= readResult;
        }

        if (dataread >= sizeToRead || readResult <= 0)
            break;
    }

    return dataread >= sizeToRead ? avRbs_ : nullptr;

}

int32_t TsParser::process()
{
    int32_t ret = 0;
    while (true)
    {
        ret = AVContext_->TSResync();
        if (ret != AVCONTEXT_CONTINUE)
            break;

        ret = AVContext_->processTSPackage();
        if (AVContext_->hasPIDStreamData())
        {
            STREAM_PKG pkg;
            while (getStreamData(&pkg))
            {
                if (pkg.streamChange)
                    showStreamInfo(pkg.pid);
                writeStreamData(&pkg);
            }
        }

        if (AVContext_->hasPIDPayload())
        {
            ret = AVContext_->processTSPayload();
            if (ret == AVCONTEXT_PROGRAM_CHANGE)
            {
                registerPmt();
                QVector<TsStream*> streams = AVContext_->getStreams();
                QVector<TsStream*>::const_iterator It = streams.begin();
                for (; It != streams.end(); ++It)
                {
                    if ((*It)->hasStreamInfo_)
                        showStreamInfo((*It)->pid_);
                }
            }
        }

        if (ret == AVCONTEXT_TS_ERROR)
            AVContext_->shift();
        else
            AVContext_->goNext();
    }
    return ret;
}

bool TsParser::getStreamData(STREAM_PKG* pkg)
{
    TsStream* es = AVContext_->getPIDStream();
    if (es == nullptr)
        return false;

    if (!es->getStreamPackage(pkg))
        return false;

    if (pkg->duration > 180000)
    {
        pkg->duration = 0;
    }
    else if (pkg->pid == mainStreamPID_)
    {
        // Fill duration map for main stream
        curTime_ += pkg->duration;
        if (curTime_ >= pinTime_)
        {
            pinTime_ += POSMAP_PTS_INTERVAL;
            if (curTime_ > endTime_)
            {
                AV_POSMAP_ITEM item;
                item.avPts = pkg->pts;
                item.avPos = AVContext_->getPosition();
                m_positionMap.insert(curTime_, item);
                endTime_ = curTime_;
                emit notifyDone(m_progress, currentThreadId());
            }
        }

        // Sync main DTS & PTS
        absDTS_ = pkg->dts;
        absPTS_ = pkg->pts;
    }
    return true;
}

void TsParser::resetPosmap()
{
    if (!m_positionMap.empty())
    {
        m_positionMap.clear();
        pinTime_ = curTime_ = endTime_ = 0;
    }
}

void TsParser::registerPmt()
{
    auto esStreams = AVContext_->getStreams();

    if (esStreams.empty())
        return;

    mainStreamPID_ = esStreams[0]->pid_;

    for (auto &stream : esStreams)
    {

        auto channel = AVContext_->getChannel(stream->pid_);
        auto codecName = stream->getStreamCodec();
        auto extension = stream->getFileExtension(stream->streamType_);
        auto fIt = outfiles_.find(stream->pid_);
        if (fIt != outfiles_.end())
            continue;

        QFileInfo fileInfo(m_file.fileName());
        auto filename = QString("%1/%2_stream_%3_%4_%5%6")
            .arg(fileInfo.path())
            .arg(fileInfo.baseName())
            .arg(channel)
            .arg(stream->pid_)
            .arg(codecName)
            .arg(extension);

        qDebug() << "Stream channel" << channel << "PID" << stream->pid_ << "codec" << codecName << "to file" << filename;

        auto &outFile = outfiles_[stream->pid_];
        outFile.setFileName(filename);
        if (!outFile.open(QIODevice::WriteOnly | QFile::Truncate))
        {
            emit notifyError(tr("Unable to open\n %1 \n %2").arg(outFile.fileName()).arg(outFile.errorString()));
            return;
        }

        AVContext_->startStreaming(stream->pid_);
    }
}

void TsParser::showStreamInfo(uint16_t pid)
{
    auto es = AVContext_->getStream(pid);
    if (es == nullptr)
        return;

    es->streamInfo_.pid = pid;
    es->streamInfo_.channel = AVContext_->getChannel(pid);
    strcpy(es->streamInfo_.codecName, es->getStreamCodec().toStdString().c_str());
    emit streamFound(es->streamInfo_, this);
}

void TsParser::writeStreamData(STREAM_PKG* pkg)
{
    if (pkg == nullptr)
        return;

    if (pkg->size > 0 && pkg->data)
    {
        auto It = outfiles_.find(pkg->pid);
        if (It != outfiles_.end())
        {
            auto c = It->second.write(reinterpret_cast<const char*>(pkg->data), pkg->size);
            It->second.flush();
            if (c != pkg->size)
                AVContext_->stopStreaming(pkg->pid);
        }
    }
}
