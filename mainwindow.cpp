// file: mainwindow.cpp
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
#include <QDir>

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

    // Auto-load default VCD file
    loadDefaultVcdFile();
}

void MainWindow::loadDefaultVcdFile()
{
    // Default VCD file path
    QString defaultPath = "C:/Users/mismael/Desktop/OWV/rtl.vcd";

    // Check if default file exists
    if (QFile::exists(defaultPath)) {
        loadVcdFile(defaultPath);
    } else {
        statusLabel->setText("Default VCD file not found. Use File → Open to load a VCD file.");
        qDebug() << "Default VCD file not found:" << defaultPath;
    }
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

    // === LEFT PANEL - SIGNAL BROWSER ===
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(5, 5, 5, 5);
    leftLayout->setSpacing(8);

    // Signal Hierarchy Group
    QGroupBox *hierarchyGroup = new QGroupBox("Signal Hierarchy");
    QVBoxLayout *hierarchyLayout = new QVBoxLayout(hierarchyGroup);

    signalTree = new QTreeWidget();
    signalTree->setHeaderLabel("Modules & Signals");
    signalTree->setMinimumWidth(300);
    signalTree->setAlternatingRowColors(true);

    connect(signalTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::signalSelectionChanged);
    connect(signalTree, &QTreeWidget::itemChanged,
            this, &MainWindow::onSignalItemChanged);

    hierarchyLayout->addWidget(signalTree);

    // Signal Controls Group
    QGroupBox *controlsGroup = new QGroupBox("Signal Controls");
    QVBoxLayout *controlsLayout = new QVBoxLayout(controlsGroup);

    // Selection buttons
    QHBoxLayout *selectionLayout = new QHBoxLayout();
    selectAllButton = new QPushButton("Select All");
    deselectAllButton = new QPushButton("Deselect All");
    addSignalsButton = new QPushButton("+ Add Signals");
    removeSignalsButton = new QPushButton("− Remove Signals");

    // Style buttons
    QString buttonStyle = "QPushButton { padding: 6px; font-size: 11px; }";
    selectAllButton->setStyleSheet(buttonStyle);
    deselectAllButton->setStyleSheet(buttonStyle);
    addSignalsButton->setStyleSheet(buttonStyle + " QPushButton { background-color: #4CAF50; color: white; }");
    removeSignalsButton->setStyleSheet(buttonStyle + " QPushButton { background-color: #f44336; color: white; }");

    selectionLayout->addWidget(selectAllButton);
    selectionLayout->addWidget(deselectAllButton);
    selectionLayout->addWidget(addSignalsButton);
    selectionLayout->addWidget(removeSignalsButton);

    // Reordering buttons
    QHBoxLayout *reorderLayout = new QHBoxLayout();
    moveUpButton = new QPushButton("↑ Move Up");
    moveDownButton = new QPushButton("↓ Move Down");

    moveUpButton->setStyleSheet(buttonStyle);
    moveDownButton->setStyleSheet(buttonStyle);

    reorderLayout->addWidget(moveUpButton);
    reorderLayout->addWidget(moveDownButton);
    reorderLayout->addStretch();

    controlsLayout->addLayout(selectionLayout);
    controlsLayout->addLayout(reorderLayout);

    // Visible Signals Group
    QGroupBox *visibleGroup = new QGroupBox("Visible Signals");
    QVBoxLayout *visibleLayout = new QVBoxLayout(visibleGroup);

    visibleSignalsList = new QListWidget();
    visibleSignalsList->setAlternatingRowColors(true);
    visibleSignalsList->setSelectionMode(QListWidget::SingleSelection);

    connect(visibleSignalsList, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onVisibleSignalSelectionChanged);

    visibleLayout->addWidget(visibleSignalsList);

    // Add all groups to left panel
    leftLayout->addWidget(hierarchyGroup, 2);  // More space for hierarchy
    leftLayout->addWidget(controlsGroup);
    leftLayout->addWidget(visibleGroup, 1);    // Less space for visible list

    // === MAIN WAVEFORM AREA ===
    waveformWidget = new WaveformWidget();
    connect(waveformWidget, &WaveformWidget::timeChanged,
            this, &MainWindow::updateTimeDisplay);

    // Add panels to splitter
    mainSplitter->addWidget(leftPanel);
    mainSplitter->addWidget(waveformWidget);

    // Set splitter proportions
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    // Set initial sizes
    QList<int> sizes;
    sizes << 400 << 800;
    mainSplitter->setSizes(sizes);

    setCentralWidget(mainSplitter);

    // Connect new buttons
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllSignals);
    connect(deselectAllButton, &QPushButton::clicked, this, &MainWindow::deselectAllSignals);
    connect(addSignalsButton, &QPushButton::clicked, this, &MainWindow::addSelectedSignals);
    connect(removeSignalsButton, &QPushButton::clicked, this, &MainWindow::removeSelectedSignals);
    connect(moveUpButton, &QPushButton::clicked, this, &MainWindow::moveSignalUp);
    connect(moveDownButton, &QPushButton::clicked, this, &MainWindow::moveSignalDown);
}

void MainWindow::onVisibleSignalSelectionChanged()
{
    // Enable/disable reordering buttons based on selection
    bool hasSelection = visibleSignalsList->currentRow() >= 0;
    moveUpButton->setEnabled(hasSelection);
    moveDownButton->setEnabled(hasSelection);
    removeSignalsButton->setEnabled(hasSelection);
}

void MainWindow::moveSignalDown()
{
    int currentRow = visibleSignalsList->currentRow();
    QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;
    if (currentRow >= 0 && currentRow < currentSignals.size() - 1) {
        currentSignals.move(currentRow, currentRow + 1);
        waveformWidget->setVisibleSignals(currentSignals);
        refreshVisibleSignalsList();
        visibleSignalsList->setCurrentRow(currentRow + 1);
    }
}

void MainWindow::moveSignalUp()
{
    int currentRow = visibleSignalsList->currentRow();
    if (currentRow > 0) {
        QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;
        currentSignals.move(currentRow, currentRow - 1);
        waveformWidget->setVisibleSignals(currentSignals);
        refreshVisibleSignalsList();
        visibleSignalsList->setCurrentRow(currentRow - 1);
    }
}

void MainWindow::removeSelectedSignals()
{
    int currentRow = visibleSignalsList->currentRow();
    if (currentRow >= 0 && currentRow < waveformWidget->visibleSignals.size()) {
        QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;
        currentSignals.removeAt(currentRow);
        waveformWidget->setVisibleSignals(currentSignals);
        refreshVisibleSignalsList();
        statusLabel->setText("Signal removed");
    } else {
        statusLabel->setText("No signal selected to remove");
    }
}

void MainWindow::refreshVisibleSignalsList()
{
    visibleSignalsList->clear();
    for (const auto& signal : waveformWidget->visibleSignals) {
        QString displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
        visibleSignalsList->addItem(displayName);
    }

    // Update status
    statusLabel->setText(QString("%1 signal(s) displayed").arg(waveformWidget->visibleSignals.size()));
}

void MainWindow::addSelectedSignals()
{
    // Get all checked signals from tree
    QList<VCDSignal> signalsToAdd;
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>()) {
                VCDSignal signal = data.value<VCDSignal>();
                // Check if signal is already in visible list
                bool alreadyExists = false;
                for (const auto& visibleSignal : waveformWidget->visibleSignals) {
                    if (visibleSignal.identifier == signal.identifier) {
                        alreadyExists = true;
                        break;
                    }
                }
                if (!alreadyExists) {
                    signalsToAdd.append(signal);
                }
            }
        }
        ++it;
    }

    if (!signalsToAdd.isEmpty()) {
        // Add to waveform widget
        QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;
        currentSignals.append(signalsToAdd);
        waveformWidget->setVisibleSignals(currentSignals);

        // Update visible signals list
        refreshVisibleSignalsList();

        statusLabel->setText(QString("Added %1 signal(s)").arg(signalsToAdd.size()));
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
    // This method is now handled by the new button system
    // We'll keep it for checkbox changes but it won't update the waveform directly
    refreshVisibleSignalsList();
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
