#include "waveformview.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QFontMetrics>

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
    , m_timeScale(1.0)
    , m_hasSignal(false)
    , m_pixelsPerTimeUnit(10.0)
    , m_timeOffset(0.0)
    , m_verticalOffset(0)
    , m_dragging(false)
    , m_horizontalScrollBar(new QScrollBar(Qt::Horizontal, this))
    , m_verticalScrollBar(new QScrollBar(Qt::Vertical, this))
{
    m_horizontalScrollBar->setRange(0, 1000);
    m_verticalScrollBar->setRange(0, 100);

    connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_timeOffset = value / m_pixelsPerTimeUnit;
        update();
    });

    connect(m_verticalScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_verticalOffset = value;
        update();
    });

    QHBoxLayout* hLayout = new QHBoxLayout;
    hLayout->addWidget(m_horizontalScrollBar);
    hLayout->setContentsMargins(0, 0, 0, 0);

    QVBoxLayout* vLayout = new QVBoxLayout(this);
    vLayout->addLayout(hLayout);
    vLayout->addWidget(m_verticalScrollBar);
    vLayout->setContentsMargins(0, 0, 0, 0);

    setMinimumHeight(200);
}

void WaveformView::clear()
{
    m_hasSignal = false;
    m_signal.values.clear();
    update();
}

void WaveformView::setSignalData(const VCDParser::Signal& signal, double timeScale)
{
    m_signal = signal;
    m_timeScale = timeScale;
    m_hasSignal = true;
    m_timeOffset = 0;
    m_verticalOffset = 0;
    m_pixelsPerTimeUnit = 10.0;

    updateScrollBars();
    update();
}

void WaveformView::zoomIn()
{
    m_pixelsPerTimeUnit *= 1.5;
    updateScrollBars();
    update();
}

void WaveformView::zoomOut()
{
    m_pixelsPerTimeUnit /= 1.5;
    if (m_pixelsPerTimeUnit < 0.1) m_pixelsPerTimeUnit = 0.1;
    updateScrollBars();
    update();
}

void WaveformView::resetZoom()
{
    m_pixelsPerTimeUnit = 10.0;
    m_timeOffset = 0;
    m_horizontalScrollBar->setValue(0);
    update();
}

void WaveformView::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);

    if (m_hasSignal) {
        drawGrid(painter);
        drawWaveform(painter);
    } else {
        painter.setPen(Qt::black);
        QFont font = painter.font();
        font.setPointSize(14);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, "Select a signal from the list");
    }
}

void WaveformView::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));

    int gridSpacing = 50;
    int firstGrid = static_cast<int>(m_timeOffset * m_pixelsPerTimeUnit) % gridSpacing;
    for (int x = firstGrid; x < width(); x += gridSpacing) {
        painter.drawLine(x, 0, x, height() - 20);
    }

    painter.setPen(QPen(Qt::lightGray, 1, Qt::SolidLine));
    painter.drawLine(0, height() / 2, width(), height() / 2);
}

void WaveformView::drawWaveform(QPainter& painter)
{
    if (m_signal.values.isEmpty()) return;

    painter.setPen(QPen(Qt::black, 2));

    int waveformHeight = height() - 40;
    int centerY = waveformHeight / 2 + 20;

    painter.setPen(Qt::black);
    QFont font = painter.font();
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(10, 15, m_signal.name);

    QPointF lastPoint;
    bool first = true;

    for (const auto& value : m_signal.values) {
        double time = value.time * m_timeScale;
        double x = (time - m_timeOffset) * m_pixelsPerTimeUnit;

        if (x < -10) continue;
        if (x > width() + 10) break;

        int y;
        if (value.value == "0") {
            y = centerY + 20;
        } else if (value.value == "1") {
            y = centerY - 20;
        } else if (value.value == "x" || value.value == "X") {
            y = centerY;
        } else {
            y = centerY;
        }

        if (!first) {
            painter.drawLine(lastPoint.x(), lastPoint.y(), x, lastPoint.y());
            painter.drawLine(x, lastPoint.y(), x, y);
        }

        lastPoint = QPointF(x, y);
        first = false;
    }

    if (!first) {
        painter.drawLine(lastPoint.x(), lastPoint.y(), width(), lastPoint.y());
    }
}

void WaveformView::updateScrollBars()
{
    if (!m_hasSignal || m_signal.values.isEmpty()) {
        m_horizontalScrollBar->setRange(0, 0);
        m_verticalScrollBar->setRange(0, 0);
        return;
    }

    quint64 startTime = m_signal.values.first().time;
    quint64 endTime = m_signal.values.last().time;
    double totalTime = (endTime - startTime) * m_timeScale;
    double visibleTime = width() / m_pixelsPerTimeUnit;

    if (visibleTime >= totalTime) {
        m_horizontalScrollBar->setRange(0, 0);
    } else {
        int maxScroll = static_cast<int>((totalTime - visibleTime) * m_pixelsPerTimeUnit);
        m_horizontalScrollBar->setRange(0, maxScroll);
    }

    m_verticalScrollBar->setRange(0, 0);
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else {
        zoomOut();
    }
    event->accept();
}

void WaveformView::resizeEvent(QResizeEvent *event)
{
    updateScrollBars();
    QWidget::resizeEvent(event);
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
    QWidget::mousePressEvent(event);
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        int deltaX = event->pos().x() - m_lastMousePos.x();
        m_lastMousePos = event->pos();

        int newScrollValue = m_horizontalScrollBar->value() - deltaX;
        m_horizontalScrollBar->setValue(newScrollValue);
    }
    QWidget::mouseMoveEvent(event);
}

void WaveformView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
    QWidget::mouseReleaseEvent(event);
}
