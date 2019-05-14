#include "resource.h"
#include "mainwindow.h"
#include "infotreemodel.h"

#include <QtWidgets>

//////////////////////////////////////////////////////////
MainWindow::MainWindow()
    : guardParsers_(new QMutex())
{
    // initialize global objects
    qRegisterMetaType<STREAM_INFO>("STREAM_INFO");
    Resources::Init();

    // setup the main dialog
    setup();
    setGeometry( Resources::desktopX*0.3, 
                 Resources::desktopY*0.2, 
                 Resources::desktopX*0.5,
                 Resources::desktopY*0.3 );
    show();
    setWindowTitle(tr("MPEG-TS Streams Extractor"));
}

void MainWindow::setup()
{
    QVBoxLayout* vlayout = new QVBoxLayout();
    QGroupBox* group = new QGroupBox();
    QHBoxLayout* hlayout = new QHBoxLayout();

    btnOpenFile_ = new QPushButton(this);
    btnOpenFile_->setIcon(QIcon(*Resources::pxOpenfile));
    btnOpenFile_->setToolTip(tr("Choose a mpeg-ts file(s)"));
    btnOpenFile_->setFixedSize(36,36);
    btnOpenFile_->setIconSize(QSize(35,34));
    QObject::connect(btnOpenFile_, SIGNAL(clicked()), this, SLOT(onOpenFiles()));
    hlayout->addWidget(btnOpenFile_, 0, Qt::AlignLeft);

    QLabel* label = new QLabel(tr(" Add the mpeg-ts file(s) for extraction of streams with audio, video, subtitles, etc "));
    hlayout->addWidget(label, 5, Qt::AlignCenter);

    label->setFrameStyle(QFrame::Panel|QFrame::Sunken);
    label->setFixedHeight(26);
    group->setLayout(hlayout);
    vlayout->addWidget(group);

    treeModel_ = new InfoTreeModel(this);
    treeView_ = new QTreeView(this);
    treeView_->setModel(treeModel_);
    treeView_->header()->hide();
    treeModel_->setView(treeView_);
    vlayout->addWidget(treeView_);

    progress_ = new QProgressBar(this);
    progress_->setRange(0,100);
    progress_->setFixedHeight(26);
    progress_->setTextVisible(true);
    progress_->setVisible(false);
    vlayout->addWidget(progress_);

    vlayout->setAlignment(Qt::AlignTop);
    setLayout(vlayout);
}

void MainWindow::onOpenFiles()
{
    QStringList tsFiles = QFileDialog::getOpenFileNames(this, tr("Choose a mpeg-ts file"), "","mpeg-ts (*.ts *.tp *.m2ts)");
    QStringList::const_iterator It = tsFiles.begin();

    for(; It != tsFiles.end(); ++It) 
    {
        const QString& path = *It;
        if( path.isEmpty() || treeModel_->isAdded(path) ) 
            continue;

        TsParser* parser = new TsParser(path, this);
        QObject::connect(parser, SIGNAL(streamFound(STREAM_INFO)), this, SLOT(onStreamFound(STREAM_INFO)), Qt::DirectConnection);
        QObject::connect(parser, SIGNAL(notifyStart(qint32,TsParser)), this, SLOT(onNotifyStart(qint32,TsParser)), Qt::DirectConnection);
        QObject::connect(parser, SIGNAL(notifyDone(qint32,qint32)), this, SLOT(onNotifyDone(qint32,qint32)), Qt::QueuedConnection);
        QObject::connect(parser, SIGNAL(notifyError(QString)), this, SLOT(onNotifyError(QString)), Qt::QueuedConnection);

        treeModel_->addSourceTs(path);
        if( !parser->start() )
            delete parser;
    }
}

static inline int streamIdentifier(qint32 compositionId, qint32 ancillaryId)
{
    return ((compositionId & 0xff00) >> 8)
            | ((compositionId & 0xff) << 8)
            | ((ancillaryId & 0xff00) << 16)
            | ((ancillaryId & 0xff) << 24);
}

void MainWindow::onStreamFound(const STREAM_INFO& streamInfo)
{
    treeModel_->addStreamInfo(streamInfo);

/*
    sprintf_s(info, 5000, "dump stream infos for channel %u PID %.4x\n", channel, es->pid_);
    sprintf_s(info, 5000, "  Codec name     : %s\n", es->getStreamCodec().toStdString().c_str());
    sprintf_s(info, 5000, "  Language       : %s\n", es->streamInfo_.language);
    sprintf_s(info, 5000, "  Identifier     : %.8x\n", streamIdentifier(es->streamInfo_.compositionId, es->streamInfo_.ancillaryId));
    sprintf_s(info, 5000, "  FPS scale      : %d\n", es->streamInfo_.fpsScale);
    sprintf_s(info, 5000, "  FPS rate       : %d\n", es->streamInfo_.fpsRate);
    sprintf_s(info, 5000, "  Interlaced     : %s\n", (es->streamInfo_.interlaced ? "true" : "false"));
    sprintf_s(info, 5000, "  Height         : %d\n", es->streamInfo_.height);
    sprintf_s(info, 5000, "  Width          : %d\n", es->streamInfo_.width);
    sprintf_s(info, 5000, "  Aspect         : %3.3f\n", es->streamInfo_.aspect);
    sprintf_s(info, 5000, "  Channels       : %d\n", es->streamInfo_.channels);
    sprintf_s(info, 5000, "  Sample rate    : %d\n", es->streamInfo_.sampleRate);
    sprintf_s(info, 5000, "  Block align    : %d\n", es->streamInfo_.blockAlign);
    sprintf_s(info, 5000, "  Bit rate       : %d\n", es->streamInfo_.bitRate);
    sprintf_s(info, 5000, "  Bit per sample : %d\n", es->streamInfo_.bitsPerSample);
    */
}

void MainWindow::onNotifyError(const QString& info)
{
    QMessageBox(QMessageBox::Warning, tr("Error"), info, QMessageBox::Close).exec();
}

void MainWindow::onNotifyStart(qint32 threadId, const TsParser& parser)
{
    QMutexLocker g(guardParsers_.data());
    parsersMap_.insert(threadId, const_cast<TsParser*>(&parser));
}

void MainWindow::onNotifyDone(qint32 percent, qint32 threadId)
{
    // on first destroy parser when 101%
    static qint32 lastP = percent;

    if( percent == 101 ) {
        QMutexLocker g(guardParsers_.data());
        ParsersMapT::iterator It = parsersMap_.find(threadId);
        if( It != parsersMap_.end() ) {
            delete It.value();
            parsersMap_.erase(It);
        }
    }

    // value when few threads is working
    lastP = abs(progress_->value()-percent) > 1 ? qRound(qreal(percent+lastP+2)/2) : percent;
    if( lastP > 100 )
    {
        progress_->reset();
        progress_->setVisible(false);
    }
    else if( percent != 101 && !progress_->isVisible() )
        progress_->setVisible(true);
    else if( lastP > progress_->value() )
        progress_->setValue(lastP);
}
