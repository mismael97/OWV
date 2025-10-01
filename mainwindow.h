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
#include <QGroupBox>
#include <QDockWidget>
#include "vcdparser.h"
#include "waveformwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void signalSelectionChanged();
    void updateTimeDisplay(int time);
    void about();
    void onSignalItemChanged(QTreeWidgetItem *item, int column);
    void selectAllSignals();
    void deselectAllSignals();
    void addSelectedSignals();
    void removeSelectedSignal();
    void showSignalHierarchy();  // Add this
    void hideSignalHierarchy();  // Add this

private:
    void createActions();
    void createToolBar();
    void createStatusBar();
    void setupUI();
    void loadVcdFile(const QString &filename);
    void loadDefaultVcdFile();
    void populateSignalTree();
    void updateVisibleSignals();
    void setAllSignalsCheckState(Qt::CheckState state);

    // UI Components
    QSplitter *mainSplitter;
    QTreeWidget *signalTree;
    WaveformWidget *waveformWidget;
    QScrollBar *timeScrollBar;

    // Toolbar Actions
    QAction *openAction;
    QAction *zoomInAction;
    QAction *zoomOutAction;
    QAction *zoomFitAction;
    QAction *aboutAction;
    QAction *showHierarchyAction;  // Add this

    // Signal selection buttons
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QPushButton *addSignalsButton;
    QPushButton *removeSignalButton;  // Changed from removeSignalsButton

    // Dock for signal hierarchy
    QDockWidget *hierarchyDock;  // Add this

    // Status Bar
    QLabel *statusLabel;
    QLabel *timeLabel;

    // Data
    VCDParser *vcdParser;
};

#endif // MAINWINDOW_H
