#include "waveformwidget.h"
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <cmath>
#include <stdexcept>

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent), vcdParser(nullptr), timeScale(1.0), timeOffset(0),
    timeMarkersHeight(30), topMargin(10),
    isDragging(false), isDraggingSignal(false), dragSignalIndex(-1), lastSelectedSignal(-1)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    // Remove or comment out the line below, or change to Qt::DefaultContextMenu
    // setContextMenuPolicy(Qt::CustomContextMenu);  // ← Remove or change this line

    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    connect(horizontalScrollBar, &QScrollBar::valueChanged, [this](int value) {
        timeOffset = value;
        update();
    });
}

void WaveformWidget::setVcdData(VCDParser *parser)
{
    vcdParser = parser;
    displayItems.clear();
    timeScale = 1.0;
    timeOffset = 0;
    selectedSignals.clear();
    lastSelectedSignal = -1;
    updateScrollBar();
    update();
}

void WaveformWidget::setVisibleSignals(const QList<VCDSignal> &visibleSignals)
{
    displayItems.clear();
    for (const auto& signal : visibleSignals) {
        displayItems.append(DisplayItem(signal));
    }
    selectedSignals.clear();
    lastSelectedSignal = -1;
    update();
    emit signalSelected(-1);
}

void WaveformWidget::removeSelectedSignals()
{
    if (selectedSignals.isEmpty()) return;

    // Remove items in reverse order to maintain correct indices
    QList<int> indices = selectedSignals.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    
    for (int index : indices) {
        if (index >= 0 && index < displayItems.size()) {
            // If it's a group, also remove all signals in the group
            if (isGroupItem(index)) {
                // Find all signals in this group and remove them
                QList<int> signalIndices = displayItems[index].getGroup().signalIndices;
                std::sort(signalIndices.begin(), signalIndices.end(), std::greater<int>());
                for (int sigIndex : signalIndices) {
                    if (sigIndex >= 0 && sigIndex < displayItems.size()) {
                        displayItems.removeAt(sigIndex);
                    }
                }
            }
            displayItems.removeAt(index);
        }
    }
    
    selectedSignals.clear();
    lastSelectedSignal = -1;
    update();
    emit signalSelected(-1);
}

void WaveformWidget::selectAllSignals()
{
    selectedSignals.clear();
    for (int i = 0; i < displayItems.size(); i++) {
        if (isSignalItem(i)) {
            selectedSignals.insert(i);
        }
    }
    lastSelectedSignal = displayItems.size() - 1;
    update();
    emit signalSelected(selectedSignals.isEmpty() ? -1 : *selectedSignals.begin());
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

    if (!vcdParser || displayItems.isEmpty()) {
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

    // Draw selection count if multiple signals selected
    if (selectedSignals.size() > 1) {
        painter.drawText(signalNamesWidth - 50, timeMarkersHeight - 8, 
                        QString("(%1 selected)").arg(selectedSignals.size()));
    }
    
    for (int i = 0; i < displayItems.size(); i++) {
        const DisplayItem &item = displayItems[i];
        int yPos = topMargin + timeMarkersHeight + i * signalHeight;

        // Draw different backgrounds based on item type
        if (selectedSignals.contains(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(60, 60, 90));
        } else if (isSpaceItem(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(100, 150, 255, 50)); // Light blue
        } else if (isGroupItem(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(255, 100, 100, 50)); // Light red
        } else if (i % 2 == 0) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(40, 40, 43));
        }

        // Draw item name with appropriate prefix
        painter.setPen(QPen(Qt::white));
        QString displayName;
        
        if (isSpaceItem(i)) {
            displayName = "⏐ " + item.getName();
        } else if (isGroupItem(i)) {
            displayName = "⫿ " + item.getName();
        } else {
            try {
                const VCDSignal &signal = item.getSignal();
                displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
            } catch (const std::runtime_error&) {
                displayName = "Invalid Signal";
            }
        }
        
        painter.drawText(5, yPos + signalHeight / 2 + 4, displayName);

        // Draw signal width for regular signals
        if (isSignalItem(i)) {
            try {
                const VCDSignal &signal = item.getSignal();
                painter.setPen(QPen(QColor(180, 180, 180)));
                painter.drawText(signalNamesWidth - 35, yPos + signalHeight / 2 + 4,
                                QString("[%1:0]").arg(signal.width - 1));
            } catch (const std::runtime_error&) {
                // Skip width for invalid signals
            }
        }

        // Draw horizontal separator
        painter.setPen(QPen(QColor(80, 80, 80)));
        painter.drawLine(0, yPos + signalHeight, signalNamesWidth, yPos + signalHeight);
    }
}

void WaveformWidget::drawWaveformArea(QPainter &painter)
{
    painter.setClipRect(signalNamesWidth, 0, width() - signalNamesWidth, height());
    painter.translate(signalNamesWidth, 0);

    painter.fillRect(0, 0, width() - signalNamesWidth, height(), QColor(30, 30, 30));

    if (!displayItems.isEmpty()) {
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

    int timeStep = calculateTimeStep(startTime, endTime);
    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep) {
        int x = timeToX(time);
        painter.drawLine(x, 0, x, height());

        painter.setPen(QPen(Qt::white));
        painter.drawText(x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    }

    for (int i = 0; i <= displayItems.size(); i++) {
        int y = topMargin + timeMarkersHeight + i * signalHeight;
        painter.drawLine(0, y, width() - signalNamesWidth, y);
    }

    // Draw selection highlight for all selected signals
    for (int index : selectedSignals) {
        int y = topMargin + timeMarkersHeight + index * signalHeight;
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
    for (int i = 0; i < displayItems.size(); i++) {
        const DisplayItem &item = displayItems[i];
        if (isSignalItem(i)) {
            try {
                int yPos = topMargin + timeMarkersHeight + i * signalHeight;
                drawSignalWaveform(painter, item.getSignal(), yPos);
            } catch (const std::runtime_error&) {
                // Skip invalid signals
            }
        }
    }

    // Draw drag preview if dragging
    if (isDraggingSignal && dragSignalIndex >= 0 && dragSignalIndex < displayItems.size()) {
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

int WaveformWidget::getItemAtPosition(const QPoint &pos) const
{
    if (displayItems.isEmpty()) return -1;

    int y = pos.y();
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop) return -1;

    int itemIndex = (y - signalAreaTop) / signalHeight;
    if (itemIndex >= 0 && itemIndex < displayItems.size()) {
        return itemIndex;
    }

    return -1;
}

bool WaveformWidget::isSignalItem(int index) const
{
    return index >= 0 && index < displayItems.size() && displayItems[index].getType() == DisplayItem::Signal;
}

bool WaveformWidget::isSpaceItem(int index) const
{
    return index >= 0 && index < displayItems.size() && displayItems[index].getType() == DisplayItem::Space;
}

bool WaveformWidget::isGroupItem(int index) const
{
    return index >= 0 && index < displayItems.size() && displayItems[index].getType() == DisplayItem::Group;
}

void WaveformWidget::handleMultiSelection(int itemIndex, QMouseEvent *event)
{
    if (!isSignalItem(itemIndex)) {
        // Only allow selection of signals, not spaces or groups
        if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
            selectedSignals.clear();
            lastSelectedSignal = -1;
        }
        return;
    }

    if (event->modifiers() & Qt::ShiftModifier && lastSelectedSignal != -1) {
        // Shift-click: select range from last selected to current
        selectedSignals.clear();
        int start = qMin(lastSelectedSignal, itemIndex);
        int end = qMax(lastSelectedSignal, itemIndex);
        for (int i = start; i <= end; i++) {
            if (isSignalItem(i)) {
                selectedSignals.insert(i);
            }
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl-click: toggle selection
        if (selectedSignals.contains(itemIndex)) {
            selectedSignals.remove(itemIndex);
        } else {
            selectedSignals.insert(itemIndex);
        }
        lastSelectedSignal = itemIndex;
    } else {
        // Regular click: single selection
        selectedSignals.clear();
        selectedSignals.insert(itemIndex);
        lastSelectedSignal = itemIndex;
    }
}

void WaveformWidget::updateSelection(int itemIndex, bool isMultiSelect)
{
    if (!isMultiSelect) {
        selectedSignals.clear();
        if (isSignalItem(itemIndex)) {
            selectedSignals.insert(itemIndex);
        }
        lastSelectedSignal = itemIndex;
    }
    update();
    emit signalSelected(itemIndex);
}

void WaveformWidget::startDragSignal(int signalIndex)
{
    if (!isSignalItem(signalIndex)) return;
    
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
    newIndex = qMax(0, qMin(newIndex, displayItems.size() - 1));

    if (newIndex != dragSignalIndex) {
        displayItems.move(dragSignalIndex, newIndex);

        // Update selection if dragging a selected signal
        if (selectedSignals.contains(dragSignalIndex)) {
            selectedSignals.remove(dragSignalIndex);
            selectedSignals.insert(newIndex);
            lastSelectedSignal = newIndex;
        }

        dragSignalIndex = newIndex;
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
        int itemIndex = getItemAtPosition(event->pos());

        if (itemIndex >= 0) {
            // Handle multi-selection
            handleMultiSelection(itemIndex, event);

            // Prepare for drag (only for signals)
            if (isSignalItem(itemIndex)) {
                startDragSignal(itemIndex);
            }
            update();
            emit signalSelected(itemIndex);
        } else if (!inNamesColumn) {
            // Clear selection when clicking empty space
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                selectedSignals.clear();
                lastSelectedSignal = -1;
                update();
                emit signalSelected(-1);
            }
            
            // Start timeline dragging with left button (waveform area only)
            isDragging = true;
            dragStartX = event->pos().x() - signalNamesWidth;
            dragStartOffset = timeOffset;
            setCursor(Qt::ClosedHandCursor);
        }
    }
}

void WaveformWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    if (itemIndex >= 0 && isGroupItem(itemIndex)) {
        renameItem(itemIndex);
        event->accept();
    } else {
        QWidget::mouseDoubleClickEvent(event);
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

void WaveformWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier) {
        selectAllSignals();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
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

void WaveformWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    showContextMenu(event->globalPos(), itemIndex);
}

void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex)
{
    QMenu contextMenu(this);

    bool hasSelection = !selectedSignals.isEmpty();
    bool isSignal = isSignalItem(itemIndex);
    
    if (hasSelection) {
        contextMenu.addAction("Remove Selected Signals", this, &WaveformWidget::removeSelectedSignals);
        contextMenu.addSeparator();
    }

    if (isSignal) {
        contextMenu.addAction("Add Space Above", this, [this, itemIndex]() { addSpaceAbove(itemIndex); });
        contextMenu.addAction("Add Space Below", this, [this, itemIndex]() { addSpaceBelow(itemIndex); });
        contextMenu.addSeparator();
    }

    if (hasSelection && selectedSignals.size() > 1) {
        contextMenu.addAction("Group Signals", this, &WaveformWidget::addGroup);
        contextMenu.addSeparator();
    }

    if (isSignal) {
        contextMenu.addAction("Remove Signal", this, &WaveformWidget::removeSelectedSignals);
    }

    contextMenu.exec(pos);
    emit contextMenuRequested(pos, itemIndex);
}

QString WaveformWidget::promptForName(const QString &title, const QString &defaultName)
{
    bool ok;
    QString name = QInputDialog::getText(this, title, "Name:", QLineEdit::Normal, defaultName, &ok);
    if (ok && !name.isEmpty()) {
        return name;
    }
    return defaultName;
}

void WaveformWidget::addSpaceAbove(int index)
{
    QString name = promptForName("Add Space", "Space");
    SpaceItem space;
    space.name = name;
    
    int insertIndex = qMax(0, index);
    displayItems.insert(insertIndex, DisplayItem(space));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    QString name = promptForName("Add Space", "Space");
    SpaceItem space;
    space.name = name;
    
    int insertIndex = qMin(displayItems.size(), index + 1);
    displayItems.insert(insertIndex, DisplayItem(space));
    update();
}

void WaveformWidget::addGroup()
{
    if (selectedSignals.size() < 2) return;
    
    QString name = promptForName("Create Group", "Group");
    GroupItem group;
    group.name = name;
    
    // Get selected signal indices and sort them
    QList<int> signalIndices = selectedSignals.values();
    std::sort(signalIndices.begin(), signalIndices.end());
    
    // Create the group at the position of the first selected signal
    int insertIndex = signalIndices.first();
    displayItems.insert(insertIndex, DisplayItem(group));
    
    // Update the group with signal indices (adjusting for the insertion)
    GroupItem& insertedGroup = displayItems[insertIndex].getGroup();
    for (int i = 0; i < signalIndices.size(); i++) {
        insertedGroup.signalIndices.append(signalIndices[i] + 1 + i); // +1 for group, +i for previous shifts
    }
    
    update();
}

void WaveformWidget::renameItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size()) return;
    
    DisplayItem &item = displayItems[itemIndex];
    QString currentName = item.getName();
    QString newName = promptForName("Rename", currentName);
    
    if (!newName.isEmpty() && newName != currentName) {
        switch(item.getType()) {
            case DisplayItem::Space:
                item.getSpace().name = newName;
                break;
            case DisplayItem::Group:
                item.getGroup().name = newName;
                break;
            case DisplayItem::Signal:
                // Don't rename signals for now
                break;
        }
        update();
    }
}