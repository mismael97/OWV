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
#include <QtConcurrent>
#include <QMenuBar>
#include <QSettings>
#include <QListWidget>
#include <QDialogButtonBox>

// In the constructor, initialize history
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), vcdParser(new VCDParser(this)),
      rtlProcessedForSignalDialog(false)
{
    qRegisterMetaType<VCDSignal>("VCDSignal");
    setWindowTitle("VCD Wave Viewer");
    setMinimumSize(1200, 800);

    // NEW: Setup history file path
    historyFilePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(historyFilePath);
    historyFilePath += "/vcd_history.ini";

    createActions();
    setupUI();
    createMenuBar();
    createMainToolbar();
    setupNavigationControls();
    createStatusBar();

    // NEW: Load history and show startup dialog
    loadHistory();
    showStartupDialog();
}

void MainWindow::loadHistory()
{
    QSettings settings(historyFilePath, QSettings::IniFormat);
    recentFiles = settings.value("recentFiles").toStringList();
    
    // Remove non-existent files
    for (int i = recentFiles.size() - 1; i >= 0; --i) {
        if (!QFile::exists(recentFiles[i])) {
            recentFiles.removeAt(i);
        }
    }
    
    saveHistory(); // Save cleaned list
    updateRecentMenu();
}

void MainWindow::saveHistory()
{
    QSettings settings(historyFilePath, QSettings::IniFormat);
    settings.setValue("recentFiles", recentFiles);
}

// NEW: Add file to history
void MainWindow::addToHistory(const QString &filePath)
{
    // Remove if already in list
    recentFiles.removeAll(filePath);
    
    // Add to front
    recentFiles.prepend(filePath);
    
    // Limit to max recent files
    while (recentFiles.size() > MAX_RECENT_FILES) {
        recentFiles.removeLast();
    }
    
    saveHistory();
    updateRecentMenu();
}

// NEW: Update the Recent menu
void MainWindow::updateRecentMenu()
{
    if (!recentMenu) return;
    
    // Clear existing actions
    recentMenu->clear();
    
    if (recentFiles.isEmpty()) {
        QAction *noRecentAction = new QAction("No recent files", this);
        noRecentAction->setEnabled(false);
        recentMenu->addAction(noRecentAction);
    } else {
        for (const QString &filePath : recentFiles) {
            QFileInfo fileInfo(filePath);
            QString displayName = fileInfo.fileName();
            QString fullPath = fileInfo.absoluteFilePath();
            
            // Truncate if too long
            if (displayName.length() > 50) {
                displayName = displayName.left(47) + "...";
            }
            
            QAction *recentAction = new QAction(displayName, this);
            recentAction->setData(fullPath);
            recentAction->setToolTip(fullPath);
            
            connect(recentAction, &QAction::triggered, this, [this, fullPath]() {
                loadVcdFile(fullPath);
            });
            
            recentMenu->addAction(recentAction);
        }
        
        // Add separator and clear history action
        recentMenu->addSeparator();
        QAction *clearHistoryAction = new QAction("Clear History", this);
        connect(clearHistoryAction, &QAction::triggered, this, [this]() {
            recentFiles.clear();
            saveHistory();
            updateRecentMenu();
        });
        recentMenu->addAction(clearHistoryAction);
    }
}

// NEW: Show startup dialog with recent files
void MainWindow::showStartupDialog()
{
    if (recentFiles.isEmpty()) {
        statusLabel->setText("Use File → Open to load a VCD file");
        return;
    }
    
    // Create startup dialog
    QDialog startupDialog(this);
    startupDialog.setWindowTitle("VCD Wave Viewer - Recent Files");
    startupDialog.setMinimumWidth(500);
    
    QVBoxLayout *layout = new QVBoxLayout(&startupDialog);
    
    QLabel *titleLabel = new QLabel("Open Recent VCD File");
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold; margin: 10px;");
    layout->addWidget(titleLabel);
    
    QListWidget *fileList = new QListWidget();
    fileList->setAlternatingRowColors(true);
    
    for (const QString &filePath : recentFiles) {
        QFileInfo fileInfo(filePath);
        QString displayText = QString("%1\n%2")
            .arg(fileInfo.fileName())
            .arg(fileInfo.absolutePath());
        
        QListWidgetItem *item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, filePath);
        item->setToolTip(filePath);
        fileList->addItem(item);
    }
    
    layout->addWidget(fileList);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *openButton = new QPushButton("Open Selected");
    QPushButton *browseButton = new QPushButton("Browse...");
    QPushButton *cancelButton = new QPushButton("Cancel");
    
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(browseButton);
    buttonLayout->addWidget(cancelButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    connect(openButton, &QPushButton::clicked, &startupDialog, [&]() {
        QListWidgetItem *currentItem = fileList->currentItem();
        if (currentItem) {
            QString filePath = currentItem->data(Qt::UserRole).toString();
            startupDialog.accept();
            loadVcdFile(filePath);
        }
    });
    
    connect(browseButton, &QPushButton::clicked, &startupDialog, [&]() {
        startupDialog.accept();
        openFile();
    });
    
    connect(cancelButton, &QPushButton::clicked, &startupDialog, &QDialog::reject);
    
    connect(fileList, &QListWidget::itemDoubleClicked, &startupDialog, [&](QListWidgetItem *item) {
        QString filePath = item->data(Qt::UserRole).toString();
        startupDialog.accept();
        loadVcdFile(filePath);
    });
    
    // Show dialog
    if (startupDialog.exec() == QDialog::Rejected) {
        statusLabel->setText("Use File → Open to load a VCD file");
    }
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
    
    // NEW: Recent files submenu
    recentMenu = fileMenu->addMenu("Recent");  // ADD THIS - make sure to declare recentMenu in .h
    fileMenu->addSeparator();

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
    statusLabel->setText(QString("Signal height increased to %1").arg(waveformWidget->getSignalHeight()));
}

void MainWindow::decreaseSignalHeight()
{
    waveformWidget->setSignalHeight(waveformWidget->getSignalHeight() - 2);
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

void MainWindow::updateLineThicknessActions()
{
    lineThinAction->setChecked(false);
    lineMediumAction->setChecked(false);

    int currentWidth = waveformWidget->getLineWidth();
    if (currentWidth == 1)
        lineThinAction->setChecked(true);
    else if (currentWidth == 2)
        lineMediumAction->setChecked(true);
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
    removeSignalsButton->setEnabled(index >= 0);
    
    // Update navigation buttons based on new selection
    updateNavigationButtons(); });

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

    // NEW: Set up RTL processing for signal dialog
    dialog.setRtlProcessingInfo(this, currentVcdFilePath, rtlProcessedForSignalDialog, tempVcdFilePathForSignalDialog);

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
            int displayedCount = 0;
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

    // REMOVED: Don't clean up temp file here - keep it for the session
    // if (QFile::exists(tempVcdFilePathForSignalDialog)) {
    //     QFile::remove(tempVcdFilePathForSignalDialog);
    // }
    // rtlProcessedForSignalDialog = false; // Don't reset this either
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Clean up signal dialog temp file when application closes
    if (QFile::exists(tempVcdFilePathForSignalDialog)) {
        QFile::remove(tempVcdFilePathForSignalDialog);
        qDebug() << "Cleaned up signal dialog temp file:" << tempVcdFilePathForSignalDialog;
    }
    
    QMainWindow::closeEvent(event);
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
    QString defaultPath = "C:/Users/mismael/Desktop/OWV/test.vcd";

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

// Update openFile to handle the new flow
void MainWindow::openFile()
{
    QString filename = QFileDialog::getOpenFileName(
        this, "Open VCD File", "", "VCD Files (*.vcd)");

    if (!filename.isEmpty())
    {
        loadVcdFile(filename);
    }
}

void MainWindow::loadVcdFile(const QString &filename)
{
    // NEW: Add to history
    addToHistory(filename);
    
    // NEW: Clean up previous temp files when loading a new VCD file
    if (QFile::exists(tempVcdFilePathForSignalDialog)) {
        QFile::remove(tempVcdFilePathForSignalDialog);
        qDebug() << "Cleaned up previous signal dialog temp file:" << tempVcdFilePathForSignalDialog;
    }
    
    // Reset RTL processing state for the new file
    rtlProcessedForSignalDialog = false;
    
    // Store the original VCD file path
    currentVcdFilePath = filename;
    
    // Don't check for RTL during file loading - only in signal dialog
    QString vcdToLoad = filename;
    
    // Continue with VCD loading...
    statusBar()->clearMessage();

    // Create and show progress bar in status bar
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 0); // Indeterminate progress (spinning)
    progressBar->setMaximumWidth(200);
    progressBar->setTextVisible(false);
    statusBar()->addPermanentWidget(progressBar);

    statusLabel->setText("Loading VCD file...");

    // Disable UI during loading to prevent user interaction
    setEnabled(false);
    QApplication::processEvents(); // Force UI update

    // Use QtConcurrent to run parsing in background thread
    QFuture<bool> parseFuture = QtConcurrent::run([this, vcdToLoad]()
                                                  { return vcdParser->parseHeaderOnly(vcdToLoad); });

    // Create a watcher to handle completion
    QFutureWatcher<bool> *watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, progressBar, watcher, vcdToLoad]()
            {
        bool success = watcher->result();
        
        // Re-enable UI
        setEnabled(true);
        
        // Remove progress bar
        statusBar()->removeWidget(progressBar);
        delete progressBar;
        watcher->deleteLater();
        
        if (success) {
            QString statusMessage = QString("Loaded: %1 (%2 signals)").arg(QFileInfo(vcdToLoad).fileName()).arg(vcdParser->getSignals().size());
            statusLabel->setText(statusMessage);

            // Pass parser to waveform widget but don't load all signals
            waveformWidget->setVcdData(vcdParser);

            // Clear any existing signals from previous file
            waveformWidget->setVisibleSignals(QList<VCDSignal>());
            
            // NEW: Update window title to show current file
            setWindowTitle(QString("VCD Wave Viewer - %1").arg(QFileInfo(vcdToLoad).fileName()));
        } else {
            QMessageBox::critical(this, "Error",
                                  "Failed to parse VCD file: " + vcdParser->getError());
            statusLabel->setText("Ready");
        } });

    watcher->setFuture(parseFuture);
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

void MainWindow::setupNavigationControls()
{
    // Create navigation controls
    QWidget *navWidget = new QWidget();
    QHBoxLayout *navLayout = new QHBoxLayout(navWidget);
    navLayout->setContentsMargins(5, 0, 5, 0);
    navLayout->setSpacing(3); // Reduced spacing between widgets

    QLabel *navLabel = new QLabel("Navigate:");
    navigationModeCombo = new QComboBox();
    QFont boldFont;
    boldFont.setWeight(QFont::Bold);

    navigationModeCombo->addItem("⇄");
    navigationModeCombo->addItem("↱");
    navigationModeCombo->addItem("↳"); 

    // Set the font for each item
    navigationModeCombo->setItemData(0, boldFont, Qt::FontRole);
    navigationModeCombo->setItemData(1, boldFont, Qt::FontRole);
    navigationModeCombo->setItemData(2, boldFont, Qt::FontRole);
    
    // Make combo box smaller
    navigationModeCombo->setMaximumWidth(60);
    navigationModeCombo->setMaximumHeight(22);

    // Create smaller prev/next buttons
    prevValueButton = new QPushButton("◀");
    nextValueButton = new QPushButton("▶");
    
    // Set smaller button sizes
    prevValueButton->setFixedSize(22, 22);
    nextValueButton->setFixedSize(22, 22);
    
    // Set smaller font for buttons
    QFont smallFont = prevValueButton->font();
    smallFont.setPointSize(8);
    prevValueButton->setFont(smallFont);
    nextValueButton->setFont(smallFont);

    prevValueButton->setEnabled(false);
    nextValueButton->setEnabled(false);

    // Create time input field
    QLabel *timeLabel = new QLabel("Time:");
    QLineEdit *timeInput = new QLineEdit();
    
    // Set initial placeholder with current time (start with 0)
    timeInput->setPlaceholderText("Time: 0");
    
    timeInput->setMaximumWidth(80); // Slightly wider to fit "Time: 1234"
    timeInput->setMaximumHeight(22);
    
    // Set validator to accept only numbers
    QIntValidator *validator = new QIntValidator(0, 1000000000, this);
    timeInput->setValidator(validator);

    // Connect time input - when Enter is pressed, move cursor to that time
    connect(timeInput, &QLineEdit::returnPressed, this, [this, timeInput]() {
        bool ok;
        int time = timeInput->text().toInt(&ok);
        if (ok) {
            // Move cursor to the specified time
            waveformWidget->navigateToTime(time);
            // Update the time display
            updateTimeDisplay(time);
            // Clear the input field and immediately update placeholder
            timeInput->clear();
            timeInput->clearFocus(); // Remove focus so placeholder is visible
            
            // Force update the placeholder with the new time immediately
            QString timeText = QString("Time: %1").arg(time);
            timeInput->setPlaceholderText(timeText);
        }
    });

    // Connect to update the placeholder text with YELLOW TIMELINE CURSOR time
    connect(waveformWidget, &WaveformWidget::cursorTimeChanged, this, [timeInput](int time) {
        // Always update the placeholder to match the cursor time
        QString timeText = QString("Time: %1").arg(time);
        timeInput->setPlaceholderText(timeText);
    });

    // FIX: Use simpler connection syntax
    connect(navigationModeCombo, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onNavigationModeChanged(int)));
    connect(prevValueButton, &QPushButton::clicked, this, &MainWindow::onPrevValueClicked);
    connect(nextValueButton, &QPushButton::clicked, this, &MainWindow::onNextValueClicked);

    navLayout->addWidget(navLabel);
    navLayout->addWidget(navigationModeCombo);
    navLayout->addWidget(prevValueButton);
    navLayout->addWidget(nextValueButton);
    navLayout->addWidget(timeLabel);
    navLayout->addWidget(timeInput);
    navLayout->addStretch();

    // Add to main toolbar
    mainToolBar->addWidget(navWidget);
}


void MainWindow::onNavigationModeChanged(int index)
{
    // Only 3 modes now: 0=ValueChange, 1=SignalRise, 2=SignalFall
    if (index >= 0 && index <= 2) {
        waveformWidget->setNavigationMode(static_cast<WaveformWidget::NavigationMode>(index));
        updateNavigationButtons();
    }
}

void MainWindow::onPrevValueClicked()
{
    waveformWidget->navigateToPreviousEvent();
    updateNavigationButtons();
}

void MainWindow::onNextValueClicked()
{
    waveformWidget->navigateToNextEvent();
    updateNavigationButtons();
}

void MainWindow::updateNavigationButtons()
{
    bool hasSelection = !waveformWidget->getSelectedItemIndices().isEmpty();
    
    if (hasSelection) {
        bool hasPrev = waveformWidget->hasPreviousEvent();
        bool hasNext = waveformWidget->hasNextEvent();

        prevValueButton->setEnabled(hasPrev);
        nextValueButton->setEnabled(hasNext);
        
        qDebug() << "Navigation buttons - HasPrev:" << hasPrev << "HasNext:" << hasNext;
    } else {
        prevValueButton->setEnabled(false);
        nextValueButton->setEnabled(false);
    }
}

bool MainWindow::runVcdPortMapperForSignalDialog(const QString &inputVcd, const QString &outputVcd, const QString &rtlDir)
{
    // Get the path to the Python script
    QString pythonScript = QCoreApplication::applicationDirPath() + "/vcd_port_mapper.py";
    
    // If script doesn't exist in application dir, try current dir
    if (!QFile::exists(pythonScript)) {
        pythonScript = "vcd_port_mapper.py";
    }
    
    if (!QFile::exists(pythonScript)) {
        qDebug() << "VCD port mapper script not found:" << pythonScript;
        return false;
    }

    // Convert paths to absolute paths to avoid any relative path issues
    QString absInputVcd = QFileInfo(inputVcd).absoluteFilePath();
    QString absOutputVcd = QFileInfo(outputVcd).absoluteFilePath();
    QString absRtlDir = QFileInfo(rtlDir).absoluteFilePath();
    QString absPythonScript = QFileInfo(pythonScript).absoluteFilePath();

    qDebug() << "=== VCD PORT MAPPER EXECUTION ===";
    qDebug() << "Python script:" << absPythonScript;
    qDebug() << "Input VCD:" << absInputVcd;
    qDebug() << "Output VCD:" << absOutputVcd;
    qDebug() << "RTL Directory:" << absRtlDir;
    qDebug() << "RTL Directory exists:" << QDir(absRtlDir).exists();
    
    // Check if RTL directory exists and has files
    QDir rtlDirectory(absRtlDir);
    QStringList rtlFiles = rtlDirectory.entryList(QStringList() << "*.v" << "*.sv", QDir::Files);
    qDebug() << "RTL files found:" << rtlFiles.size();
    if (rtlFiles.size() > 0) {
        qDebug() << "First few RTL files:" << rtlFiles.mid(0, 5);
    }

    // Prepare the command - use absolute paths
    QStringList arguments;
    arguments << absPythonScript << absInputVcd << "-o" << absOutputVcd << "-r" << absRtlDir;
    
    qDebug() << "Command: python" << arguments;
    
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels); // Merge stdout and stderr
    
    // Connect to readyRead signal to capture output in real-time
    QObject::connect(&process, &QProcess::readyRead, [&process]() {
        QByteArray output = process.readAll();
        qDebug() << "Python output:" << output.trimmed();
    });
    
    process.start("python", arguments);
    
    if (!process.waitForStarted(5000)) {
        qDebug() << "Failed to start VCD port mapper process";
        qDebug() << "Error:" << process.errorString();
        return false;
    }
    
    qDebug() << "Process started successfully";
    
    if (!process.waitForFinished(60000)) { // 60 second timeout
        qDebug() << "VCD port mapper timed out";
        process.kill();
        return false;
    }
    
    int exitCode = process.exitCode();
    QByteArray allOutput = process.readAll();
    
    qDebug() << "Process finished with exit code:" << exitCode;
    qDebug() << "Final output:" << allOutput.trimmed();
    
    if (exitCode != 0) {
        qDebug() << "VCD port mapper failed with exit code:" << exitCode;
        return false;
    }
    
    // Verify the output file was created
    if (QFile::exists(absOutputVcd)) {
        QFile outputFile(absOutputVcd);
        if (outputFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Output file created successfully, size:" << outputFile.size() << "bytes";
            outputFile.close();
            
            // Quick check if the file has the expected content
            if (outputFile.size() > 100) { // Reasonable minimum size
                qDebug() << "VCD port mapper completed successfully";
                return true;
            } else {
                qDebug() << "Output file seems too small, might be empty";
                return false;
            }
        }
    } else {
        qDebug() << "Output file was not created";
        return false;
    }
    
    qDebug() << "VCD port mapper completed successfully";
    return true;
}

QString MainWindow::findRtlDirectoryForSignalDialog(const QString &vcdFile)
{
    QFileInfo vcdInfo(vcdFile);
    QDir vcdDir = vcdInfo.dir();
    
    qDebug() << "=== FINDING RTL DIRECTORY ===";
    qDebug() << "VCD file:" << vcdFile;
    qDebug() << "VCD directory:" << vcdDir.absolutePath();
    
    // First, check if there's a directory that contains RTL files
    // We'll look for directories that have Verilog/SystemVerilog files
    
    QDir parentDir = vcdDir;
    
    // Look in the VCD file directory and its subdirectories
    QStringList searchDirs;
    searchDirs << vcdDir.absolutePath(); // Current directory
    
    // Add all immediate subdirectories
    QFileInfoList subdirs = vcdDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subdir : subdirs) {
        searchDirs << subdir.absoluteFilePath();
    }
    
    qDebug() << "Searching in directories:" << searchDirs;
    
    for (const QString &searchDir : searchDirs) {
        QDir dir(searchDir);
        QStringList rtlFiles = dir.entryList(QStringList() << "*.v" << "*.sv", QDir::Files);
        
        if (!rtlFiles.isEmpty()) {
            qDebug() << "Found RTL directory:" << searchDir << "with" << rtlFiles.size() << "RTL files";
            qDebug() << "Sample RTL files:" << rtlFiles.mid(0, 3); // Show first 3 files
            return searchDir;
        } else {
            qDebug() << "No RTL files found in:" << searchDir;
        }
    }
    
    qDebug() << "No RTL directory found automatically";
    return "";
}

bool MainWindow::processVcdWithRtlForSignalDialog(const QString &vcdFile)
{
    QString rtlDir = findRtlDirectoryForSignalDialog(vcdFile);
    if (rtlDir.isEmpty()) {
        qDebug() << "No RTL directory found for signal dialog";
        return false;
    }
    
    // Create temp VCD file path for signal dialog
    QFileInfo fileInfo(vcdFile);
    tempVcdFilePathForSignalDialog = fileInfo.path() + "/" + fileInfo.completeBaseName() + "_temp_signal_dialog.vcd";
    
    qDebug() << "Processing VCD with RTL for signal dialog:";
    qDebug() << "  Input VCD:" << vcdFile;
    qDebug() << "  RTL Dir:" << rtlDir;
    qDebug() << "  Output VCD:" << tempVcdFilePathForSignalDialog;
    
    bool success = runVcdPortMapperForSignalDialog(vcdFile, tempVcdFilePathForSignalDialog, rtlDir);
    
    if (success) {
        // Verify the output file was created
        if (QFile::exists(tempVcdFilePathForSignalDialog)) {
            QFile file(tempVcdFilePathForSignalDialog);
            if (file.open(QIODevice::ReadOnly)) {
                qDebug() << "RTL processing successful, output file size:" << file.size() << "bytes";
                file.close();
            }
        }
    } else {
        qDebug() << "RTL processing failed";
    }
    
    return success;
}

void MainWindow::showRtlDirectoryDialogForSignalDialog()
{
    QString rtlDir = QFileDialog::getExistingDirectory(this, "Select RTL Directory for Signal Filtering",
                                                      QFileInfo(currentVcdFilePath).dir().path());
    
    if (!rtlDir.isEmpty()) {
        // Reprocess VCD with the new RTL directory for signal dialog
        if (runVcdPortMapperForSignalDialog(currentVcdFilePath, tempVcdFilePathForSignalDialog, rtlDir)) {
            rtlProcessedForSignalDialog = true;
        }
    }
}

// You can add this check somewhere in your initialization
void checkPythonAvailability()
{
    QProcess process;
    process.start("python", QStringList() << "--version");
    if (process.waitForFinished(5000)) {
        QByteArray output = process.readAll();
        qDebug() << "Python version:" << output.trimmed();
    } else {
        qDebug() << "Python not found or not working";
    }
    
    // Also check python3
    process.start("python3", QStringList() << "--version");
    if (process.waitForFinished(5000)) {
        QByteArray output = process.readAll();
        qDebug() << "Python3 version:" << output.trimmed();
    }
}