#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QVector>
#include <QList>
#include <QLabel>
#include <QMenu>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "vcdparser.h"

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);
    void setVcdData(VCDParser *parser);
    void setVisibleSignals(const QList<VCDSignal> &visibleSignals);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void removeSelectedSignals();
    int getSelectedSignal() const { return selectedSignal; }
    int signalHeight = 30;

    QList<VCDSignal> visibleSignals;

signals:
    void timeChanged(int time);
    void signalSelected(int signalIndex);
    void contextMenuRequested(const QPoint &pos, int signalIndex);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void drawSignalNamesColumn(QPainter &painter);
    void drawWaveformArea(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawSignals(QPainter &painter);
    void drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void updateScrollBar();
    int timeToX(int time) const;
    int xToTime(int x) const;
    QString getSignalValueAtTime(const QString &identifier, int time) const;
    int calculateTimeStep(int startTime, int endTime) const;
    int getSignalAtPosition(const QPoint &pos) const;
    void startDragSignal(int signalIndex);
    void performDrag(int mouseY);
    void showContextMenu(const QPoint &pos, int signalIndex);
    void addSpaceAbove();
    void addSpaceBelow();
    void addGroup();

    VCDParser *vcdParser;

    // Layout parameters
    int signalNamesWidth = 250;  // Fixed width for signal names column
    double timeScale;
    int timeOffset;
    int timeMarkersHeight;
    int topMargin;

    bool isDragging;
    bool isDraggingSignal;
    int dragStartX;
    int dragStartOffset;
    int dragSignalIndex;
    int dragStartY;

    int selectedSignal;

    QScrollBar *horizontalScrollBar;
};

#endif // WAVEFORMWIDGET_H
