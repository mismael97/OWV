#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QTreeWidget>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include "vcdparser.h"
#include "waveformview.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openVCDFile();
    void onVCDLoaded();
    void onSignalSelected(const QString& signalName);
    void onSignalTreeSelectionChanged();
    void zoomIn();
    void zoomOut();
    void resetZoom();

private:
    void createActions();
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void setupUI();

    VCDParser* m_parser;
    WaveformView* m_waveformView;
    QTreeWidget* m_signalTree;
    QSplitter* m_splitter;

    QAction* m_openAct;
    QAction* m_exitAct;
    QAction* m_zoomInAct;
    QAction* m_zoomOutAct;
    QAction* m_resetZoomAct;

    QLabel* m_statusLabel;
};

#endif // MAINWINDOW_H
