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
    void openFile();
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void updateTimeDisplay(int time);
    void about();
    void showAddSignalsDialog();
    void removeSelectedSignals();
    void toggleBusDisplayFormat();

private:
    void createActions();
    void createToolBar();
    void createStatusBar();
    void setupUI();
    void loadVcdFile(const QString &filename);
    void loadDefaultVcdFile();
    QAction *busHexAction;
    QAction *busBinaryAction;

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
