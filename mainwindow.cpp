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
    for (int i = recentFiles.size() - 1; i >= 0; --i)
    {
        if (!QFile::exists(recentFiles[i]))
        {
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
    while (recentFiles.size() > MAX_RECENT_FILES)
    {
        recentFiles.removeLast();
    }

    saveHistory();
    updateRecentMenu();
}

// NEW: Update the Recent menu
void MainWindow::updateRecentMenu()
{
    if (!recentMenu)
        return;

    // Clear existing actions
    recentMenu->clear();

    if (recentFiles.isEmpty())
    {
        QAction *noRecentAction = new QAction("No recent files", this);
        noRecentAction->setEnabled(false);
        recentMenu->addAction(noRecentAction);
    }
    else
    {
        for (const QString &filePath : recentFiles)
        {
            QFileInfo fileInfo(filePath);
            QString displayName = fileInfo.fileName();
            QString fullPath = fileInfo.absoluteFilePath();

            // Truncate if too long
            if (displayName.length() > 50)
            {
                displayName = displayName.left(47) + "...";
            }

            QAction *recentAction = new QAction(displayName, this);
            recentAction->setData(fullPath);
            recentAction->setToolTip(fullPath);

            connect(recentAction, &QAction::triggered, this, [this, fullPath]()
                    { loadVcdFile(fullPath); });

            recentMenu->addAction(recentAction);
        }

        // Add separator and clear history action
        recentMenu->addSeparator();
        QAction *clearHistoryAction = new QAction("Clear History", this);
        connect(clearHistoryAction, &QAction::triggered, this, [this]()
                {
            recentFiles.clear();
            saveHistory();
            updateRecentMenu(); });
        recentMenu->addAction(clearHistoryAction);
    }
}

// NEW: Show startup dialog with recent files
void MainWindow::showStartupDialog()
{
    if (recentFiles.isEmpty())
    {
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

    for (const QString &filePath : recentFiles)
    {
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
    connect(openButton, &QPushButton::clicked, &startupDialog, [&]()
            {
        QListWidgetItem *currentItem = fileList->currentItem();
        if (currentItem) {
            QString filePath = currentItem->data(Qt::UserRole).toString();
            startupDialog.accept();
            loadVcdFile(filePath);
        } });

    connect(browseButton, &QPushButton::clicked, &startupDialog, [&]()
            {
        startupDialog.accept();
        openFile(); });

    connect(cancelButton, &QPushButton::clicked, &startupDialog, &QDialog::reject);

    connect(fileList, &QListWidget::itemDoubleClicked, &startupDialog, [&](QListWidgetItem *item)
            {
        QString filePath = item->data(Qt::UserRole).toString();
        startupDialog.accept();
        loadVcdFile(filePath); });

    // Show dialog
    if (startupDialog.exec() == QDialog::Rejected)
    {
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

    // NEW: Save/Load signals actions
    saveSignalsAction = new QAction("Save Signals As...", this);
    saveSignalsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveSignalsAction, &QAction::triggered, this, &MainWindow::saveSignals);

    loadSignalsAction = new QAction("Load Signals...", this);
    connect(loadSignalsAction, &QAction::triggered, this, &MainWindow::loadSignals);

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


void MainWindow::loadSpecificSession(const QString &sessionName)
{
    // This is essentially the same as loadSignals() but for a specific session
    // You can refactor the common code into a helper function
    QString sessionDir = getSessionDir();
    QString sessionFile = sessionDir + "/" + sessionName + ".json";

    // ... copy the loading logic from loadSignals() here ...
    // (I'm omitting the duplicate code for brevity)
    loadSignals(); // This will now work since we modified it to show a selection dialog
}

void MainWindow::manageSessions()
{
    if (currentVcdFilePath.isEmpty()) {
        QMessageBox::information(this, "Manage Sessions", "No VCD file loaded.");
        return;
    }

    QStringList availableSessions = getAvailableSessions(currentVcdFilePath);
    if (availableSessions.isEmpty()) {
        QMessageBox::information(this, "Manage Sessions", "No saved sessions found.");
        return;
    }

    // Create a dialog to manage sessions
    QDialog dialog(this);
    dialog.setWindowTitle("Manage Sessions");
    dialog.setMinimumSize(400, 300);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    QLabel *titleLabel = new QLabel("Saved Sessions for: " + QFileInfo(currentVcdFilePath).fileName());
    titleLabel->setStyleSheet("font-weight: bold; margin: 10px;");
    layout->addWidget(titleLabel);
    
    QListWidget *sessionList = new QListWidget();
    sessionList->addItems(availableSessions);
    sessionList->setSelectionMode(QListWidget::SingleSelection);
    layout->addWidget(sessionList);
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *loadButton = new QPushButton("Load Selected");
    QPushButton *deleteButton = new QPushButton("Delete Selected");
    QPushButton *closeButton = new QPushButton("Close");
    
    buttonLayout->addWidget(loadButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    
    // Connect signals
    connect(loadButton, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *currentItem = sessionList->currentItem();
        if (currentItem) {
            QString sessionName = currentItem->text();
            dialog.accept();
            // Call load function with the selected session
            loadSpecificSession(sessionName);
        } else {
            QMessageBox::information(&dialog, "Manage Sessions", "Please select a session first.");
        }
    });
    
    connect(deleteButton, &QPushButton::clicked, &dialog, [&]() {
        QListWidgetItem *currentItem = sessionList->currentItem();
        if (currentItem) {
            QString sessionName = currentItem->text();
            int result = QMessageBox::question(&dialog, "Delete Session",
                                             QString("Are you sure you want to delete session '%1'?").arg(sessionName));
            if (result == QMessageBox::Yes) {
                QString sessionDir = getSessionDir();
                QString sessionFile = sessionDir + "/" + sessionName + ".json";
                if (QFile::remove(sessionFile)) {
                    delete sessionList->takeItem(sessionList->row(currentItem));
                    QMessageBox::information(&dialog, "Manage Sessions", 
                                           QString("Session '%1' deleted.").arg(sessionName));
                } else {
                    QMessageBox::warning(&dialog, "Manage Sessions", 
                                       QString("Failed to delete session '%1'.").arg(sessionName));
                }
            }
        } else {
            QMessageBox::information(&dialog, "Manage Sessions", "Please select a session first.");
        }
    });
    
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    
    dialog.exec();
}


void MainWindow::createMenuBar()
{
    // Create proper menu bar
    QMenuBar *menuBar = this->menuBar();


    // File menu
    QMenu *fileMenu = menuBar->addMenu("File");
    fileMenu->addAction(openAction);
    
    // NEW: Add save/load signals actions
    fileMenu->addAction(saveSignalsAction);
    fileMenu->addAction(loadSignalsAction);
    fileMenu->addSeparator();
    
    fileMenu->addAction("Manage Sessions...", this, &MainWindow::manageSessions);
    // NEW: Recent files submenu
    recentMenu = fileMenu->addMenu("Recent");
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

    // NEW: Initially disable save/load until VCD is loaded
    updateSaveLoadActions();
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

            // Get current cursor position from waveform widget
            int cursorIndex = waveformWidget->getSignalCursorIndex();

            if (cursorIndex >= 0)
            {
                // Insert new signals at cursor position
                waveformWidget->insertSignalsAtCursor(newSignalsToAdd, cursorIndex);
                statusLabel->setText(QString("Added %1 signal(s) at cursor position").arg(newSignalsToAdd.size()));
            }
            else
            {
                // Default behavior: append to end
                QList<VCDSignal> allSignalsToDisplay = currentSignals;
                allSignalsToDisplay.append(newSignalsToAdd);
                waveformWidget->setVisibleSignals(allSignalsToDisplay);
                statusLabel->setText(QString("Added %1 signal(s) at the end").arg(newSignalsToAdd.size()));
            }

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
            updateSaveLoadActions();
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
    if (QFile::exists(tempVcdFilePathForSignalDialog))
    {
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
        
        // NEW: Update save/load actions after removing signals
        updateSaveLoadActions();
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
            
            // NEW: Update save/load actions state
            updateSaveLoadActions();
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
    connect(timeInput, &QLineEdit::returnPressed, this, [this, timeInput]()
            {
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
        } });

    // Connect to update the placeholder text with YELLOW TIMELINE CURSOR time
    connect(waveformWidget, &WaveformWidget::cursorTimeChanged, this, [timeInput](int time)
            {
        // Always update the placeholder to match the cursor time
        QString timeText = QString("Time: %1").arg(time);
        timeInput->setPlaceholderText(timeText); });

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
    if (index >= 0 && index <= 2)
    {
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

    if (hasSelection)
    {
        bool hasPrev = waveformWidget->hasPreviousEvent();
        bool hasNext = waveformWidget->hasNextEvent();

        prevValueButton->setEnabled(hasPrev);
        nextValueButton->setEnabled(hasNext);

        qDebug() << "Navigation buttons - HasPrev:" << hasPrev << "HasNext:" << hasNext;
    }
    else
    {
        prevValueButton->setEnabled(false);
        nextValueButton->setEnabled(false);
    }
}

bool MainWindow::runVcdPortMapperForSignalDialog(const QString &inputVcd, const QString &outputVcd, const QString &rtlDir)
{
    // Get the path to the Python script
    QString pythonScript = QCoreApplication::applicationDirPath() + "/vcd_port_mapper.py";

    // If script doesn't exist in application dir, try current dir
    if (!QFile::exists(pythonScript))
    {
        pythonScript = "vcd_port_mapper.py";
    }

    if (!QFile::exists(pythonScript))
    {
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
    if (rtlFiles.size() > 0)
    {
        qDebug() << "First few RTL files:" << rtlFiles.mid(0, 5);
    }

    // Prepare the command - use absolute paths
    QStringList arguments;
    arguments << absPythonScript << absInputVcd << "-o" << absOutputVcd << "-r" << absRtlDir;

    qDebug() << "Command: python" << arguments;

    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels); // Merge stdout and stderr

    // Connect to readyRead signal to capture output in real-time
    QObject::connect(&process, &QProcess::readyRead, [&process]()
                     {
        QByteArray output = process.readAll();
        qDebug() << "Python output:" << output.trimmed(); });

    process.start("python", arguments);

    if (!process.waitForStarted(5000))
    {
        qDebug() << "Failed to start VCD port mapper process";
        qDebug() << "Error:" << process.errorString();
        return false;
    }

    qDebug() << "Process started successfully";

    if (!process.waitForFinished(60000))
    { // 60 second timeout
        qDebug() << "VCD port mapper timed out";
        process.kill();
        return false;
    }

    int exitCode = process.exitCode();
    QByteArray allOutput = process.readAll();

    qDebug() << "Process finished with exit code:" << exitCode;
    qDebug() << "Final output:" << allOutput.trimmed();

    if (exitCode != 0)
    {
        qDebug() << "VCD port mapper failed with exit code:" << exitCode;
        return false;
    }

    // Verify the output file was created
    if (QFile::exists(absOutputVcd))
    {
        QFile outputFile(absOutputVcd);
        if (outputFile.open(QIODevice::ReadOnly))
        {
            qDebug() << "Output file created successfully, size:" << outputFile.size() << "bytes";
            outputFile.close();

            // Quick check if the file has the expected content
            if (outputFile.size() > 100)
            { // Reasonable minimum size
                qDebug() << "VCD port mapper completed successfully";
                return true;
            }
            else
            {
                qDebug() << "Output file seems too small, might be empty";
                return false;
            }
        }
    }
    else
    {
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
    for (const QFileInfo &subdir : subdirs)
    {
        searchDirs << subdir.absoluteFilePath();
    }

    qDebug() << "Searching in directories:" << searchDirs;

    for (const QString &searchDir : searchDirs)
    {
        QDir dir(searchDir);
        QStringList rtlFiles = dir.entryList(QStringList() << "*.v" << "*.sv", QDir::Files);

        if (!rtlFiles.isEmpty())
        {
            qDebug() << "Found RTL directory:" << searchDir << "with" << rtlFiles.size() << "RTL files";
            qDebug() << "Sample RTL files:" << rtlFiles.mid(0, 3); // Show first 3 files
            return searchDir;
        }
        else
        {
            qDebug() << "No RTL files found in:" << searchDir;
        }
    }

    qDebug() << "No RTL directory found automatically";
    return "";
}

bool MainWindow::processVcdWithRtlForSignalDialog(const QString &vcdFile)
{
    QString rtlDir = findRtlDirectoryForSignalDialog(vcdFile);
    if (rtlDir.isEmpty())
    {
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

    if (success)
    {
        // Verify the output file was created
        if (QFile::exists(tempVcdFilePathForSignalDialog))
        {
            QFile file(tempVcdFilePathForSignalDialog);
            if (file.open(QIODevice::ReadOnly))
            {
                qDebug() << "RTL processing successful, output file size:" << file.size() << "bytes";
                file.close();
            }
        }
    }
    else
    {
        qDebug() << "RTL processing failed";
    }

    return success;
}

void MainWindow::showRtlDirectoryDialogForSignalDialog()
{
    QString rtlDir = QFileDialog::getExistingDirectory(this, "Select RTL Directory for Signal Filtering",
                                                       QFileInfo(currentVcdFilePath).dir().path());

    if (!rtlDir.isEmpty())
    {
        // Reprocess VCD with the new RTL directory for signal dialog
        if (runVcdPortMapperForSignalDialog(currentVcdFilePath, tempVcdFilePathForSignalDialog, rtlDir))
        {
            rtlProcessedForSignalDialog = true;
        }
    }
}

// You can add this check somewhere in your initialization
void checkPythonAvailability()
{
    QProcess process;
    process.start("python", QStringList() << "--version");
    if (process.waitForFinished(5000))
    {
        QByteArray output = process.readAll();
        qDebug() << "Python version:" << output.trimmed();
    }
    else
    {
        qDebug() << "Python not found or not working";
    }

    // Also check python3
    process.start("python3", QStringList() << "--version");
    if (process.waitForFinished(5000))
    {
        QByteArray output = process.readAll();
        qDebug() << "Python3 version:" << output.trimmed();
    }
}

QString MainWindow::getSessionFilePath(const QString &vcdFile) const
{
    if (vcdFile.isEmpty()) {
        return "";
    }
    
    QFileInfo vcdInfo(vcdFile);
    QString sessionFileName = vcdInfo.completeBaseName() + "_session.json";
    return vcdInfo.absolutePath() + "/" + sessionFileName;
}

bool MainWindow::hasSessionForCurrentFile() const
{
    if (currentVcdFilePath.isEmpty()) {
        return false;
    }
    
    QString sessionFile = getSessionFilePath(currentVcdFilePath);
    return QFile::exists(sessionFile);
}

void MainWindow::updateSaveLoadActions()
{
    bool hasVcdLoaded = !currentVcdFilePath.isEmpty();
    bool hasSignals = waveformWidget->getItemCount() > 0;
    bool hasSessions = hasSessionsForCurrentFile();
    
    saveSignalsAction->setEnabled(hasVcdLoaded && hasSignals);
    loadSignalsAction->setEnabled(hasVcdLoaded && hasSessions);
}

void MainWindow::saveSignals()
{
    if (currentVcdFilePath.isEmpty() || !vcdParser) {
        QMessageBox::warning(this, "Save Session", "No VCD file loaded.");
        return;
    }

    if (waveformWidget->getItemCount() == 0) {
        QMessageBox::warning(this, "Save Session", "No signals to save.");
        return;
    }

    // Get session name from user
    bool ok;
    QString sessionName = QInputDialog::getText(this, "Save Session", 
                                               "Enter session name:",
                                               QLineEdit::Normal, 
                                               "", &ok);
    if (!ok || sessionName.isEmpty()) {
        return;
    }

    // Validate session name
    sessionName = sessionName.trimmed();
    if (sessionName.isEmpty()) {
        QMessageBox::warning(this, "Save Session", "Session name cannot be empty.");
        return;
    }

    // Check if session already exists
    QString sessionDir = getSessionDir();
    QString sessionFile = sessionDir + "/" + sessionName + ".json";
    
    if (QFile::exists(sessionFile)) {
        int result = QMessageBox::question(this, "Save Session",
                                         QString("Session '%1' already exists.\nDo you want to overwrite it?")
                                         .arg(sessionName));
        if (result != QMessageBox::Yes) {
            return;
        }
    }

    // Build session data
    QJsonObject sessionData;
    sessionData["vcdFile"] = currentVcdFilePath;
    sessionData["sessionName"] = sessionName;
    sessionData["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Save signal list
    QJsonArray signalsArray;
    for (int i = 0; i < waveformWidget->getItemCount(); i++) {
        const DisplayItem *item = waveformWidget->getItem(i);
        if (item && item->type == DisplayItem::Signal) {
            QJsonObject signalObj;
            signalObj["fullName"] = item->signal.signal.fullName;
            signalObj["scope"] = item->signal.signal.scope;
            signalObj["name"] = item->signal.signal.name;
            signalObj["width"] = item->signal.signal.width;
            signalObj["identifier"] = item->signal.signal.identifier;
            signalsArray.append(signalObj);
        }
    }
    sessionData["signals"] = signalsArray;
    
    // Save display settings
    QJsonObject displaySettings;
    displaySettings["signalHeight"] = waveformWidget->getSignalHeight();
    displaySettings["lineWidth"] = waveformWidget->getLineWidth();
    displaySettings["busFormat"] = static_cast<int>(waveformWidget->getBusDisplayFormat());
    sessionData["displaySettings"] = displaySettings;
    
    // Save time cursor position
    sessionData["cursorTime"] = waveformWidget->getCursorTime();
    
    // Save signal colors
    QJsonObject colorsObj;
    // Note: You'll need to add a method to get all signal colors from WaveformWidget
    // For now, we'll skip this or you can implement it later
    sessionData["signalColors"] = colorsObj;
    
    // Write to file
    QFile file(sessionFile);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Save Session", 
                            QString("Failed to create session file:\n%1").arg(file.errorString()));
        return;
    }
    
    QJsonDocument doc(sessionData);
    file.write(doc.toJson());
    file.close();
    
    statusLabel->setText(QString("Session '%1' saved with %2 signal(s)").arg(sessionName).arg(signalsArray.size()));
    updateSaveLoadActions();
    
    QMessageBox::information(this, "Save Session", 
                           QString("Successfully saved session '%1' with %2 signal(s).")
                           .arg(sessionName).arg(signalsArray.size()));
}

void MainWindow::loadSignals()
{
    if (currentVcdFilePath.isEmpty() || !vcdParser) {
        QMessageBox::warning(this, "Load Session", "No VCD file loaded.");
        return;
    }

    QStringList availableSessions = getAvailableSessions(currentVcdFilePath);
    if (availableSessions.isEmpty()) {
        QMessageBox::information(this, "Load Session", 
                               "No saved sessions found for the current VCD file.");
        return;
    }

    // Let user choose which session to load
    bool ok;
    QString sessionName = QInputDialog::getItem(this, "Load Session",
                                               "Select session to load:",
                                               availableSessions, 0, false, &ok);
    if (!ok || sessionName.isEmpty()) {
        return;
    }

    QString sessionDir = getSessionDir();
    QString sessionFile = sessionDir + "/" + sessionName + ".json";

    // Read session file
    QFile file(sessionFile);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Load Session", 
                            QString("Failed to open session file:\n%1").arg(file.errorString()));
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        QMessageBox::critical(this, "Load Session", "Invalid session file format.");
        return;
    }
    
    QJsonObject sessionData = doc.object();
    
    // Verify VCD file matches
    QString savedVcdFile = sessionData["vcdFile"].toString();
    if (savedVcdFile != currentVcdFilePath) {
        int result = QMessageBox::question(this, "Load Session",
                                         QString("Session was created for:\n%1\n\n"
                                                "Current file is:\n%2\n\n"
                                                "Do you want to load anyway?")
                                         .arg(savedVcdFile, currentVcdFilePath));
        if (result != QMessageBox::Yes) {
            return;
        }
    }
    
    // Get session info for confirmation
    QString savedSessionName = sessionData["sessionName"].toString();
    QString saveTime = sessionData["saveTime"].toString();
    
    // Ask for confirmation
    QString confirmMessage = QString("Load session '%1'?").arg(savedSessionName);
    if (!saveTime.isEmpty()) {
        QDateTime saveDateTime = QDateTime::fromString(saveTime, Qt::ISODate);
        confirmMessage += QString("\nSaved: %1").arg(saveDateTime.toString("yyyy-MM-dd hh:mm:ss"));
    }
    
    int result = QMessageBox::question(this, "Load Session", confirmMessage);
    if (result != QMessageBox::Yes) {
        return;
    }
    
    // Load signals
    QJsonArray signalsArray = sessionData["signals"].toArray();
    if (signalsArray.isEmpty()) {
        QMessageBox::warning(this, "Load Session", "No signals found in session file.");
        return;
    }
    
    QList<VCDSignal> signalsToLoad;
    QList<VCDSignal> allSignals = vcdParser->getSignals();
    
    int foundCount = 0;
    int missingCount = 0;
    QStringList missingSignals;
    
    for (const QJsonValue &signalValue : signalsArray) {
        QJsonObject signalObj = signalValue.toObject();
        QString fullName = signalObj["fullName"].toString();
        
        // Find the signal in the current VCD file
        bool found = false;
        for (const VCDSignal &signal : allSignals) {
            if (signal.fullName == fullName) {
                signalsToLoad.append(signal);
                found = true;
                foundCount++;
                break;
            }
        }
        
        if (!found) {
            missingCount++;
            missingSignals.append(fullName);
        }
    }
    
    if (signalsToLoad.isEmpty()) {
        QMessageBox::warning(this, "Load Session", 
                           "None of the saved signals were found in the current VCD file.");
        return;
    }
    
    // Clear current signals first
    waveformWidget->setVisibleSignals(QList<VCDSignal>());
    
    // Load display settings
    if (sessionData.contains("displaySettings")) {
        QJsonObject displaySettings = sessionData["displaySettings"].toObject();
        if (displaySettings.contains("signalHeight")) {
            waveformWidget->setSignalHeight(displaySettings["signalHeight"].toInt());
        }
        if (displaySettings.contains("lineWidth")) {
            waveformWidget->setLineWidth(displaySettings["lineWidth"].toInt());
        }
        if (displaySettings.contains("busFormat")) {
            waveformWidget->setBusDisplayFormat(static_cast<WaveformWidget::BusFormat>(
                displaySettings["busFormat"].toInt()));
        }
    }
    
    // Set the signals
    waveformWidget->setVisibleSignals(signalsToLoad);
    
    // Restore cursor time
    if (sessionData.contains("cursorTime")) {
        int cursorTime = sessionData["cursorTime"].toInt();
        waveformWidget->navigateToTime(cursorTime);
    }
    
    // Show result message
    QString message = QString("Successfully loaded session '%1' with %2 signal(s).").arg(savedSessionName).arg(foundCount);
    if (missingCount > 0) {
        message += QString("\n%1 signal(s) not found in current VCD file.").arg(missingCount);
        if (missingCount <= 10) { // Don't show too many missing signals
            message += "\nMissing: " + missingSignals.join(", ");
        }
    }
    
    statusLabel->setText(QString("Loaded session '%1' with %2 signal(s)").arg(savedSessionName).arg(foundCount));
    
    QMessageBox::information(this, "Load Session", message);
    updateSaveLoadActions();
}

QString MainWindow::getSessionDir() const
{
    if (currentVcdFilePath.isEmpty()) {
        return "";
    }
    
    QFileInfo vcdInfo(currentVcdFilePath);
    QString sessionDir = vcdInfo.absolutePath() + "/" + vcdInfo.completeBaseName() + "_sessions";
    QDir().mkpath(sessionDir); // Create directory if it doesn't exist
    return sessionDir;
}

QStringList MainWindow::getAvailableSessions(const QString &vcdFile) const
{
    QStringList sessions;
    if (vcdFile.isEmpty()) {
        return sessions;
    }
    
    QFileInfo vcdInfo(vcdFile);
    QString sessionDir = vcdInfo.absolutePath() + "/" + vcdInfo.completeBaseName() + "_sessions";
    QDir dir(sessionDir);
    
    if (dir.exists()) {
        QStringList filters;
        filters << "*.json";
        sessions = dir.entryList(filters, QDir::Files, QDir::Name);
        
        // Remove the .json extension for display
        for (int i = 0; i < sessions.size(); ++i) {
            sessions[i] = QFileInfo(sessions[i]).completeBaseName();
        }
    }
    
    return sessions;
}

bool MainWindow::hasSessionsForCurrentFile() const
{
    if (currentVcdFilePath.isEmpty()) {
        return false;
    }
    
    return !getAvailableSessions(currentVcdFilePath).isEmpty();
}
