
#include "mainwindow.h"

#include <QStandardPaths>
#include <QtWidgets>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
    , m_error(new QTextEdit(this))
    , m_treeView( new QTreeView(this))
    , m_progress(new QProgressBar(this))
    , m_buttonOpenFile(new QPushButton(tr("Select file(s)"), this))
    , m_lock(new QMutex())
{
    setWindowTitle(tr("TS Streams Extractor"));
    setWindowIcon(QPixmap(":/tssplitter_logo.svg"));

    setWindowModality(Qt::WindowModality::ApplicationModal);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAcceptDrops(true);

    qRegisterMetaType<STREAM_INFO>("STREAM_INFO");

    // tree
    //m_treeView->setModel(new QStandardItemModel(0, 0));
    m_treeView->setRootIsDecorated(true);
    m_treeView->setSelectionMode(QTreeView::SingleSelection);
    m_treeView->setAnimated(true);
    m_treeView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_treeView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_treeView->setSortingEnabled(false);
    m_treeView->setAlternatingRowColors(true);
    m_treeView->header()->setSortIndicator(0, Qt::AscendingOrder);
    m_treeView->header()->setDefaultAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // progress
    m_progress->setRange(0, 100);
    m_progress->setFixedHeight(26);
    m_progress->setTextVisible(true);
    m_progress->setVisible(false);

    // open
    m_buttonOpenFile->setDefault(true);

    // layout
    auto vlayout = new QVBoxLayout();
    vlayout->addWidget(m_treeView);

    vlayout->addWidget(m_error);

    auto hlayout = new QHBoxLayout();
    hlayout->addWidget(m_progress);
    hlayout->addWidget(m_buttonOpenFile, 0, Qt::AlignRight);
    hlayout->setContentsMargins(1, 1, 1, 1);
    hlayout->setMargin(1);
    hlayout->setSpacing(1);

    vlayout->addLayout(hlayout);
    vlayout->setMargin(1);
    vlayout->setContentsMargins(1, 1, 1, 1);
    vlayout->setSpacing(1);

    setLayout(vlayout);

    // connect
    QObject::connect(m_buttonOpenFile, &QPushButton::clicked, this, &MainWindow::onOpenFiles);

#if defined (Q_OS_ANDROID)
    resize(QApplication::desktop()->availableGeometry(this).size());
#else
    resize(QApplication::desktop()->availableGeometry(this).size() / 2);
#endif
}

MainWindow::~MainWindow()
{
    QMutexLocker g(m_lock.data());
    for (auto & item : m_parsersMap)
        delete item.second;
    m_parsersMap.clear();
}

void MainWindow::OpenFiles(const QStringList & tsFiles)
{
    m_treeView->setModel(new QStandardItemModel(0, 1));

    auto model = reinterpret_cast<QStandardItemModel*>(m_treeView->model());
    model->setHorizontalHeaderLabels(QStringList() << QObject::tr("info"));

    auto root = model->invisibleRootItem();

    for (const auto &path : tsFiles)
    {

        if (path.isEmpty())
            continue;

        TsParser* parser = new TsParser(path, this);
        QObject::connect(parser, &TsParser::streamFound, this, &MainWindow::onStreamFound, Qt::DirectConnection);
        QObject::connect(parser, &TsParser::notifyStart, this, &MainWindow::onNotifyStart, Qt::DirectConnection);
        QObject::connect(parser, &TsParser::notifyDone, this, &MainWindow::onNotifyDone, Qt::QueuedConnection);
        QObject::connect(parser, &TsParser::notifyError, this, &MainWindow::onNotifyError, Qt::QueuedConnection);

        root->appendRow(new QStandardItem(path));

        if (!parser->start())
            delete parser;
    }
}

void MainWindow::onOpenFiles()
{

    QSettings settings(QCoreApplication::organizationName(), QCoreApplication::applicationName());

    auto defFolder = settings.value(QLatin1String("DEFAULT_FOLDER")).toString();

    if (defFolder.isEmpty() || !QDir(defFolder).exists())
    {
        for (auto &itemLoc : QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation))
        {
            if (itemLoc.isEmpty())
                continue;
            if (QDir(itemLoc).exists())
            {
                defFolder = itemLoc;
                break;
            }
        }
    }

    QStringList tsFiles = QFileDialog::getOpenFileNames(this, tr("Select file(s)"), defFolder,
        QLatin1String("TS (") + m_supportedExt + QLatin1String(")"));

    if (tsFiles.size())
    {
        const auto path = QFileInfo(tsFiles.front()).path();
        settings.setValue(QLatin1String("DEFAULT_FOLDER"), path);
    }
    else
    {
        return;
    }

    OpenFiles(tsFiles);
}

static inline int streamIdentifier(int32_t compositionId, int32_t ancillaryId)
{
    return ((compositionId & 0xff00) >> 8)
        | ((compositionId & 0xff) << 8)
        | ((ancillaryId & 0xff00) << 16)
        | ((ancillaryId & 0xff) << 24);
}

void MainWindow::onStreamFound(const STREAM_INFO& streamInfo, TsParser* self)
{
    auto model = reinterpret_cast<QStandardItemModel*>(m_treeView->model());
    const auto srcName = self->getSourceName();
    auto items = model->findItems(srcName, Qt::MatchFixedString, 0);
    if (items.size() != 1)
        return;

    auto item = items.front();

    auto subitem = new QStandardItem(QString("channel %1 PID %2").arg(streamInfo.channel).arg(streamInfo.pid, 4, 16));
    item->appendRow(subitem);

    subitem->appendRow(new QStandardItem(QString("Codec name     : %1").arg(streamInfo.codecName)));
    subitem->appendRow(new QStandardItem(QString("Language       : %1").arg(streamInfo.language)));
    subitem->appendRow(new QStandardItem(QString("Identifier     : %1")
          .arg(streamIdentifier(streamInfo.compositionId, streamInfo.ancillaryId), 8, 16)));
    subitem->appendRow(new QStandardItem(QString("FPS scale      : %1").arg(streamInfo.fpsScale)));
    subitem->appendRow(new QStandardItem(QString("FPS rate       : %1").arg(streamInfo.fpsRate)));
    subitem->appendRow(new QStandardItem(QString("Interlaced     : %1").arg((streamInfo.interlaced ? "true" : "false"))));
    subitem->appendRow(new QStandardItem(QString("Height         : %1").arg(streamInfo.height)));
    subitem->appendRow(new QStandardItem(QString("Width          : %1").arg(streamInfo.width)));
    subitem->appendRow(new QStandardItem(QString("Aspect         : %1").arg(streamInfo.aspect, 0, 'f', 2)));
    subitem->appendRow(new QStandardItem(QString("Channels       : %1").arg(streamInfo.channels)));
    subitem->appendRow(new QStandardItem(QString("Sample rate    : %1").arg(streamInfo.sampleRate)));
    subitem->appendRow(new QStandardItem(QString("Block align    : %1").arg(streamInfo.blockAlign)));
    subitem->appendRow(new QStandardItem(QString("Bit rate       : %1").arg(streamInfo.bitRate)));
    subitem->appendRow(new QStandardItem(QString("Bit per sample : %1").arg(streamInfo.bitsPerSample)));

}

void MainWindow::onNotifyError(const QString& info)
{
    m_error->append(info + "\n");
}

void MainWindow::onNotifyStart(Qt::HANDLE threadId, TsParser *parser)
{
    QMutexLocker g(m_lock.data());
    m_parsersMap[threadId] = parser;
}

void MainWindow::onNotifyDone(int32_t percent, Qt::HANDLE threadId)
{
    // on first destroy parser when 101%
    static int32_t lastP = percent;

    if (percent == 101)
    {
        QMutexLocker g(m_lock.data());
        auto It = m_parsersMap.find(threadId);
        if (It != m_parsersMap.end())
        {
            delete It->second;
            m_parsersMap.erase(It);
        }
    }

    // value when few threads is working
    lastP = abs(m_progress->value() - percent) > 1 ? qRound(double(percent + lastP + 2) / 2) : percent;
    if (lastP > 100)
    {
        m_progress->reset();
        m_progress->setVisible(false);
    }
    else if (percent != 101 && !m_progress->isVisible())
    {
        m_progress->setVisible(true);
    }
    else if (lastP > m_progress->value())
    {
        m_progress->setValue(lastP);
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    event->accept();
    auto urls = event->mimeData()->urls();

    QStringList tsFiles;
    for (auto &item : urls)
    {
        tsFiles << item.toLocalFile();
    }

    OpenFiles(tsFiles);

    event->acceptProposedAction();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}
