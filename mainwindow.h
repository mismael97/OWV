#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QSplitter>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QComboBox>
#include "vcdparser.h"
#include "waveformwidget.h"
#include <QProcess>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <QFileSystemWatcher>

#include <QRegularExpression>
#include <QInputDialog>

#include <QRadioButton>
#include <QLineEdit>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QGroupBox>

class ValueSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ValueSearchDialog(QWidget *parent = nullptr);

    QString getSearchValue() const { return valueEdit->text(); }
    int getSearchFormat() const { return formatGroup->checkedId(); }
    void setLastValues(const QString &value, int format);

private:

    QString convertToBinaryStrict(const QString &value, int signalWidth, int format) const;
    QLineEdit *valueEdit;
    QButtonGroup *formatGroup;
    QRadioButton *autoRadio;
    QRadioButton *binaryRadio;
    QRadioButton *hexRadio;
    QRadioButton *decimalRadio;
    QRadioButton *octalRadio;
};

class SignalSelectionDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // NEW: Make these methods public so SignalSelectionDialog can access them
    bool hasRtlDirectoryForSignalDialog();
    QString findRtlDirectoryForSignalDialog(const QString &vcdFile);
    bool processVcdWithRtlForSignalDialog(const QString &vcdFile);
    bool runVcdPortMapperForSignalDialog(const QString &inputVcd, const QString &outputVcd, const QString &rtlDir);
    void showRtlDirectoryDialogForSignalDialog();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override; // ADD override

private slots:
    void searchSignalValue(); // NEW: Search for signal values
    void findNextValue();     // NEW: Find next occurrence
    void findPreviousValue(); // NEW: Find previous occurrence
    void clearValueSearch();  // NEW: Clear value search highlights

    void updateSaveLoadActions();
    void saveSignals();
    void loadSignals();
    void refreshVcd();
    void onVcdFileChanged(const QString &path); // NEW: Handle file changes

    void setLineThicknessThin();
    void setLineThicknessMedium();
    void openFile();
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void updateTimeDisplay(int time);
    void about();
    void showAddSignalsDialog();
    void removeSelectedSignals();
    void toggleBusDisplayFormat();
    void resetSignalColors();
    void setBusHexFormat();
    void setBusBinaryFormat();
    void setBusOctalFormat();
    void setBusDecimalFormat();
    void updateBusFormatActions();
    void increaseSignalHeight();
    void decreaseSignalHeight();

    // NEW SLOTS:
    void onNavigationModeChanged(int index);
    void onPrevValueClicked();
    void onNextValueClicked();

private:
    unsigned long long convertToNumeric(const QString &value, int format) const;
QString convertToBinaryStrict(const QString &value, int signalWidth, int format) const;

    // NEW: Value search members
    QAction *searchValueAction;
    QAction *findNextValueAction;
    QAction *findPreviousValueAction;
    QAction *clearValueSearchAction;

    struct ValueSearchMatch
    {
        QString signalName;
        int timestamp;
        QString value;
        int signalIndex;
    };

    QList<ValueSearchMatch> valueSearchMatches;
    int currentSearchMatchIndex;
    QString lastSearchValue;
    int lastSearchFormat; // NEW: Remember last search format

    void performValueSearch(const QString &searchValue, int searchFormat); // FIXED: Added searchFormat parameter
    QString convertToBinary(const QString &value, int signalWidth) const;
    bool matchesSearchValue(const QString &signalValue, const QString &searchValue, int signalWidth, int searchFormat) const; // FIXED: Added searchFormat parameter
    void highlightSearchMatch(int matchIndex);

    // NEW: Search format constants
    enum SearchFormat
    {
        FormatAuto = 0,
        FormatBinary = 1,
        FormatHex = 2,
        FormatDecimal = 3,
        FormatOctal = 4
    };

    void checkForVcdUpdates();
    void manageSessions();
    void loadSpecificSession(const QString &sessionName);
    // Add these new actions
    QAction *saveSignalsAction;
    QAction *loadSignalsAction;
    QAction *refreshVcdAction;

    // Add these helper methods
    QString getSessionDir() const;
    QStringList getAvailableSessions(const QString &vcdFile) const;
    bool hasSessionsForCurrentFile() const;
    void reloadVcdData();

    QFileSystemWatcher *fileWatcher; // NEW: Monitor VCD file changes
    QTimer *refreshTimer;            // NEW: Debounce timer for file changes

    // Add these helper methods
    QString getSessionFilePath(const QString &vcdFile) const;
    bool hasSessionForCurrentFile() const;
    void loadHistory();
    void saveHistory();
    void addToHistory(const QString &filePath);
    void updateRecentMenu();
    void showStartupDialog();

    QString historyFilePath;
    QStringList recentFiles;
    const int MAX_RECENT_FILES = 10;

    QMenu *recentMenu; // ADD THIS

    QString currentVcdFilePath;

    // NEW: Track RTL processing state for signal dialog
    bool rtlProcessedForSignalDialog;
    QString tempVcdFilePathForSignalDialog;

    bool processVcdWithRtl(const QString &vcdFile);
    QString findRtlDirectory(const QString &vcdFile);
    bool runVcdPortMapper(const QString &inputVcd, const QString &outputVcd, const QString &rtlDir);

    void showRtlDirectoryDialog();
    void createToolbarBelowMenu();
    void updateLineThicknessActions();
    QAction *increaseHeightAction;
    QAction *decreaseHeightAction;

    QMenu *lineThicknessMenu;
    QAction *lineThinAction;
    QAction *lineMediumAction;
    QAction *lineThickAction;
    // Wave menu actions
    QMenu *waveMenu;
    QAction *defaultColorsAction;
    QMenu *busFormatMenu;
    QAction *busHexAction;
    QAction *busBinaryAction;
    QAction *busOctalAction;
    QAction *busDecimalAction;
    QAction *resetColorsAction;
    void createActions();
    void createToolBar();
    void createStatusBar();
    void setupUI();
    void loadVcdFile(const QString &filename);
    void loadDefaultVcdFile();

    void createMenuBar();
    void createMainToolbar();
    void setupNavigationControls();
    void updateNavigationButtons();

    // Add these to private section
    QToolBar *mainToolBar;
    QLineEdit *searchField;

    // Navigation controls
    QComboBox *navigationModeCombo;
    QPushButton *prevValueButton;
    QPushButton *nextValueButton;

    // UI Components
    WaveformWidget *waveformWidget;
    QScrollBar *timeScrollBar;

    // Toolbar Actions
    QAction *openAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomFitAction;
    QAction *aboutAction;

    // Bottom controls
    QPushButton *addSignalsButton;
    QPushButton *removeSignalsButton;

    // Status Bar
    QLabel *statusLabel;
    QLabel *timeLabel;

    // Data
    VCDParser *vcdParser;
};

#endif // MAINWINDOW_H