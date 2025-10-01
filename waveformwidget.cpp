#include "waveformwidget.h"
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QInputDialog>
#include <QApplication>
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

// In waveformwidget.cpp, update the removeSelectedSignals method:
// In waveformwidget.cpp, update the removeSelectedSignals method with debug output:
void WaveformWidget::removeSelectedSignals()
{
    if (selectedSignals.isEmpty()) {
        qDebug() << "No items selected for removal";
        return;
    }

    qDebug() << "Removing" << selectedSignals.size() << "items:" << selectedSignals;
    
    // Count what we're removing
    int signalCount = 0;
    int spaceCount = 0;
    int groupCount = 0;
    
    for (int index : selectedSignals) {
        if (isSignalItem(index)) signalCount++;
        else if (isSpaceItem(index)) spaceCount++;
        else if (isGroupItem(index)) groupCount++;
    }
    
    qDebug() << "Removing - Signals:" << signalCount << "Spaces:" << spaceCount << "Groups:" << groupCount;

    // Remove items in reverse order to maintain correct indices
    QList<int> indices = selectedSignals.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    
    for (int index : indices) {
        if (index >= 0 && index < displayItems.size()) {
            qDebug() << "Removing item at index" << index << "type:" << displayItems[index].getType();
            
            // If it's a group, also remove all signals in the group
            if (isGroupItem(index)) {
                const GroupItem& group = displayItems[index].getGroup();
                qDebug() << "Removing group with" << group.signalIndices.size() << "signals";
                // Remove signals in the group (in reverse order)
                QList<int> signalIndices = group.signalIndices;
                std::sort(signalIndices.begin(), signalIndices.end(), std::greater<int>());
                for (int sigIndex : signalIndices) {
                    if (sigIndex >= 0 && sigIndex < displayItems.size()) {
                        qDebug() << "Removing group signal at index" << sigIndex;
                        displayItems.removeAt(sigIndex);
                    }
                }
            }
            displayItems.removeAt(index);
        }
    }
    
    // Update group indices after deletion
    updateAllGroupIndices();
    
    selectedSignals.clear();
    lastSelectedSignal = -1;
    update();
    emit signalSelected(-1);
    
    qDebug() << "Removal completed. Total items now:" << displayItems.size();
}

void WaveformWidget::updateAllGroupIndices()
{
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            GroupItem& group = displayItems[i].getGroup();
            QList<int> validIndices;
            
            // Find which signals still exist and are in the correct position
            for (int j = i + 1; j < displayItems.size(); j++) {
                if (isSignalItem(j)) {
                    // Check if this signal should be in our group
                    // For now, we'll assume all signals after the group header until the next group/space belong to this group
                    if (j < displayItems.size() - 1 && !isGroupItem(j) && !isSpaceItem(j)) {
                        validIndices.append(j);
                    } else {
                        break;
                    }
                } else if (isGroupItem(j) || isSpaceItem(j)) {
                    // Reached the next group or space, stop
                    break;
                }
            }
            
            group.signalIndices = validIndices;
        }
    }
}

// Helper method to update group indices after deletion
void WaveformWidget::updateGroupIndicesAfterDeletion(const QList<int>& deletedIndices)
{
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            GroupItem& group = displayItems[i].getGroup();
            QList<int> updatedIndices;
            
            for (int sigIndex : group.signalIndices) {
                int adjustment = 0;
                for (int deletedIndex : deletedIndices) {
                    if (sigIndex > deletedIndex) {
                        adjustment++;
                    }
                }
                int newIndex = sigIndex - adjustment;
                if (newIndex >= 0 && newIndex < displayItems.size() && isSignalItem(newIndex)) {
                    updatedIndices.append(newIndex);
                }
            }
            
            group.signalIndices = updatedIndices;
        }
    }
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

bool WaveformWidget::isSignalInGroup(int index) const
{
    if (!isSignalItem(index)) return false;
    
    // Check if this signal is part of any group
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            const GroupItem& group = displayItems[i].getGroup();
            if (group.signalIndices.contains(index)) {
                return true;
            }
        }
    }
    return false;
}

int WaveformWidget::getGroupBottomIndex(int groupIndex) const
{
    if (!isGroupItem(groupIndex)) return -1;
    
    const GroupItem& group = displayItems[groupIndex].getGroup();
    if (group.signalIndices.isEmpty()) return -1;
    
    // Find the maximum index in the group's signal indices
    int maxIndex = -1;
    for (int sigIndex : group.signalIndices) {
        if (sigIndex > maxIndex) maxIndex = sigIndex;
    }
    return maxIndex;
}

// Check if an index is a group header or group bottom
bool WaveformWidget::isGroupBoundary(int index) const
{
    if (isGroupItem(index)) return true;
    
    // Check if this is the last signal in any group
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            const GroupItem& group = displayItems[i].getGroup();
            int bottomIndex = getGroupBottomIndex(i);
            if (index == bottomIndex) {
                return true;
            }
        }
    }
    return false;
}

// Select entire group when group header or bottom is clicked
void WaveformWidget::selectGroup(int groupIndex)
{
    if (!isGroupItem(groupIndex)) return;
    
    selectedSignals.clear();
    const GroupItem& group = displayItems[groupIndex].getGroup();
    
    // Select the group header and all signals in the group
    selectedSignals.insert(groupIndex);
    for (int sigIndex : group.signalIndices) {
        selectedSignals.insert(sigIndex);
    }
    
    lastSelectedSignal = groupIndex;
    update();
    emit signalSelected(groupIndex);
}

// In waveformwidget.cpp, update the drawSignalNamesColumn method:
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
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(100, 150, 255, 80)); // Light blue
        } else if (isGroupItem(i)) {
            // Group header - darker red background
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(200, 80, 80, 120));
        } else if (isSignalInGroup(i)) {
            // Signal within a group - slightly indented background
            painter.fillRect(20, yPos, signalNamesWidth - 20, signalHeight, 
                           i % 2 == 0 ? QColor(50, 50, 53) : QColor(45, 45, 48));
        } else if (i % 2 == 0) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(40, 40, 43));
        }

        // Draw item name with appropriate prefix and indentation
        painter.setPen(QPen(Qt::white));
        QString displayName;
        int textIndent = 5;
        
        if (isSpaceItem(i)) {
            displayName = "⏐ " + item.getName();
        } else if (isGroupItem(i)) {
            displayName = "⫿ " + item.getName();
            // Draw group expand/collapse indicator (optional)
            painter.drawText(textIndent, yPos + signalHeight / 2 + 4, "▶");
            textIndent += 15;
        } else if (isSignalInGroup(i)) {
            // Indent signals within groups
            textIndent = 25;
            const VCDSignal &signal = item.getSignal();
            displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
            // Draw bullet point for signals in groups
            painter.drawText(10, yPos + signalHeight / 2 + 4, "•");
        } else {
            try {
                const VCDSignal &signal = item.getSignal();
                displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
            } catch (const std::runtime_error&) {
                displayName = "Invalid Signal";
            }
        }
        
        painter.drawText(textIndent, yPos + signalHeight / 2 + 4, displayName);

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

        // Draw group bottom separator for groups
        if (isGroupItem(i)) {
            int groupBottomIndex = getGroupBottomIndex(i);
            if (groupBottomIndex >= 0 && groupBottomIndex < displayItems.size()) {
                int bottomYPos = topMargin + timeMarkersHeight + groupBottomIndex * signalHeight;
                painter.setPen(QPen(QColor(255, 100, 100), 2));
                painter.drawLine(0, bottomYPos + signalHeight, signalNamesWidth, bottomYPos + signalHeight);
            }
        }
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
    if (itemIndex < 0 || itemIndex >= displayItems.size()) return;

    // Allow selection of spaces with Ctrl or Ctrl+Shift
    bool allowSpaceSelection = (event->modifiers() & Qt::ControlModifier);
    
    // Handle group header/bottom clicks
    if (isGroupItem(itemIndex)) {
        // Single click on group header selects the entire group
        if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
            selectGroup(itemIndex);
            return;
        }
    } else if (isGroupBoundary(itemIndex) && !(event->modifiers() & Qt::ControlModifier)) {
        // Find which group this boundary belongs to and select it
        for (int i = 0; i < displayItems.size(); i++) {
            if (isGroupItem(i)) {
                int bottomIndex = getGroupBottomIndex(i);
                if (itemIndex == bottomIndex) {
                    if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
                        selectGroup(i);
                        return;
                    }
                    break;
                }
            }
        }
    }

    // If it's a space and we're not allowing space selection, clear selection and return
    if (isSpaceItem(itemIndex) && !allowSpaceSelection) {
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
            if (isSignalItem(i) || isSpaceItem(i)) {
                selectedSignals.insert(i);
            }
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl-click: toggle selection (including spaces)
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

// In waveformwidget.cpp, update the keyPressEvent method:
void WaveformWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier) {
        selectAllSignals();
        event->accept();
    } else if (event->key() == Qt::Key_Delete) {
        // Delete any selected items (signals, spaces, or groups)
        removeSelectedSignals();
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

void WaveformWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    showContextMenu(event->globalPos(), itemIndex);
}

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)
    updateScrollBar();
}

// In waveformwidget.cpp, update the contextMenuEvent:
// In waveformwidget.cpp, update getItemAtPosition with debug if needed:
int WaveformWidget::getItemAtPosition(const QPoint &pos) const
{
    if (displayItems.isEmpty()) {
        qDebug() << "No display items";
        return -1;
    }

    int y = pos.y();
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop) {
        qDebug() << "Click above signal area";
        return -1;
    }

    int itemIndex = (y - signalAreaTop) / signalHeight;
    
    if (itemIndex >= 0 && itemIndex < displayItems.size()) {
        qDebug() << "Found item at index:" << itemIndex;
        return itemIndex;
    }

    qDebug() << "Item index out of range:" << itemIndex << "display items count:" << displayItems.size();
    return -1;
}

// In waveformwidget.cpp, update the showContextMenu method to ensure the item is selected:
// In waveformwidget.cpp, update the showContextMenu method:
void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex)
{
    QMenu contextMenu(this);

    bool hasSelection = !selectedSignals.isEmpty();
    bool isSignal = isSignalItem(itemIndex);
    bool isSpace = isSpaceItem(itemIndex);
    bool isGroup = isGroupItem(itemIndex);

    // If right-clicking on a specific item, handle selection
    if (itemIndex >= 0) {
        // Store the current selection temporarily
        QSet<int> previousSelection = selectedSignals;
        int previousLastSelected = lastSelectedSignal;

        // If the clicked item is not in selection, select only it
        if (!selectedSignals.contains(itemIndex)) {
            selectedSignals.clear();
            selectedSignals.insert(itemIndex);
            lastSelectedSignal = itemIndex;
            update();
        }

        // Create the context menu
        if (!selectedSignals.isEmpty()) {
            QString removeText = "Remove Selected";
            // Count what types of items are selected
            int signalCount = 0;
            int spaceCount = 0;
            int groupCount = 0;

            for (int index : selectedSignals) {
                if (isSignalItem(index)) signalCount++;
                else if (isSpaceItem(index)) spaceCount++;
                else if (isGroupItem(index)) groupCount++;
            }

            if (signalCount > 0 && spaceCount == 0 && groupCount == 0) {
                removeText = "Remove Selected Signals";
            } else if (spaceCount > 0 && signalCount == 0 && groupCount == 0) {
                removeText = "Remove Selected Spaces";
            } else if (groupCount > 0 && signalCount == 0 && spaceCount == 0) {
                removeText = "Remove Selected Groups";
            }

            contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
            contextMenu.addSeparator();
        }

        if (isSignal || isSpace) {
            contextMenu.addAction("Add Space Above", this, [this, itemIndex]() { addSpaceAbove(itemIndex); });
            contextMenu.addAction("Add Space Below", this, [this, itemIndex]() { addSpaceBelow(itemIndex); });
            contextMenu.addSeparator();
        }

        if (selectedSignals.size() > 1) {
            // Only allow grouping if all selected items are signals
            bool allSignals = true;
            for (int index : selectedSignals) {
                if (!isSignalItem(index)) {
                    allSignals = false;
                    break;
                }
            }
            if (allSignals) {
                contextMenu.addAction("Group Signals", this, &WaveformWidget::addGroup);
                contextMenu.addSeparator();
            }
        }

        if (isSignal || isSpace || isGroup) {
            QString removeText = "Remove";
            if (isSignal) removeText = "Remove Signal";
            else if (isSpace) removeText = "Remove Space";
            else if (isGroup) removeText = "Remove Group";

            contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
        }

        // Execute the context menu
        QAction* selectedAction = contextMenu.exec(pos);

        // If no action was taken (user clicked away), restore previous selection
        if (!selectedAction) {
            selectedSignals = previousSelection;
            lastSelectedSignal = previousLastSelected;
            update();
        }
    } else {
        // Right-click on empty area
        if (hasSelection) {
            contextMenu.addAction("Remove Selected Signals", this, &WaveformWidget::removeSelectedSignals);
            contextMenu.addSeparator();
        }
        contextMenu.addAction("Add Space", this, [this]() {
            // Add space at the end
            SpaceItem space;
            space.name = "Space";
            displayItems.append(DisplayItem(space));
            update();
        });
    }

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
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "Space");
    SpaceItem space;
    space.name = name;
    
    displayItems.insert(index, DisplayItem(space));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "Space");
    SpaceItem space;
    space.name = name;
    
    int insertIndex = index + 1;
    if (insertIndex > displayItems.size()) {
        insertIndex = displayItems.size();
    }
    
    displayItems.insert(insertIndex, DisplayItem(space));
    update();
}

// In waveformwidget.cpp, update the addGroup method:
// In waveformwidget.cpp, replace the addGroup method with this corrected version:
void WaveformWidget::addGroup()
{
    if (selectedSignals.size() < 2) return;

    QString name = promptForName("Create Group", "Group");
    if (name.isEmpty()) return; // User cancelled

    // Get selected signal indices and sort them
    QList<int> signalIndices = selectedSignals.values();
    std::sort(signalIndices.begin(), signalIndices.end());

    // We need to insert items, so we'll work with a temporary list
    QList<DisplayItem> newDisplayItems;
    int currentOriginalIndex = 0;

    for (int i = 0; i <= displayItems.size(); i++) {
        // Check if we're at a position where we need to insert group elements
        bool atFirstSignal = (i == signalIndices.first());
        bool atLastSignal = (!signalIndices.isEmpty() && i == signalIndices.last() + 1);
        bool inSignalRange = (signalIndices.contains(i));

        if (atFirstSignal) {
            // Insert space above the group
            SpaceItem topSpace;
            topSpace.name = "";
            newDisplayItems.append(DisplayItem(topSpace));

            // Insert group header
            GroupItem group;
            group.name = name;
            newDisplayItems.append(DisplayItem(group));
        }

        if (i < displayItems.size()) {
            // Add the current item
            newDisplayItems.append(displayItems[i]);

            // If this is a signal in the group, add it to the group's signal indices
            if (inSignalRange && isSignalItem(i)) {
                // Find the group header we just added
                for (int j = newDisplayItems.size() - 1; j >= 0; j--) {
                    if (isGroupItem(j)) {
                        GroupItem& group = newDisplayItems[j].getGroup();
                        // Store the current index in the new list
                        group.signalIndices.append(newDisplayItems.size() - 1);
                        break;
                    }
                }
            }
        }

        if (atLastSignal) {
            // Insert space below the group
            SpaceItem bottomSpace;
            bottomSpace.name = "";
            newDisplayItems.append(DisplayItem(bottomSpace));
        }
    }

    displayItems = newDisplayItems;
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
