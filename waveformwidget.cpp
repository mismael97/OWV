#include "waveformwidget.h"
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent), vcdParser(nullptr), timeScale(1.0), timeOffset(0),
    signalHeight(30), timeMarkersHeight(30), leftMargin(100), topMargin(10),
    isDragging(false)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    connect(horizontalScrollBar, &QScrollBar::valueChanged, [this](int value) {
        timeOffset = value;
        update();
    });
}

void WaveformWidget::setVcdData(VCDParser *parser)
{
    vcdParser = parser;
    visibleSignals.clear();
    timeScale = 1.0;
    timeOffset = 0;
    updateScrollBar();
    update();
}

void WaveformWidget::setVisibleSignals(const QList<VCDSignal> &visibleSignals)
{
    this->visibleSignals = visibleSignals;
    update();
}

void WaveformWidget::zoomIn()
{
    timeScale *= 1.2;
    updateScrollBar();
    update();
}

void WaveformWidget::zoomOut()
{
    timeScale /= 1.2;
    if (timeScale < 0.1) timeScale = 0.1;
    updateScrollBar();
    update();
}

void WaveformWidget::zoomFit()
{
    if (!vcdParser || vcdParser->getEndTime() == 0) return;

    int availableWidth = width() - leftMargin - 20;
    timeScale = static_cast<double>(availableWidth) / vcdParser->getEndTime();
    timeOffset = 0;
    updateScrollBar();
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.fillRect(rect(), Qt::white);

    if (!vcdParser || visibleSignals.isEmpty()) {
        painter.drawText(rect(), Qt::AlignCenter, "No signals selected");
        return;
    }

    drawGrid(painter);
    drawSignals(painter);
}

void WaveformWidget::drawGrid(QPainter &painter)
{
    painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));

    int startTime = xToTime(0);
    int endTime = xToTime(width());

    int timeStep = calculateTimeStep(startTime, endTime);
    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep) {
        int x = timeToX(time);
        painter.drawLine(x, 0, x, height());

        painter.setPen(QPen(Qt::black));
        painter.drawText(x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(Qt::lightGray, 1, Qt::DotLine));
    }

    for (int i = 0; i <= visibleSignals.size(); i++) {
        int y = topMargin + timeMarkersHeight + i * signalHeight;
        painter.drawLine(0, y, width(), y);
    }
}

int WaveformWidget::calculateTimeStep(int startTime, int endTime) const
{
    int timeRange = endTime - startTime;
    if (timeRange <= 0) return 100;

    double pixelsPerStep = 100.0;
    double targetStep = pixelsPerStep / timeScale;

    double power = std::pow(10, std::floor(std::log10(targetStep)));
    double normalized = targetStep / power;

    if (normalized < 1.5) return static_cast<int>(power);
    else if (normalized < 3) return static_cast<int>(2 * power);
    else if (normalized < 7) return static_cast<int>(5 * power);
    else return static_cast<int>(10 * power);
}

void WaveformWidget::drawSignals(QPainter &painter)
{
    for (int i = 0; i < visibleSignals.size(); i++) {
        const VCDSignal &signal = visibleSignals[i];
        int yPos = topMargin + timeMarkersHeight + i * signalHeight;

        painter.setPen(QPen(Qt::black));
        QString displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
        painter.drawText(5, yPos + signalHeight / 2 + 5, displayName);

        drawSignalWaveform(painter, signal, yPos);
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos)
{
    const auto &changes = vcdParser->getValueChanges().value(signal.identifier);
    if (changes.isEmpty()) return;

    painter.setPen(QPen(Qt::blue, 2));

    int signalMidY = yPos + signalHeight / 2;
    int highLevel = yPos + 5;
    int lowLevel = yPos + signalHeight - 5;

    int prevTime = 0;
    QString prevValue = "0";
    int prevX = timeToX(prevTime);

    for (const auto &change : changes) {
        int currentX = timeToX(change.timestamp);

        if (prevValue == "1" || prevValue == "X" || prevValue == "Z") {
            painter.drawLine(prevX, highLevel, currentX, highLevel);
        } else {
            painter.drawLine(prevX, lowLevel, currentX, lowLevel);
        }

        if (prevValue != change.value) {
            painter.drawLine(currentX, highLevel, currentX, lowLevel);
        }

        prevTime = change.timestamp;
        prevValue = change.value;
        prevX = currentX;
    }

    int endX = timeToX(vcdParser->getEndTime());
    if (prevValue == "1" || prevValue == "X" || prevValue == "Z") {
        painter.drawLine(prevX, highLevel, endX, highLevel);
    } else {
        painter.drawLine(prevX, lowLevel, endX, lowLevel);
    }

    for (const auto &change : changes) {
        if (change.value == "X" || change.value == "Z") {
            int x = timeToX(change.timestamp);
            painter.setPen(QPen(Qt::red));
            painter.drawText(x + 2, signalMidY, change.value);
            painter.setPen(QPen(Qt::blue, 2));
        }
    }
}

void WaveformWidget::updateScrollBar()
{
    if (!vcdParser) {
        horizontalScrollBar->setRange(0, 0);
        return;
    }

    int contentWidth = timeToX(vcdParser->getEndTime());
    int viewportWidth = width() - leftMargin;

    horizontalScrollBar->setRange(0, qMax(0, contentWidth - viewportWidth));
    horizontalScrollBar->setPageStep(viewportWidth);
    horizontalScrollBar->setSingleStep(viewportWidth / 10);

    horizontalScrollBar->setGeometry(leftMargin, height() - 20, width() - leftMargin, 20);
}

int WaveformWidget::timeToX(int time) const
{
    return leftMargin + static_cast<int>(time * timeScale) - timeOffset;
}

int WaveformWidget::xToTime(int x) const
{
    return static_cast<int>((x - leftMargin + timeOffset) / timeScale);
}

QString WaveformWidget::getSignalValueAtTime(const QString &identifier, int time) const
{
    const auto &changes = vcdParser->getValueChanges().value(identifier);
    QString value = "0";

    for (const auto &change : changes) {
        if (change.timestamp > time) break;
        value = change.value;
    }

    return value;
}

void WaveformWidget::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0) {
        zoomIn();
    } else {
        zoomOut();
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        dragStartX = event->pos().x();
        dragStartOffset = timeOffset;
        setCursor(Qt::ClosedHandCursor);
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging) {
        int delta = dragStartX - event->pos().x();
        timeOffset = dragStartOffset + delta;
        updateScrollBar();
        update();

        int currentTime = xToTime(event->pos().x());
        emit timeChanged(currentTime);
    } else {
        int currentTime = xToTime(event->pos().x());
        emit timeChanged(currentTime);
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isDragging) {
        isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    updateScrollBar();
}
