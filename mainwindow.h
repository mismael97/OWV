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

private:
    void createActions();
    void createToolBar();
    void createStatusBar();
    void setupUI();
    void loadVcdFile(const QString &filename);
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

    // Signal selection buttons
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;

    // Status Bar
    QLabel *statusLabel;
    QLabel *timeLabel;

    // Data
    VCDParser *vcdParser;
};

#endif // MAINWINDOW_H
