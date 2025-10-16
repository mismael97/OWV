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
    void saveSignals();
    void loadSignals();
    void updateSaveLoadActions();
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
    // Add these new actions
    QAction *saveSignalsAction;
    QAction *loadSignalsAction;

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