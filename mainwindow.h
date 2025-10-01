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
    void addSelectedSignals();           // Add this
    void removeSelectedSignals();        // Add this
    void moveSignalUp();                 // Add this
    void moveSignalDown();               // Add this
    void onVisibleSignalSelectionChanged(); // Add this

private:
    void createActions();
    void createToolBar();
    void createStatusBar();
    void setupUI();
    void loadVcdFile(const QString &filename);
    void loadDefaultVcdFile();           // Add this
    void populateSignalTree();
    void updateVisibleSignals();
    void setAllSignalsCheckState(Qt::CheckState state);
    void refreshVisibleSignalsList();    // Add this

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

    // Signal selection buttons
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QPushButton *addSignalsButton;       // Add this
    QPushButton *removeSignalsButton;    // Add this
    QPushButton *moveUpButton;           // Add this
    QPushButton *moveDownButton;         // Add this

    QListWidget *visibleSignalsList;     // Add this

    // Status Bar
    QLabel *statusLabel;
    QLabel *timeLabel;

    // Data
    VCDParser *vcdParser;
};

#endif // MAINWINDOW_H
