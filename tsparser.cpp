#include "tsparser.h"
#include "tscontext.h"

#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>

////////////////////////////////////////////////////////////////////
TsParser::TsParser(const QString& filePath, QObject* parent)
    : QThread(parent),
    tsfile_(filePath),
    avBufSize_(AV_BUFFER_SIZE),
    mainStreamPID_(0xffff),
    absDTS_(PTS_UNSET),
    absPTS_(PTS_UNSET),
    pinTime_(0),
    curTime_(0),
    endTime_(0),
    done_(0),
    fileSize_(0)
{
    avBuf_ = (quint8*)malloc(sizeof(*avBuf_)*(avBufSize_ + 1));
    if( avBuf_ != NULL )
    {
        avPos_ = 0;
        avRbs_ = avBuf_;
        avRbe_ = avBuf_;
        AVContext_.reset(new AVContext(*this, 0, 0));
    }
    else
        throw std::exception("Alloc AV buffer failed");
}

TsParser::~TsParser()
{
    exit();

    FilesT::iterator It = outfiles_.begin();
    for(; It != outfiles_.end(); ++It)
        if( It.value() != NULL )
            fclose(It.value());

    // Free AV buffer
    if( avBuf_ != NULL ) {
        free(avBuf_);
        avBuf_ = NULL;
    }
    wait();
}

bool TsParser::start()
{
    if( tsfile_.open(QFile::ReadOnly) ) {
        QThread::start();
        return true;
    }
    emit notifyError("Cannot open mpeg-ts source file");
    return false;
}

void TsParser::run()
{
    emit notifyStart((qint32)currentThreadId(), *this);

    qint32 code = process();
    switch(code) {
    case AVCONTEXT_TS_ERROR:
        emit notifyError("One of TS streams has an invalid format");
        break;
    case AVCONTEXT_IO_ERROR:
        if( done_ < 100 ) // IO error happens when reached the end of source file as well
            emit notifyError("Error opening TS source");
        break;
    case AVCONTEXT_TS_NOSYNC:
        emit notifyError("Error TS source opening or it has an invalid format");
        break;
    }
    emit notifyDone(101, (qint32)currentThreadId());
    QThread::exec();
}

const quint8* TsParser::read(const qint64& pos, qint32 n)
{
    // out of range
    if( n > avBufSize_ )
        return NULL;

    if( avPos_ == 0 ) 
        fileSize_ = tsfile_.size();

    // already read?
    qint32 sz = avRbe_ - avBuf_;
    if( pos < avPos_ || pos > avPos_+sz )
    {
        // seek and reset buffer
        qint64 ret = -1;
        if( tsfile_.seek(pos) )
            ret = tsfile_.pos();
        if( ret != 0 )
            return NULL;
        avPos_ = pos;
        avRbs_ = avRbe_ = avBuf_;
    }
    else
    {
        // move to the desired pos in buffer
        avRbs_ = avBuf_ + (qint32)(pos - avPos_);
    }

    if( fileSize_ )
        done_ = qRound(qreal(avPos_)*100/fileSize_);

    qint32 dataread = avRbe_ - avRbs_;
    if( dataread >= n )
        return avRbs_;

    memmove(avBuf_, avRbs_, dataread);
    avRbs_ = avBuf_;
    avRbe_ = avRbs_ + dataread;
    avPos_ = pos;

    qint64 len = (qint64)(avBufSize_ - dataread);
    while( len > 0 )
    {
        qint64 c = tsfile_.read((char*)avRbe_, len);
        if( c > 0 ) {
            avRbe_ += c;
            dataread += c;
            len -= c;
        }

        if( dataread >= n || c <= 0 )
            break;
    }
    return dataread >= n ? avRbs_ : NULL;
}

qint32 TsParser::process()
{
    qint32 ret = 0;
    while( true )
    {
        ret = AVContext_->TSResync();
        if( ret != AVCONTEXT_CONTINUE )
            break;

        ret = AVContext_->processTSPackage();
        if( AVContext_->hasPIDStreamData() ) 
        {
            STREAM_PKG pkg;
            while( getStreamData(&pkg) )
            {
                if( pkg.streamChange )
                    showStreamInfo( pkg.pid );
                writeStreamData( &pkg );
            }
        }

        if( AVContext_->hasPIDPayload() )
        {
            ret = AVContext_->processTSPayload();
            if( ret == AVCONTEXT_PROGRAM_CHANGE )
            {
                registerPmt();
                QVector<TsStream*> streams = AVContext_->getStreams();
                QVector<TsStream*>::const_iterator It = streams.begin();
                for(; It != streams.end(); ++It)
                {
                    if( (*It)->hasStreamInfo_ )
                        showStreamInfo((*It)->pid_);
                }
            }
        }

        if( ret == AVCONTEXT_TS_ERROR )
            AVContext_->shift();
        else
            AVContext_->goNext();
    }
    return ret;
}

bool TsParser::getStreamData(STREAM_PKG* pkg)
{
    TsStream* es = AVContext_->getPIDStream();
    if( es == NULL )
        return false;

    if( !es->getStreamPackage(pkg) )
        return false;

    if( pkg->duration > 180000 )
    {
        pkg->duration = 0;
    }
    else if( pkg->pid == mainStreamPID_ )
    {
        // Fill duration map for main stream
        curTime_ += pkg->duration;
        if( curTime_ >= pinTime_ )
        {
            pinTime_ += POSMAP_PTS_INTERVAL;
            if( curTime_ > endTime_ )
            {
                AV_POSMAP_ITEM item;
                item.avPts = pkg->pts;
                item.avPos = AVContext_->getPosition();
                posMap_.insert(curTime_,item);
                endTime_ = curTime_;
                emit notifyDone(done_,(qint32)currentThreadId());
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
    if( !posMap_.empty() )
    {
        posMap_.clear();
        pinTime_ = curTime_ = endTime_ = 0;
    }
}

void TsParser::registerPmt()
{
    const QVector<TsStream*> esStreams = AVContext_->getStreams();
    if( !esStreams.empty() )
    {
        mainStreamPID_ = esStreams[0]->pid_;
        QVector<TsStream*>::const_iterator It = esStreams.begin();
        for(; It != esStreams.end(); ++It)
        {
            quint16 channel = AVContext_->getChannel((*It)->pid_);
            QString codecName = (*It)->getStreamCodec();
            QString extension = (*It)->getFileExtension((*It)->streamType_);
            FilesT::iterator fIt = outfiles_.find((*It)->pid_);
            if( fIt == outfiles_.end() )
            {
                char filename[512];
                sprintf_s( filename, 512, "%s_stream_%u_%.4x_%s%s", 
                           QFileInfo(tsfile_.fileName()).baseName().toStdString().c_str(),
                           channel, 
                           (*It)->pid_, 
                           codecName.toStdString().c_str(),
                           extension.toStdString().c_str() );

                qDebug() << "Stream channel" << channel << "PID" << (*It)->pid_ << "codec" << codecName << "to file" << filename;
                FILE* f = NULL;
                fopen_s(&f, filename, "wb+");
                if( f == NULL ) {
                    qDebug() << "Cannot open" << filename << "for writing!";
                    continue;
                }
                outfiles_.insert((*It)->pid_,f);
            }
            else
                continue;
            AVContext_->startStreaming((*It)->pid_);
        }
    }
}

void TsParser::showStreamInfo(quint16 pid)
{
    TsStream* es = AVContext_->getStream(pid);
    if( es == NULL ) 
        return;

    es->streamInfo_.pid = pid;
    es->streamInfo_.channel = AVContext_->getChannel(pid);
    strcpy_s(es->streamInfo_.codecName, sizeof(es->streamInfo_.codecName), es->getStreamCodec().toStdString().c_str());
    emit streamFound(es->streamInfo_);
}

void TsParser::writeStreamData(STREAM_PKG* pkg)
{
    if( pkg == NULL )
        return;

    if( pkg->size > 0 && pkg->data )
    {
        FilesT::iterator It = outfiles_.find(pkg->pid);
        if( It != outfiles_.end() )
        {
            qint32 c = fwrite((const char*)pkg->data, 1, pkg->size, It.value());
            fflush(It.value());
            if( c != pkg->size )
                AVContext_->stopStreaming(pkg->pid);
        }
    }
}
