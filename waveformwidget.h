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
    void removeSelectedSignal();

    QList<VCDSignal> visibleSignals;

signals:
    void timeChanged(int time);
    void signalSelected(int signalIndex);  // Add this

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void drawGrid(QPainter &painter);
    void drawSignals(QPainter &painter);
    void drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void updateScrollBar();
    int timeToX(int time) const;
    int xToTime(int x) const;
    QString getSignalValueAtTime(const QString &identifier, int time) const;
    int calculateTimeStep(int startTime, int endTime) const;
    int getSignalAtPosition(const QPoint &pos) const;  // Add this
    void startDragSignal(int signalIndex);  // Add this
    void performDrag(int mouseY);  // Add this

    VCDParser *vcdParser;

    double timeScale;
    int timeOffset;
    int signalHeight;
    int timeMarkersHeight;
    int leftMargin;
    int topMargin;

    bool isDragging;
    bool isDraggingSignal;  // Add this
    int dragStartX;
    int dragStartOffset;
    int dragSignalIndex;    // Add this
    int dragStartY;         // Add this

    int selectedSignal;     // Add this

    QScrollBar *horizontalScrollBar;
};

#endif // WAVEFORMWIDGET_H
