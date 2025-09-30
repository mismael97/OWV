#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_parser(new VCDParser(this))
    , m_waveformView(new WaveformView(this))
    , m_signalTree(new QTreeWidget(this))
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_statusLabel(new QLabel(this))
{
    setupUI();
    createActions();
    createMenus();
    createToolBars();
    createStatusBar();

    connect(m_parser, &VCDParser::parsingCompleted, this, &MainWindow::onVCDLoaded);
    connect(m_signalTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::onSignalTreeSelectionChanged);

    setWindowTitle("VCD Wave Viewer");
    resize(1000, 700);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    m_signalTree->setHeaderLabel("Signals");
    m_signalTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_signalTree->setColumnCount(1);
    m_signalTree->header()->setSectionResizeMode(QHeaderView::Stretch);

    m_splitter->addWidget(m_signalTree);
    m_splitter->addWidget(m_waveformView);
    m_splitter->setSizes({250, 750});

    setCentralWidget(m_splitter);
}

void MainWindow::createActions()
{
    m_openAct = new QAction(tr("&Open..."), this);
    m_openAct->setShortcuts(QKeySequence::Open);
    m_openAct->setStatusTip(tr("Open a VCD file"));
    connect(m_openAct, &QAction::triggered, this, &MainWindow::openVCDFile);

    m_exitAct = new QAction(tr("E&xit"), this);
    m_exitAct->setShortcuts(QKeySequence::Quit);
    m_exitAct->setStatusTip(tr("Exit the application"));
    connect(m_exitAct, &QAction::triggered, this, &MainWindow::close);

    m_zoomInAct = new QAction(tr("Zoom &In"), this);
    m_zoomInAct->setShortcut(QKeySequence::ZoomIn);
    m_zoomInAct->setStatusTip(tr("Zoom in the waveform"));
    connect(m_zoomInAct, &QAction::triggered, this, &MainWindow::zoomIn);

    m_zoomOutAct = new QAction(tr("Zoom &Out"), this);
    m_zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    m_zoomOutAct->setStatusTip(tr("Zoom out the waveform"));
    connect(m_zoomOutAct, &QAction::triggered, this, &MainWindow::zoomOut);

    m_resetZoomAct = new QAction(tr("&Reset Zoom"), this);
    m_resetZoomAct->setStatusTip(tr("Reset waveform zoom to default"));
    connect(m_resetZoomAct, &QAction::triggered, this, &MainWindow::resetZoom);
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(m_openAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_exitAct);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(m_zoomInAct);
    viewMenu->addAction(m_zoomOutAct);
    viewMenu->addAction(m_resetZoomAct);
}

void MainWindow::createToolBars()
{
    QToolBar* fileToolBar = addToolBar(tr("File"));
    fileToolBar->addAction(m_openAct);

    QToolBar* viewToolBar = addToolBar(tr("View"));
    viewToolBar->addAction(m_zoomInAct);
    viewToolBar->addAction(m_zoomOutAct);
    viewToolBar->addAction(m_resetZoomAct);
}

void MainWindow::createStatusBar()
{
    statusBar()->addWidget(m_statusLabel);
    m_statusLabel->setText("Ready");
}

void MainWindow::openVCDFile()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open VCD File"),
        "",
        tr("VCD Files (*.vcd);;All Files (*)")
        );

    if (fileName.isEmpty())
        return;

    m_statusLabel->setText("Loading VCD file: " + fileName);
    QApplication::processEvents();

    if (m_parser->parseFile(fileName)) {
        m_statusLabel->setText("VCD file loaded successfully");
    } else {
        QString errorMsg = m_parser->errorString();
        if (errorMsg.isEmpty()) {
            errorMsg = "Unknown error occurred while parsing VCD file";
        }
        QMessageBox::critical(this, tr("Error"),
                              tr("Failed to parse VCD file:\n%1").arg(errorMsg));
        m_statusLabel->setText("Error loading VCD file");
    }
}
void MainWindow::onVCDLoaded()
{
    m_signalTree->clear();
    m_waveformView->clear();

    const auto& signalList = m_parser->getSignals();  // ✅ Renamed variable
    for (const auto& signal : signalList) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_signalTree);
        item->setText(0, signal.name);
        item->setData(0, Qt::UserRole, signal.id);
    }

    m_statusLabel->setText(QString("Loaded %1 signals").arg(signalList.size()));
}

void MainWindow::onSignalSelected(const QString& signalName)
{
    const auto& signalList = m_parser->getSignals();  // ✅ Renamed variable
    for (const auto& signal : signalList) {
        if (signal.name == signalName) {
            m_waveformView->setSignalData(signal, m_parser->getTimeScale());
            break;
        }
    }
}

void MainWindow::onSignalTreeSelectionChanged()
{
    if (!m_signalTree->selectedItems().isEmpty()) {
        QString signalName = m_signalTree->selectedItems().first()->text(0);
        onSignalSelected(signalName);
    }
}

void MainWindow::zoomIn()
{
    m_waveformView->zoomIn();
}

void MainWindow::zoomOut()
{
    m_waveformView->zoomOut();
}

void MainWindow::resetZoom()
{
    m_waveformView->resetZoom();
}
