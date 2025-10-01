#include "waveformwidget.h"
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent), vcdParser(nullptr), timeScale(1.0), timeOffset(0),
    timeMarkersHeight(30), topMargin(10),
    isDragging(false), isDraggingSignal(false), dragSignalIndex(-1), selectedSignal(-1)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

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
    selectedSignal = -1;
    updateScrollBar();
    update();
}

void WaveformWidget::setVisibleSignals(const QList<VCDSignal> &visibleSignals)
{
    this->visibleSignals = visibleSignals;
    selectedSignal = -1;
    update();
    emit signalSelected(-1);
}

void WaveformWidget::removeSelectedSignals()
{
    if (selectedSignal >= 0 && selectedSignal < visibleSignals.size()) {
        visibleSignals.removeAt(selectedSignal);
        selectedSignal = -1;
        update();
        emit signalSelected(-1);
    }
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

    int availableWidth = width() - signalNamesWidth - 20;
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

    // Fill entire background with dark theme
    painter.fillRect(rect(), QColor(45, 45, 48));

    if (!vcdParser || visibleSignals.isEmpty()) {
        painter.setPen(QPen(Qt::white));
        painter.drawText(rect(), Qt::AlignCenter, "No signals selected");
        return;
    }

    // Draw signal names column
    drawSignalNamesColumn(painter);

    // Draw waveform area
    drawWaveformArea(painter);
}

void WaveformWidget::drawSignalNamesColumn(QPainter &painter)
{
    // Draw signal names column background
    painter.fillRect(0, 0, signalNamesWidth, height(), QColor(37, 37, 38));

    // Draw column separator
    painter.setPen(QPen(QColor(80, 80, 80), 2));
    painter.drawLine(signalNamesWidth, 0, signalNamesWidth, height());

    // Draw header
    painter.fillRect(0, 0, signalNamesWidth, timeMarkersHeight, QColor(60, 60, 60));
    painter.setPen(QPen(Qt::white));
    painter.drawText(5, timeMarkersHeight - 8, "Signal Name");

    // Draw signal names
    for (int i = 0; i < visibleSignals.size(); i++) {
        const VCDSignal &signal = visibleSignals[i];
        int yPos = topMargin + timeMarkersHeight + i * signalHeight;

        // Draw selection background
        if (i == selectedSignal) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(60, 60, 90));
        } else if (i % 2 == 0) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(40, 40, 43));
        }

        // Draw signal name
        painter.setPen(QPen(Qt::white));
        QString displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
        painter.drawText(5, yPos + signalHeight / 2 + 4, displayName);

        // Draw signal width
        painter.setPen(QPen(QColor(180, 180, 180)));
        painter.drawText(signalNamesWidth - 35, yPos + signalHeight / 2 + 4,
                         QString("[%1:0]").arg(signal.width - 1));

        // Draw horizontal separator
        painter.setPen(QPen(QColor(80, 80, 80)));
        painter.drawLine(0, yPos + signalHeight, signalNamesWidth, yPos + signalHeight);
    }
}

void WaveformWidget::drawWaveformArea(QPainter &painter)
{
    // Set clip region for waveform area only
    painter.setClipRect(signalNamesWidth, 0, width() - signalNamesWidth, height());
    painter.translate(signalNamesWidth, 0);

    // Draw waveform background
    painter.fillRect(0, 0, width() - signalNamesWidth, height(), QColor(30, 30, 30));

    if (!visibleSignals.isEmpty()) {
        drawGrid(painter);
        drawSignals(painter);
    }

    painter.translate(-signalNamesWidth, 0);
    painter.setClipping(false);
}

void WaveformWidget::drawGrid(QPainter &painter)
{
    painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));

    int startTime = xToTime(0);
    int endTime = xToTime(width() - signalNamesWidth);

    // Draw vertical time lines
    int timeStep = calculateTimeStep(startTime, endTime);
    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep) {
        int x = timeToX(time);
        painter.drawLine(x, 0, x, height());

        // Draw time marker
        painter.setPen(QPen(Qt::white));
        painter.drawText(x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    }

    // Draw horizontal signal separators
    for (int i = 0; i <= visibleSignals.size(); i++) {
        int y = topMargin + timeMarkersHeight + i * signalHeight;
        painter.drawLine(0, y, width() - signalNamesWidth, y);
    }

    // Draw selection highlight in waveform area
    if (selectedSignal >= 0) {
        int y = topMargin + timeMarkersHeight + selectedSignal * signalHeight;
        painter.fillRect(0, y, width() - signalNamesWidth, signalHeight, QColor(60, 60, 90));
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
        drawSignalWaveform(painter, signal, yPos);
    }

    // Draw drag preview if dragging
    if (isDraggingSignal && dragSignalIndex >= 0 && dragSignalIndex < visibleSignals.size()) {
        int currentY = topMargin + timeMarkersHeight + dragSignalIndex * signalHeight;
        painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
        painter.drawLine(0, currentY, width() - signalNamesWidth, currentY);
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos)
{
    const auto &changes = vcdParser->getValueChanges().value(signal.identifier);
    if (changes.isEmpty()) return;

    // Use brighter colors for dark theme
    QColor signalColor = QColor::fromHsv((qHash(signal.identifier) % 360), 180, 220);
    painter.setPen(QPen(signalColor, 2));

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

    // Draw value labels for special states
    for (const auto &change : changes) {
        if (change.value == "X" || change.value == "Z") {
            int x = timeToX(change.timestamp);
            painter.setPen(QPen(QColor(255, 100, 100)));
            painter.drawText(x + 2, signalMidY, change.value);
            painter.setPen(QPen(signalColor, 2));
        }
    }
}

int WaveformWidget::getSignalAtPosition(const QPoint &pos) const
{
    if (visibleSignals.isEmpty()) return -1;

    int y = pos.y();
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop) return -1;

    int signalIndex = (y - signalAreaTop) / signalHeight;
    if (signalIndex >= 0 && signalIndex < visibleSignals.size()) {
        return signalIndex;
    }

    return -1;
}

void WaveformWidget::startDragSignal(int signalIndex)
{
    isDraggingSignal = true;
    dragSignalIndex = signalIndex;
    dragStartY = topMargin + timeMarkersHeight + signalIndex * signalHeight;
    setCursor(Qt::ClosedHandCursor);
}

void WaveformWidget::performDrag(int mouseY)
{
    if (!isDraggingSignal || dragSignalIndex < 0) return;

    int signalAreaTop = topMargin + timeMarkersHeight;
    int newIndex = (mouseY - signalAreaTop) / signalHeight;
    newIndex = qMax(0, qMin(newIndex, visibleSignals.size() - 1));

    if (newIndex != dragSignalIndex) {
        visibleSignals.move(dragSignalIndex, newIndex);
        dragSignalIndex = newIndex;
        selectedSignal = newIndex;
        update();
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    // Check if click is in signal names column or waveform area
    bool inNamesColumn = event->pos().x() < signalNamesWidth;

    if (event->button() == Qt::MiddleButton) {
        // Start middle button drag for horizontal scrolling (waveform area only)
        if (!inNamesColumn) {
            isDragging = true;
            dragStartX = event->pos().x() - signalNamesWidth;
            dragStartOffset = timeOffset;
            setCursor(Qt::ClosedHandCursor);
        }
    } else if (event->button() == Qt::LeftButton) {
        int signalIndex = getSignalAtPosition(event->pos());

        if (signalIndex >= 0) {
            // Signal clicked - select it
            selectedSignal = signalIndex;
            emit signalSelected(signalIndex);

            // Prepare for drag (works in both names column and waveform area)
            startDragSignal(signalIndex);
            update();
        } else if (!inNamesColumn) {
            // Start timeline dragging with left button (waveform area only)
            isDragging = true;
            dragStartX = event->pos().x() - signalNamesWidth;
            dragStartOffset = timeOffset;
            setCursor(Qt::ClosedHandCursor);
        }
    }
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingSignal) {
        performDrag(event->pos().y());
    } else if (isDragging) {
        int delta = dragStartX - (event->pos().x() - signalNamesWidth);
        timeOffset = dragStartOffset + delta;
        updateScrollBar();
        update();
    }

    // Emit time change for cursor position in waveform area
    if (event->pos().x() >= signalNamesWidth) {
        int currentTime = xToTime(event->pos().x() - signalNamesWidth);
        emit timeChanged(currentTime);
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton) {
        if (isDraggingSignal) {
            isDraggingSignal = false;
            dragSignalIndex = -1;
            setCursor(Qt::ArrowCursor);
        } else if (isDragging) {
            isDragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

void WaveformWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl + Wheel for zoom
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        // Shift + Wheel for horizontal scrolling
        int scrollAmount = event->angleDelta().y();
        timeOffset += scrollAmount / 2;
        updateScrollBar();
        update();
    } else {
        // Regular wheel for vertical scrolling (if implemented) or default behavior
        QWidget::wheelEvent(event);
    }
}

void WaveformWidget::updateScrollBar()
{
    if (!vcdParser) {
        horizontalScrollBar->setRange(0, 0);
        return;
    }

    int contentWidth = timeToX(vcdParser->getEndTime());
    int viewportWidth = width() - signalNamesWidth;

    horizontalScrollBar->setRange(0, qMax(0, contentWidth - viewportWidth));
    horizontalScrollBar->setPageStep(viewportWidth);
    horizontalScrollBar->setSingleStep(viewportWidth / 10);

    horizontalScrollBar->setGeometry(signalNamesWidth, height() - 20, width() - signalNamesWidth, 20);
}

int WaveformWidget::timeToX(int time) const
{
    return static_cast<int>(time * timeScale) - timeOffset;
}

int WaveformWidget::xToTime(int x) const
{
    return static_cast<int>((x + timeOffset) / timeScale);
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

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    updateScrollBar();
}

// Context menu methods (implementation depends on your needs)
void WaveformWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int signalIndex = getSignalAtPosition(event->pos());
    showContextMenu(event->globalPos(), signalIndex);
}

void WaveformWidget::showContextMenu(const QPoint &pos, int signalIndex)
{
    QMenu contextMenu(this);

    if (signalIndex >= 0) {
        contextMenu.addAction("Add Space Above", this, &WaveformWidget::addSpaceAbove);
        contextMenu.addAction("Add Space Below", this, &WaveformWidget::addSpaceBelow);
        contextMenu.addSeparator();
        contextMenu.addAction("Create Group", this, &WaveformWidget::addGroup);
        contextMenu.addSeparator();
        contextMenu.addAction("Remove Signal", this, &WaveformWidget::removeSelectedSignals);
    } else {
        contextMenu.addAction("Add Space", this, &WaveformWidget::addSpaceAbove);
        contextMenu.addAction("Create Group", this, &WaveformWidget::addGroup);
    }

    contextMenu.exec(pos);
    emit contextMenuRequested(pos, signalIndex);
}

void WaveformWidget::addSpaceAbove() { update(); }
void WaveformWidget::addSpaceBelow() { update(); }
void WaveformWidget::addGroup() { update(); }
