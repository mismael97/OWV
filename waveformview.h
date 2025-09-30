#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QWidget>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include "vcdparser.h"

class WaveformView : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformView(QWidget *parent = nullptr);
    void clear();
    void setSignalData(const VCDParser::Signal& signal, double timeScale);
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void drawWaveform(QPainter& painter);
    void drawGrid(QPainter& painter);
    void updateScrollBars();

    VCDParser::Signal m_signal;
    double m_timeScale;
    bool m_hasSignal;

    double m_pixelsPerTimeUnit;
    double m_timeOffset;
    int m_verticalOffset;

    bool m_dragging;
    QPoint m_lastMousePos;

    QScrollBar* m_horizontalScrollBar;
    QScrollBar* m_verticalScrollBar;
};

#endif // WAVEFORMVIEW_H
