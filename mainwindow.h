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
#include <QComboBox>  // ADD THIS
#include "vcdparser.h"
#include "waveformwidget.h"

class SignalSelectionDialog;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
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
    void setupNavigationControls();  // ADD THIS
    void updateNavigationButtons();  // ADD THIS

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