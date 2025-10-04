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
#include <QToolButton>
#include <QKeyEvent>
#include <QMenuBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), vcdParser(new VCDParser(this))
{
    qRegisterMetaType<VCDSignal>("VCDSignal");
    setWindowTitle("VCD Wave Viewer");
    setMinimumSize(1200, 800);

    createActions();
    setupUI();
    createMenuBar();
    createMainToolbar();
    createStatusBar();

    loadDefaultVcdFile();
}

MainWindow::~MainWindow()
{
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Delete)
    {
        // Let the waveform widget handle deletion if it has focus
        if (waveformWidget->hasFocus() && !waveformWidget->getSelectedItemIndices().isEmpty())
        {
            waveformWidget->removeSelectedSignals();
            event->accept();
        }
        else
        {
            // Fall back to the main window's delete handling
            removeSelectedSignals();
            event->accept();
        }
    }
    else if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier)
    {
        waveformWidget->selectAllSignals();
        removeSignalsButton->setEnabled(true);
        event->accept();
    }
    else
    {
        QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::createActions()
{
    openAction = new QAction("Open", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);

    zoomInAction = new QAction("Zoom In", this);
    zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

    resetColorsAction = new QAction("Reset Colors", this);
    connect(resetColorsAction, &QAction::triggered, this, &MainWindow::resetSignalColors);

    zoomOutAction = new QAction("Zoom Out", this);
    zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

    zoomFitAction = new QAction("Zoom Fit", this);
    connect(zoomFitAction, &QAction::triggered, this, &MainWindow::zoomFit);

    aboutAction = new QAction("About", this);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);

    // Wave menu actions
    defaultColorsAction = new QAction("Default Colors", this);
    connect(defaultColorsAction, &QAction::triggered, this, &MainWindow::resetSignalColors);

    highlightBussesAction = new QAction("Highlight Busses", this);
    highlightBussesAction->setCheckable(true);
    connect(highlightBussesAction, &QAction::triggered, this, &MainWindow::toggleHighlightBusses);

    // Bus format actions
    busHexAction = new QAction("Hexadecimal", this);
    busHexAction->setCheckable(true);
    busHexAction->setChecked(true);
    connect(busHexAction, &QAction::triggered, this, &MainWindow::setBusHexFormat);

    busBinaryAction = new QAction("Binary", this);
    busBinaryAction->setCheckable(true);
    connect(busBinaryAction, &QAction::triggered, this, &MainWindow::setBusBinaryFormat);

    busOctalAction = new QAction("Octal", this);
    busOctalAction->setCheckable(true);
    connect(busOctalAction, &QAction::triggered, this, &MainWindow::setBusOctalFormat);

    busDecimalAction = new QAction("Decimal", this);
    busDecimalAction->setCheckable(true);
    connect(busDecimalAction, &QAction::triggered, this, &MainWindow::setBusDecimalFormat);

    // Line thickness actions
    lineThinAction = new QAction("Thin (1px)", this);
    lineThinAction->setCheckable(true);
    connect(lineThinAction, &QAction::triggered, this, &MainWindow::setLineThicknessThin);

    lineMediumAction = new QAction("Medium (2px)", this);
    lineMediumAction->setCheckable(true);
    lineMediumAction->setChecked(true);
    connect(lineMediumAction, &QAction::triggered, this, &MainWindow::setLineThicknessMedium);

    lineThickAction = new QAction("Thick (3px)", this);
    lineThickAction->setCheckable(true);
    connect(lineThickAction, &QAction::triggered, this, &MainWindow::setLineThicknessThick);

    // Signal height adjustment actions
    increaseHeightAction = new QAction("Increase Signal Height", this);
    increaseHeightAction->setShortcut(QKeySequence("Ctrl+Up"));
    connect(increaseHeightAction, &QAction::triggered, this, &MainWindow::increaseSignalHeight);

    decreaseHeightAction = new QAction("Decrease Signal Height", this);
    decreaseHeightAction->setShortcut(QKeySequence("Ctrl+Down"));
    connect(decreaseHeightAction, &QAction::triggered, this, &MainWindow::decreaseSignalHeight);
}

void MainWindow::createMenuBar()
{
    // Create proper menu bar
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("File");
    fileMenu->addAction(openAction);

    // Edit menu (empty for now)
    QMenu *editMenu = menuBar->addMenu("Edit");

    // View menu
    QMenu *viewMenu = menuBar->addMenu("View");
    viewMenu->addAction(zoomInAction);
    viewMenu->addAction(zoomOutAction);
    viewMenu->addAction(zoomFitAction);

    // Workspace menu (empty for now)
    QMenu *workspaceMenu = menuBar->addMenu("Workspace");

    // Wave menu with submenus
    QMenu *waveMenu = menuBar->addMenu("Wave");
    waveMenu->addAction(increaseHeightAction);
    waveMenu->addAction(decreaseHeightAction);
    waveMenu->addSeparator();

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("Help");
    helpMenu->addAction(aboutAction);

    // Signal colors submenu
    QMenu *signalColorsMenu = waveMenu->addMenu("Signal Colors");
    signalColorsMenu->addAction(defaultColorsAction);
    signalColorsMenu->addAction(highlightBussesAction);

    // Bus format submenu
    busFormatMenu = waveMenu->addMenu("Bus Format");
    busFormatMenu->addAction(busHexAction);
    busFormatMenu->addAction(busBinaryAction);
    busFormatMenu->addAction(busOctalAction);
    busFormatMenu->addAction(busDecimalAction);

    // Line thickness submenu
    lineThicknessMenu = waveMenu->addMenu("Line Thickness");
    lineThicknessMenu->addAction(lineThinAction);
    lineThicknessMenu->addAction(lineMediumAction);
    lineThicknessMenu->addAction(lineThickAction);
}

void MainWindow::createMainToolbar()
{
    // Create main toolbar that appears below the menu bar
    mainToolBar = addToolBar("Main Toolbar");
    mainToolBar->setObjectName("MainToolbar");
    mainToolBar->setMovable(false);
    mainToolBar->setIconSize(QSize(16, 16));

    // Search field
    QLabel *searchLabel = new QLabel("Search:");
    searchField = new QLineEdit();
    searchField->setPlaceholderText("Search signals...");
    searchField->setMaximumWidth(200);
    searchField->setClearButtonEnabled(true);

    // Connect search field to waveform widget search functionality
    connect(searchField, &QLineEdit::textChanged, this, [this](const QString &text)
            { waveformWidget->searchSignals(text); });

    // Zoom controls
    QAction *zoomInToolbarAction = new QAction("🔍+", this);
    zoomInToolbarAction->setToolTip("Zoom In");
    connect(zoomInToolbarAction, &QAction::triggered, this, &MainWindow::zoomIn);

    QAction *zoomOutToolbarAction = new QAction("🔍-", this);
    zoomOutToolbarAction->setToolTip("Zoom Out");
    connect(zoomOutToolbarAction, &QAction::triggered, this, &MainWindow::zoomOut);

    QAction *zoomFitToolbarAction = new QAction("⤢ Fit", this);
    zoomFitToolbarAction->setToolTip("Zoom to Fit");
    connect(zoomFitToolbarAction, &QAction::triggered, this, &MainWindow::zoomFit);

    // Add widgets to toolbar
    mainToolBar->addWidget(searchLabel);
    mainToolBar->addWidget(searchField);
    mainToolBar->addSeparator();

    // Zoom controls
    mainToolBar->addAction(zoomInToolbarAction);
    mainToolBar->addAction(zoomOutToolbarAction);
    mainToolBar->addAction(zoomFitToolbarAction);

    // Add some spacing and stretch
    mainToolBar->addSeparator();
    QWidget *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainToolBar->addWidget(spacer);
}

void MainWindow::increaseSignalHeight()
{
    waveformWidget->setSignalHeight(waveformWidget->getSignalHeight() + 2);
    waveformWidget->setBusHeight(waveformWidget->getBusHeight() + 2);
    statusLabel->setText(QString("Signal height increased to %1").arg(waveformWidget->getSignalHeight()));
}

void MainWindow::decreaseSignalHeight()
{
    waveformWidget->setSignalHeight(waveformWidget->getSignalHeight() - 2);
    waveformWidget->setBusHeight(waveformWidget->getBusHeight() - 2);
    statusLabel->setText(QString("Signal height decreased to %1").arg(waveformWidget->getSignalHeight()));
}

void MainWindow::resetSignalColors()
{
    waveformWidget->resetSignalColors();
}

void MainWindow::setLineThicknessThin()
{
    waveformWidget->setLineWidth(1);
    updateLineThicknessActions();
}

void MainWindow::setLineThicknessMedium()
{
    waveformWidget->setLineWidth(2);
    updateLineThicknessActions();
}

void MainWindow::setLineThicknessThick()
{
    waveformWidget->setLineWidth(3);
    updateLineThicknessActions();
}

void MainWindow::updateLineThicknessActions()
{
    lineThinAction->setChecked(false);
    lineMediumAction->setChecked(false);
    lineThickAction->setChecked(false);

    int currentWidth = waveformWidget->getLineWidth();
    if (currentWidth == 1)
        lineThinAction->setChecked(true);
    else if (currentWidth == 2)
        lineMediumAction->setChecked(true);
    else if (currentWidth == 3)
        lineThickAction->setChecked(true);
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

    // Create waveform widget
    waveformWidget = new WaveformWidget();
    connect(waveformWidget, &WaveformWidget::timeChanged,
            this, &MainWindow::updateTimeDisplay);
    connect(waveformWidget, &WaveformWidget::itemSelected, this, [this](int index)
            {
        // Enable/disable remove button based on selection
        removeSignalsButton->setEnabled(index >= 0); });

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
    if (!vcdParser)
        return;

    int signalCount = vcdParser->getSignals().size();

    // Show immediate feedback for large files
    if (signalCount > 10000)
    {
        statusLabel->setText(QString("Loading signal selection dialog (%1 signals)...").arg(signalCount));
        QApplication::processEvents();

        // Use a simple message box for very large files
        if (signalCount > 50000)
        {
            QMessageBox::information(this, "Large File",
                                     QString("This file contains %1 signals.\n\n"
                                             "The signal selection will load in batches for better performance.\n"
                                             "Use the search filter to find specific signals quickly.")
                                         .arg(signalCount));
        }
    }

    SignalSelectionDialog dialog(this);

    // Get current signals
    QList<VCDSignal> currentSignals;
    for (int i = 0; i < waveformWidget->getItemCount(); i++)
    {
        const DisplayItem *item = waveformWidget->getItem(i);
        if (item && item->type == DisplayItem::Signal)
        {
            currentSignals.append(item->signal.signal);
        }
    }

    // Set signals and show dialog
    dialog.setAvailableSignals(vcdParser->getSignals(), currentSignals);

    int displayedCount;
    if (dialog.exec() == QDialog::Accepted)
    {
        QList<VCDSignal> newSignalsToAdd = dialog.getSelectedSignals();
        if (!newSignalsToAdd.isEmpty())
        {
            statusLabel->setText(QString("Loading %1 signals...").arg(newSignalsToAdd.size()));
            QApplication::processEvents();

            // Add to current signals
            QList<VCDSignal> allSignalsToDisplay = currentSignals;
            allSignalsToDisplay.append(newSignalsToAdd);

            waveformWidget->setVisibleSignals(allSignalsToDisplay);

            // Update status
            displayedCount = 0;
            for (int i = 0; i < waveformWidget->getItemCount(); i++)
            {
                const DisplayItem *item = waveformWidget->getItem(i);
                if (item && item->type == DisplayItem::Signal)
                {
                    displayedCount++;
                }
            }

            statusLabel->setText(QString("%1 signal(s) displayed").arg(displayedCount));
            removeSignalsButton->setEnabled(false);
        }
    }

    statusLabel->setText(QString("%1 signal(s) displayed").arg(displayedCount));
}

void MainWindow::removeSelectedSignals()
{
    // Check if there are any selected items in the waveform widget
    if (!waveformWidget->getSelectedItemIndices().isEmpty())
    {
        waveformWidget->removeSelectedSignals();
        removeSignalsButton->setEnabled(false);

        // Count only signals for display (not spaces)
        int signalCount = 0;
        for (int i = 0; i < waveformWidget->getItemCount(); i++)
        {
            const DisplayItem *item = waveformWidget->getItem(i);
            if (item && item->type == DisplayItem::Signal)
            {
                signalCount++;
            }
        }

        statusLabel->setText(QString("%1 signal(s) displayed").arg(signalCount));
    }
}

void MainWindow::loadDefaultVcdFile()
{
    QString defaultPath = "F:/OWV/test.vcd";

    if (QFile::exists(defaultPath))
    {
        loadVcdFile(defaultPath);
    }
    else
    {
        statusLabel->setText("Default VCD file not found. Use File → Open to load a VCD file.");
        qDebug() << "Default VCD file not found:" << defaultPath;
    }
}

void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Open VCD File", "C:/Users/mismael/Desktop/OWV", "VCD Files (*.vcd)");

    if (!filename.isEmpty())
    {
        loadVcdFile(filename);
    }
}

void MainWindow::loadVcdFile(const QString &filename)
{
    statusLabel->setText("Loading VCD file...");
    QApplication::processEvents();

    // Use header-only parsing for fast loading
    if (vcdParser->parseHeaderOnly(filename))
    {
        statusLabel->setText(QString("Loaded: %1 (%2 signals) - Ready to display")
                                 .arg(QFileInfo(filename).fileName())
                                 .arg(vcdParser->getSignals().size()));

        // Pass parser to waveform widget but don't load all signals
        waveformWidget->setVcdData(vcdParser);

        // Clear any existing signals from previous file
        waveformWidget->setVisibleSignals(QList<VCDSignal>());
    }
    else
    {
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

void MainWindow::toggleBusDisplayFormat()
{
    if (sender() == busHexAction)
    {
        waveformWidget->setBusDisplayFormat(WaveformWidget::Hex);
        busHexAction->setChecked(true);
        busBinaryAction->setChecked(false);
    }
    else if (sender() == busBinaryAction)
    {
        waveformWidget->setBusDisplayFormat(WaveformWidget::Binary);
        busHexAction->setChecked(false);
        busBinaryAction->setChecked(true);
    }
}

void MainWindow::toggleHighlightBusses()
{
    waveformWidget->setHighlightBusses(highlightBussesAction->isChecked());
}

void MainWindow::setBusHexFormat()
{
    waveformWidget->setBusDisplayFormat(WaveformWidget::Hex);
    updateBusFormatActions();
}

void MainWindow::setBusBinaryFormat()
{
    waveformWidget->setBusDisplayFormat(WaveformWidget::Binary);
    updateBusFormatActions();
}

void MainWindow::setBusOctalFormat()
{
    waveformWidget->setBusDisplayFormat(WaveformWidget::Octal);
    updateBusFormatActions();
}

void MainWindow::setBusDecimalFormat()
{
    waveformWidget->setBusDisplayFormat(WaveformWidget::Decimal);
    updateBusFormatActions();
}

void MainWindow::updateBusFormatActions()
{
    busHexAction->setChecked(false);
    busBinaryAction->setChecked(false);
    busOctalAction->setChecked(false);
    busDecimalAction->setChecked(false);

    switch (waveformWidget->getBusDisplayFormat())
    {
    case WaveformWidget::Hex:
        busHexAction->setChecked(true);
        break;
    case WaveformWidget::Binary:
        busBinaryAction->setChecked(true);
        break;
    case WaveformWidget::Octal:
        busOctalAction->setChecked(true);
        break;
    case WaveformWidget::Decimal:
        busDecimalAction->setChecked(true);
        break;
    }
}