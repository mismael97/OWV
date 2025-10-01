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
#include <QDockWidget>

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

    showHierarchyAction = new QAction("Show Signal Hierarchy", this);  // Add this
    showHierarchyAction->setCheckable(true);
    showHierarchyAction->setChecked(true);
    connect(showHierarchyAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            showSignalHierarchy();
        } else {
            hideSignalHierarchy();
        }
    });

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
    toolBar->addAction(showHierarchyAction);  // Add this
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
    // === MAIN SPLITTER ===
    mainSplitter = new QSplitter(Qt::Horizontal, this);

    // === SIGNAL NAMES COLUMN (like Verdi) ===
    QWidget *signalNamesWidget = new QWidget();
    QVBoxLayout *signalNamesLayout = new QVBoxLayout(signalNamesWidget);
    signalNamesLayout->setContentsMargins(2, 2, 2, 2);
    signalNamesLayout->setSpacing(2);

    // Signal names header
    QLabel *namesHeader = new QLabel("Signals");
    namesHeader->setStyleSheet("QLabel { font-weight: bold; padding: 4px; background-color: #f0f0f0; }");
    namesHeader->setAlignment(Qt::AlignCenter);

    // Remove signal button
    QHBoxLayout *removeButtonLayout = new QHBoxLayout();
    removeSignalButton = new QPushButton("− Remove Selected");
    removeSignalButton->setStyleSheet("QPushButton { padding: 4px; font-size: 10px; background-color: #f44336; color: white; }");
    removeSignalButton->setEnabled(false);
    connect(removeSignalButton, &QPushButton::clicked, this, &MainWindow::removeSelectedSignal);

    removeButtonLayout->addWidget(removeSignalButton);
    removeButtonLayout->addStretch();

    signalNamesLayout->addWidget(namesHeader);
    signalNamesLayout->addLayout(removeButtonLayout);
    signalNamesLayout->addStretch(); // Names will be added by waveform widget

    // === WAVEFORM WIDGET ===
    waveformWidget = new WaveformWidget();
    connect(waveformWidget, &WaveformWidget::timeChanged,
            this, &MainWindow::updateTimeDisplay);
    // Connect signal for when user selects a signal in waveform
    connect(waveformWidget, &WaveformWidget::signalSelected,
            this, [this](int index) {
                removeSignalButton->setEnabled(index >= 0);
            });

    // Add to main splitter
    mainSplitter->addWidget(signalNamesWidget);
    mainSplitter->addWidget(waveformWidget);

    // Set splitter proportions (signal names column fixed width)
    mainSplitter->setStretchFactor(0, 0);
    mainSplitter->setStretchFactor(1, 1);

    // Set initial sizes
    QList<int> sizes;
    sizes << 200 << 1000; // Signal names: 200px, Waveform: 1000px
    mainSplitter->setSizes(sizes);

    setCentralWidget(mainSplitter);

    // === SIGNAL HIERARCHY DOCK ===
    hierarchyDock = new QDockWidget("Signal Hierarchy", this);
    hierarchyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget *hierarchyWidget = new QWidget();
    QVBoxLayout *hierarchyLayout = new QVBoxLayout(hierarchyWidget);
    hierarchyLayout->setContentsMargins(5, 5, 5, 5);

    // Signal tree
    signalTree = new QTreeWidget();
    signalTree->setHeaderLabel("Modules & Signals");
    signalTree->setMinimumWidth(300);
    signalTree->setAlternatingRowColors(true);

    connect(signalTree, &QTreeWidget::itemSelectionChanged,
            this, &MainWindow::signalSelectionChanged);
    connect(signalTree, &QTreeWidget::itemChanged,
            this, &MainWindow::onSignalItemChanged);

    // Hierarchy controls
    QHBoxLayout *hierarchyControlsLayout = new QHBoxLayout();
    selectAllButton = new QPushButton("Select All");
    deselectAllButton = new QPushButton("Deselect All");
    addSignalsButton = new QPushButton("+ Add to Waveform");

    QString buttonStyle = "QPushButton { padding: 6px; font-size: 11px; }";
    selectAllButton->setStyleSheet(buttonStyle);
    deselectAllButton->setStyleSheet(buttonStyle);
    addSignalsButton->setStyleSheet(buttonStyle + " QPushButton { background-color: #4CAF50; color: white; }");

    hierarchyControlsLayout->addWidget(selectAllButton);
    hierarchyControlsLayout->addWidget(deselectAllButton);
    hierarchyControlsLayout->addWidget(addSignalsButton);
    hierarchyControlsLayout->addStretch();

    hierarchyLayout->addLayout(hierarchyControlsLayout);
    hierarchyLayout->addWidget(signalTree);

    hierarchyDock->setWidget(hierarchyWidget);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);

    // Connect hierarchy buttons
    connect(selectAllButton, &QPushButton::clicked, this, &MainWindow::selectAllSignals);
    connect(deselectAllButton, &QPushButton::clicked, this, &MainWindow::deselectAllSignals);
    connect(addSignalsButton, &QPushButton::clicked, this, &MainWindow::addSelectedSignals);
}

void MainWindow::showSignalHierarchy()
{
    hierarchyDock->show();
}

void MainWindow::hideSignalHierarchy()
{
    hierarchyDock->hide();
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
    // Handle signal selection in hierarchy if needed
}

void MainWindow::onSignalItemChanged(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    // Handle checkbox changes if needed
}

void MainWindow::updateVisibleSignals()
{
    // Handle visible signals update
    statusLabel->setText(QString("%1 signal(s) displayed").arg(waveformWidget->visibleSignals.size()));
}

void MainWindow::setAllSignalsCheckState(Qt::CheckState state)
{
    disconnect(signalTree, &QTreeWidget::itemChanged, this, &MainWindow::onSignalItemChanged);

    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            (*it)->setCheckState(0, state);
        }
        ++it;
    }

    connect(signalTree, &QTreeWidget::itemChanged, this, &MainWindow::onSignalItemChanged);
}

void MainWindow::selectAllSignals()
{
    setAllSignalsCheckState(Qt::Checked);
}

void MainWindow::deselectAllSignals()
{
    setAllSignalsCheckState(Qt::Unchecked);
}

void MainWindow::addSelectedSignals()
{
    QList<VCDSignal> signalsToAdd;
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>()) {
                VCDSignal signal = data.value<VCDSignal>();
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
        QList<VCDSignal> currentSignals = waveformWidget->visibleSignals;
        currentSignals.append(signalsToAdd);
        waveformWidget->setVisibleSignals(currentSignals);
        statusLabel->setText(QString("Added %1 signal(s)").arg(signalsToAdd.size()));

        // Hide hierarchy after adding signals
        hideSignalHierarchy();
        showHierarchyAction->setChecked(false);
    }
}

void MainWindow::removeSelectedSignal()
{
    waveformWidget->removeSelectedSignal();
    removeSignalButton->setEnabled(false);
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
                       "- Verdi-like interface with separate signal names column\n"
                       "- Dockable signal hierarchy\n"
                       "- Drag to reorder signals in waveform\n"
                       "- Direct signal selection and deletion");
}
