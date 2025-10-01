#include "mainwindow.h"
#include "SignalSelectionDialog.h"
#include <QTreeWidgetItemIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QLabel>
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDir>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), vcdParser(new VCDParser(this))
{
    qRegisterMetaType<VCDSignal>("VCDSignal");
    setWindowTitle("VCD Wave Viewer");
    setMinimumSize(1200, 800);

    createActions();
    setupUI();
    createToolBar();
    createStatusBar();

    loadDefaultVcdFile();
}

MainWindow::~MainWindow()
{
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete) {
        removeSelectedSignals();
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::createActions()
{
    openAction = new QAction("Open", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    zoomInAction = new QAction("Zoom In", this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    zoomOutAction = new QAction("Zoom Out", this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    zoomFitAction = new QAction("Zoom Fit", this);
    connect(zoomFitAction, &QAction::triggered, this, &MainWindow::zoomFit);

    aboutAction = new QAction("About", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar("Main Toolbar");
    toolBar->addAction(openAction);
    toolBar->addSeparator();
    toolBar->addAction(zoomInAction);
    toolBar->addAction(zoomOutAction);
    toolBar->addAction(zoomFitAction);
    toolBar->addSeparator();
    toolBar->addAction(aboutAction);
}

void MainWindow::createStatusBar()
{
    statusLabel = new QLabel("Ready");
    statusBar()->addWidget(statusLabel);

    timeLabel = new QLabel("Time: 0");
    statusBar()->addPermanentWidget(timeLabel);
}

void MainWindow::setupUI()
{
    // Create central widget with waveform
    QWidget *centralWidget = new QWidget();
    QVBoxLayout *centralLayout = new QVBoxLayout(centralWidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Create waveform widget (now includes signal names column)
    waveformWidget = new WaveformWidget();
    connect(waveformWidget, &WaveformWidget::timeChanged,
            this, &MainWindow::updateTimeDisplay);
    connect(waveformWidget, &WaveformWidget::signalSelected, this, [this](int index) {
        // Enable/disable remove button based on selection
        removeSignalsButton->setEnabled(index >= 0);
    });

    // === BOTTOM CONTROLS ===
    QWidget *bottomControls = new QWidget();
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomControls);
    bottomLayout->setContentsMargins(10, 5, 10, 5);

    addSignalsButton = new QPushButton("+ Add Signals");
    removeSignalsButton = new QPushButton("🗑️");

    addSignalsButton->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; background-color: #4CAF50; color: white; }");
    removeSignalsButton->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; background-color: #f44336; color: white; }");
    removeSignalsButton->setEnabled(false);
    removeSignalsButton->setToolTip("Remove selected signal (Delete)");

    connect(addSignalsButton, &QPushButton::clicked, this, &MainWindow::showAddSignalsDialog);
    connect(removeSignalsButton, &QPushButton::clicked, this, &MainWindow::removeSelectedSignals);

    bottomLayout->addWidget(addSignalsButton);
    bottomLayout->addWidget(removeSignalsButton);
    bottomLayout->addStretch();

    centralLayout->addWidget(waveformWidget, 1);
    centralLayout->addWidget(bottomControls);

    setCentralWidget(centralWidget);
}

void MainWindow::showAddSignalsDialog()
{
    if (!vcdParser) return;

    SignalSelectionDialog dialog(this);
    dialog.setSignals(vcdParser->getSignals(), waveformWidget->visibleSignals);

    if (dialog.exec() == QDialog::Accepted) {
        QList<VCDSignal> newlySelectedSignals = dialog.getSelectedSignals();
        if (!newlySelectedSignals.isEmpty()) {
            QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;

            // Add only signals that aren't already displayed
            for (const auto& signal : newlySelectedSignals) {
                bool alreadyExists = false;
                for (const auto& existing : currentSignals) {
                    if (existing.identifier == signal.identifier) {
                        alreadyExists = true;
                        break;
                    }
                }
                if (!alreadyExists) {
                    currentSignals.append(signal);
                }
            }

            waveformWidget->setVisibleSignals(currentSignals);
            statusLabel->setText(QString("%1 signal(s) displayed").arg(currentSignals.size()));
        }
    }
}

void MainWindow::removeSelectedSignals()
{
    waveformWidget->removeSelectedSignals();
    removeSignalsButton->setEnabled(false);
    statusLabel->setText(QString("%1 signal(s) displayed").arg(waveformWidget->visibleSignals.size()));
}

void MainWindow::loadDefaultVcdFile()
{
    QString defaultPath = "C:/Users/mismael/Desktop/OWV/rtl.vcd";

    if (QFile::exists(defaultPath)) {
        loadVcdFile(defaultPath);
    } else {
        statusLabel->setText("Default VCD file not found. Use File → Open to load a VCD file.");
        qDebug() << "Default VCD file not found:" << defaultPath;
    }
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Open VCD File", "C:/Users/mismael/Desktop/OWV", "VCD Files (*.vcd)");

    if (!filename.isEmpty()) {
        loadVcdFile(filename);
    }
}

void MainWindow::loadVcdFile(const QString &filename)
{
    statusLabel->setText("Loading VCD file...");
    QApplication::processEvents();

    if (vcdParser->parseFile(filename)) {
        statusLabel->setText(QString("Loaded: %1 (%2 signals)").arg(QFileInfo(filename).fileName()).arg(vcdParser->getSignals().size()));
        waveformWidget->setVcdData(vcdParser);
    } else {
        QMessageBox::critical(this, "Error",
                              "Failed to parse VCD file: " + vcdParser->getError());
        statusLabel->setText("Ready");
    }
}

void MainWindow::zoomIn()
{
    waveformWidget->zoomIn();
}

void MainWindow::zoomOut()
{
    waveformWidget->zoomOut();
}

void MainWindow::zoomFit()
{
    waveformWidget->zoomFit();
}

void MainWindow::updateTimeDisplay(int time)
{
    timeLabel->setText(QString("Time: %1").arg(time));
}

void MainWindow::about()
{
    QMessageBox::about(this, "About VCD Wave Viewer",
                       "VCD Wave Viewer\n\n"
                       "A professional waveform viewer for Value Change Dump (VCD) files.\n"
                       "Built with Qt C++\n\n"
                       "Features:\n"
                       "- Unified signal names and waveform display\n"
                       "- Dark theme\n"
                       "- Drag to reorder signals\n"
                       "- Professional signal selection dialog\n"
                       "- Mouse wheel navigation");
}
