#include "mainwindow.h"
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
}

MainWindow::~MainWindow()
{
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
    mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Create a widget for the left panel (signal tree + buttons)
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);

    // Create button layout
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // Create buttons
    selectAllButton = new QPushButton("Select All");
    deselectAllButton = new QPushButton("Deselect All");

    // Style the buttons
    selectAllButton->setStyleSheet("QPushButton { padding: 5px; font-weight: bold; }");
    deselectAllButton->setStyleSheet("QPushButton { padding: 5px; font-weight: bold; }");

    // Connect buttons to slots
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllSignals);
    connect(deselectAllButton, &QPushButton::clicked, this, &MainWindow::deselectAllSignals);

    // Add buttons to layout
    buttonLayout->addWidget(selectAllButton);
    buttonLayout->addWidget(deselectAllButton);
    buttonLayout->addStretch(); // Push buttons to the left

    // Create signal tree
    signalTree = new QTreeWidget();
    signalTree->setHeaderLabel("Signals");
    signalTree->setMinimumWidth(300);

    // Connect signals
    connect(signalTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::signalSelectionChanged);
    connect(signalTree, &QTreeWidget::itemChanged,
            this, &MainWindow::onSignalItemChanged);

    // Add widgets to left panel layout
    leftLayout->addLayout(buttonLayout);
    leftLayout->addWidget(signalTree);

    // Create waveform widget
    waveformWidget = new WaveformWidget();
    connect(waveformWidget, &WaveformWidget::timeChanged,
            this, &MainWindow::updateTimeDisplay);

    // Add panels to splitter
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(waveformWidget);

    // Set splitter proportions
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    setCentralWidget(mainSplitter);
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Open VCD File", "", "VCD Files (*.vcd)");

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

        populateSignalTree();
        waveformWidget->setVcdData(vcdParser);

    } else {
        QMessageBox::critical(this, "Error",
                              "Failed to parse VCD file: " + vcdParser->getError());
        statusLabel->setText("Ready");
    }
}

void MainWindow::populateSignalTree()
{
    signalTree->clear();

    const auto& vcdSignals = vcdParser->getSignals();
    QMap<QString, QTreeWidgetItem*> scopeItems;

    for (const auto& signal : vcdSignals) {
        QString scopePath = signal.scope;
        if (!scopeItems.contains(scopePath)) {
            QStringList scopeParts = scopePath.split('.');
            QTreeWidgetItem *parent = nullptr;
            QString currentPath;

            for (const QString& part : scopeParts) {
                if (!currentPath.isEmpty()) currentPath += ".";
                currentPath += part;

                if (!scopeItems.contains(currentPath)) {
                    QTreeWidgetItem *item = new QTreeWidgetItem();
                    item->setText(0, part);
                    item->setData(0, Qt::UserRole, currentPath);

                    if (parent) {
                        parent->addChild(item);
                    } else {
                        signalTree->addTopLevelItem(item);
                    }

                    scopeItems[currentPath] = item;
                    parent = item;
                } else {
                    parent = scopeItems[currentPath];
                }
            }
        }

        QTreeWidgetItem *signalItem = new QTreeWidgetItem();
        signalItem->setText(0, signal.name);
        signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
        signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);
        signalItem->setCheckState(0, Qt::Unchecked);

        scopeItems[scopePath]->addChild(signalItem);
    }

    signalTree->expandAll();
    statusLabel->setText("Ready");
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

void MainWindow::signalSelectionChanged()
{
    // This method can be kept for selection changes if needed
    // But we'll use onSignalItemChanged for checkbox changes
}

void MainWindow::onSignalItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)

    // Only process if it's a signal item (has VCDSignal data)
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<VCDSignal>()) {
        updateVisibleSignals();
    }
}

void MainWindow::updateVisibleSignals()
{
    QList<VCDSignal> selectedSignals;

    // Get all checked signals
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>()) {
                selectedSignals.append(data.value<VCDSignal>());
            }
        }
        ++it;
    }

    // Update the waveform widget with selected signals
    waveformWidget->setVisibleSignals(selectedSignals);

    // Update status
    statusLabel->setText(QString("%1 signal(s) displayed").arg(selectedSignals.size()));
}

void MainWindow::setAllSignalsCheckState(Qt::CheckState state)
{
    // Temporarily disconnect the itemChanged signal to prevent multiple updates
    disconnect(signalTree, &QTreeWidget::itemChanged, this, &MainWindow::onSignalItemChanged);

    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            (*it)->setCheckState(0, state);
        }
        ++it;
    }

    // Reconnect the signal
    connect(signalTree, &QTreeWidget::itemChanged, this, &MainWindow::onSignalItemChanged);

    // Update the waveform
    updateVisibleSignals();
}

void MainWindow::selectAllSignals()
{
    setAllSignalsCheckState(Qt::Checked);
    statusLabel->setText("All signals selected");
}

void MainWindow::deselectAllSignals()
{
    setAllSignalsCheckState(Qt::Unchecked);
    statusLabel->setText("All signals deselected");
}

void MainWindow::updateTimeDisplay(int time)
{
    timeLabel->setText(QString("Time: %1").arg(time));
}

void MainWindow::about()
{
    QMessageBox::about(this, "About VCD Wave Viewer",
                       "VCD Wave Viewer\n\n"
                       "A simple waveform viewer for Value Change Dump (VCD) files.\n"
                       "Built with Qt C++\n\n"
                       "Features:\n"
                       "- Select/Deselect All buttons\n"
                       "- Real-time waveform updates\n"
                       "- Zoom and pan functionality");
}
