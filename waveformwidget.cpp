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
    connect(horizontalScrollBar, &QScrollBar::valueChanged, [this](int value)
            {
        timeOffset = value;
        update(); });
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
    for (const auto &signal : visibleSignals)
    {
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
    if (selectedSignals.isEmpty()) return;

    // First, identify all items to delete (including group signals)
    QSet<int> itemsToDelete;
    
    for (int index : selectedSignals) {
        itemsToDelete.insert(index);
        
        // If this is a group, add all its signals to deletion
        if (isGroupItem(index)) {
            const GroupItem& group = displayItems[index].getGroup();
            for (int sigIndex : group.signalIndices) {
                itemsToDelete.insert(sigIndex);
            }
        }
    }

    // Remove items in reverse order
    QList<int> indices = itemsToDelete.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    
    for (int index : indices) {
        if (index >= 0 && index < displayItems.size()) {
            displayItems.removeAt(index);
        }
    }
    
    // Rebuild all group relationships
    updateAllGroupIndices();
    
    selectedSignals.clear();
    lastSelectedSignal = -1;
    update();
    emit signalSelected(-1);
}
void WaveformWidget::updateAllGroupIndices()
{
    // Clear all group signal indices first
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            GroupItem& group = displayItems[i].getGroup();
            group.signalIndices.clear();
        }
    }
    
    // For each group, only add signals that were explicitly selected when creating the group
    // Don't automatically add signals based on position
    // Groups should only contain signals that were explicitly added to them
    qDebug() << "Group system: Manual signal tracking - no automatic grouping";
}

// Helper method to update group indices after deletion
void WaveformWidget::updateGroupIndicesAfterDeletion(const QList<int> &deletedIndices)
{
    for (int i = 0; i < displayItems.size(); i++)
    {
        if (isGroupItem(i))
        {
            GroupItem &group = displayItems[i].getGroup();
            QList<int> updatedIndices;

            for (int sigIndex : group.signalIndices)
            {
                int adjustment = 0;
                for (int deletedIndex : deletedIndices)
                {
                    if (sigIndex > deletedIndex)
                    {
                        adjustment++;
                    }
                }
                int newIndex = sigIndex - adjustment;
                if (newIndex >= 0 && newIndex < displayItems.size() && isSignalItem(newIndex))
                {
                    updatedIndices.append(newIndex);
                }
            }

            group.signalIndices = updatedIndices;
        }
    }
}

// Update the selectAllSignals method to select all item types:
void WaveformWidget::selectAllSignals()
{
    selectedSignals.clear();
    for (int i = 0; i < displayItems.size(); i++)
    {
        selectedSignals.insert(i);
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
    if (timeScale < 0.1)
        timeScale = 0.1;
    updateScrollBar();
    update();
}

void WaveformWidget::zoomFit()
{
    if (!vcdParser || vcdParser->getEndTime() == 0)
        return;

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

    if (!vcdParser || displayItems.isEmpty())
    {
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

    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            const GroupItem &group = displayItems[i].getGroup();
            if (group.signalIndices.contains(index)) {
                return true;
            }
        }
    }
    return false;
}

int WaveformWidget::getGroupBottomIndex(int groupIndex) const
{
    if (!isGroupItem(groupIndex))
        return -1;

    const GroupItem &group = displayItems[groupIndex].getGroup();
    if (group.signalIndices.isEmpty())
        return -1;

    // Find the maximum index in the group's signal indices
    int maxIndex = -1;
    for (int sigIndex : group.signalIndices)
    {
        if (sigIndex > maxIndex)
            maxIndex = sigIndex;
    }
    return maxIndex;
}

// Check if an index is a group header or group bottom
bool WaveformWidget::isGroupBoundary(int index) const
{
    if (isGroupItem(index))
        return true;

    // Check if this is the last signal in any group
    for (int i = 0; i < displayItems.size(); i++)
    {
        if (isGroupItem(i))
        {
            const GroupItem &group = displayItems[i].getGroup();
            int bottomIndex = getGroupBottomIndex(i);
            if (index == bottomIndex)
            {
                return true;
            }
        }
    }
    return false;
}

// Select entire group when group header or bottom is clicked
void WaveformWidget::selectGroup(int groupIndex)
{
    if (!isGroupItem(groupIndex))
        return;

    selectedSignals.clear();
    const GroupItem &group = displayItems[groupIndex].getGroup();

    // Select the group header and all signals in the group
    selectedSignals.insert(groupIndex);
    for (int sigIndex : group.signalIndices)
    {
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

        // Determine if this signal is in a group
        bool inGroup = false;
        for (int j = 0; j < displayItems.size(); j++) {
            if (isGroupItem(j)) {
                const GroupItem& group = displayItems[j].getGroup();
                if (group.signalIndices.contains(i)) {
                    inGroup = true;
                    break;
                }
            }
        }

        // Draw different backgrounds based on item type and group membership
        if (selectedSignals.contains(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(60, 60, 90));
        } else if (isSpaceItem(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(80, 160, 80, 120));
        } else if (isGroupItem(i)) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(160, 80, 80, 120));
        } else if (inGroup) {
            // Signal within a group - indented background
            painter.fillRect(20, yPos, signalNamesWidth - 20, signalHeight, 
                           i % 2 == 0 ? QColor(45, 45, 48) : QColor(40, 40, 43));
        } else if (i % 2 == 0) {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, yPos, signalNamesWidth, signalHeight, QColor(40, 40, 43));
        }

        // Draw item name with appropriate indentation
        painter.setPen(QPen(Qt::white));
        QString displayName;
        int textIndent = 5;
        
        if (isSpaceItem(i)) {
            QString spaceName = item.getName();
            displayName = spaceName.isEmpty() ? "⏐" : "⏐ " + spaceName;
        } else if (isGroupItem(i)) {
            displayName = "⫿ " + item.getName();
        } else if (inGroup) {
            // Indent signals within groups
            textIndent = 25;
            try {
                const VCDSignal &signal = item.getSignal();
                displayName = "    " + (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name);
            } catch (const std::runtime_error&) {
                displayName = "    Invalid Signal";
            }
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
                // Adjust width position for indented signals
                int widthX = signalNamesWidth - 35;
                if (isSignalInGroup(i)) {
                    widthX -= 20; // Adjust for indentation
                }
                painter.drawText(widthX, yPos + signalHeight / 2 + 4,
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

    if (!displayItems.isEmpty())
    {
        drawGrid(painter);

        // Draw black backgrounds for spaces and groups in waveform area
        for (int i = 0; i < displayItems.size(); i++) {
                const DisplayItem &item = displayItems[i];
                int yPos = topMargin + timeMarkersHeight + i * signalHeight;

            if (isSpaceItem(i) || isGroupItem(i))
            {
                // Black background for spaces and groups in waveform area
                painter.fillRect(0, yPos, width() - signalNamesWidth, signalHeight, QColor(0, 0, 0));
            }
        }

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
    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep)
    {
        int x = timeToX(time);
        painter.drawLine(x, 0, x, height());

        painter.setPen(QPen(Qt::white));
        painter.drawText(x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    }

    for (int i = 0; i <= displayItems.size(); i++)
    {
        int y = topMargin + timeMarkersHeight + i * signalHeight;
        painter.drawLine(0, y, width() - signalNamesWidth, y);
    }

    // Draw selection highlight for all selected signals
    for (int index : selectedSignals)
    {
        int y = topMargin + timeMarkersHeight + index * signalHeight;
        painter.fillRect(0, y, width() - signalNamesWidth, signalHeight, QColor(60, 60, 90));
    }
}

int WaveformWidget::calculateTimeStep(int startTime, int endTime) const
{
    int timeRange = endTime - startTime;
    if (timeRange <= 0)
        return 100;

    double pixelsPerStep = 100.0;
    double targetStep = pixelsPerStep / timeScale;

    double power = std::pow(10, std::floor(std::log10(targetStep)));
    double normalized = targetStep / power;

    if (normalized < 1.5)
        return static_cast<int>(power);
    else if (normalized < 3)
        return static_cast<int>(2 * power);
    else if (normalized < 7)
        return static_cast<int>(5 * power);
    else
        return static_cast<int>(10 * power);
}

void WaveformWidget::drawSignals(QPainter &painter)
{
    for (int i = 0; i < displayItems.size(); i++)
    {
        const DisplayItem &item = displayItems[i];
        if (isSignalItem(i))
        {
            try
            {
                int yPos = topMargin + timeMarkersHeight + i * signalHeight;
                drawSignalWaveform(painter, item.getSignal(), yPos);
            }
            catch (const std::runtime_error &)
            {
                // Skip invalid signals
            }
        }
    }

    // Draw drag preview if dragging
    if (isDraggingSignal && dragSignalIndex >= 0 && dragSignalIndex < displayItems.size())
    {
        int currentY = topMargin + timeMarkersHeight + dragSignalIndex * signalHeight;
        painter.setPen(QPen(Qt::red, 2, Qt::DashLine));
        painter.drawLine(0, currentY, width() - signalNamesWidth, currentY);
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos)
{
    const auto &changes = vcdParser->getValueChanges().value(signal.identifier);
    if (changes.isEmpty())
        return;

    // Use brighter colors for dark theme
    QColor signalColor = QColor::fromHsv((qHash(signal.identifier) % 360), 180, 220);
    painter.setPen(QPen(signalColor, 2));

    int signalMidY = yPos + signalHeight / 2;
    int highLevel = yPos + 5;
    int lowLevel = yPos + signalHeight - 5;

    int prevTime = 0;
    QString prevValue = "0";
    int prevX = timeToX(prevTime);

    for (const auto &change : changes)
    {
        int currentX = timeToX(change.timestamp);

        if (prevValue == "1" || prevValue == "X" || prevValue == "Z")
        {
            painter.drawLine(prevX, highLevel, currentX, highLevel);
        }
        else
        {
            painter.drawLine(prevX, lowLevel, currentX, lowLevel);
        }

        if (prevValue != change.value)
        {
            painter.drawLine(currentX, highLevel, currentX, lowLevel);
        }

        prevTime = change.timestamp;
        prevValue = change.value;
        prevX = currentX;
    }

    int endX = timeToX(vcdParser->getEndTime());
    if (prevValue == "1" || prevValue == "X" || prevValue == "Z")
    {
        painter.drawLine(prevX, highLevel, endX, highLevel);
    }
    else
    {
        painter.drawLine(prevX, lowLevel, endX, lowLevel);
    }

    // Draw value labels for special states
    for (const auto &change : changes)
    {
        if (change.value == "X" || change.value == "Z")
        {
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

    // Allow selection of all item types (signals, spaces, groups)
    bool allowSelection = true;

    // Handle group header clicks - select only the header by default
    if (isGroupItem(itemIndex)) {
        if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier))) {
            // Single click on group header selects only the header
            selectedSignals.clear();
            selectedSignals.insert(itemIndex);
            lastSelectedSignal = itemIndex;
            update();
            emit signalSelected(itemIndex);
            return;
        }
    }

    if (event->modifiers() & Qt::ShiftModifier && lastSelectedSignal != -1) {
        // Shift-click: select range from last selected to current
        selectedSignals.clear();
        int start = qMin(lastSelectedSignal, itemIndex);
        int end = qMax(lastSelectedSignal, itemIndex);
        for (int i = start; i <= end; i++) {
            selectedSignals.insert(i);
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl-click: toggle selection (all item types)
        if (selectedSignals.contains(itemIndex)) {
            selectedSignals.remove(itemIndex);
        } else {
            selectedSignals.insert(itemIndex);
        }
        lastSelectedSignal = itemIndex;
    } else {
        // Regular click: single selection (all item types)
        selectedSignals.clear();
        selectedSignals.insert(itemIndex);
        lastSelectedSignal = itemIndex;
    }
    
    update();
    emit signalSelected(itemIndex);
}

void WaveformWidget::updateSelection(int itemIndex, bool isMultiSelect)
{
    if (!isMultiSelect)
    {
        selectedSignals.clear();
        if (isSignalItem(itemIndex))
        {
            selectedSignals.insert(itemIndex);
        }
        lastSelectedSignal = itemIndex;
    }
    update();
    emit signalSelected(itemIndex);
}

// In waveformwidget.cpp, update the drag methods:
void WaveformWidget::startDragSignal(int signalIndex)
{
    // Allow dragging of all item types
    isDraggingSignal = true;
    dragSignalIndex = signalIndex;
    dragStartY = topMargin + timeMarkersHeight + signalIndex * signalHeight;
    setCursor(Qt::ClosedHandCursor);
}

void WaveformWidget::updateSignalGroupMembership(int signalIndex)
{
    if (!isSignalItem(signalIndex)) return;
    
    // Remove signal from all groups first
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            GroupItem& group = displayItems[i].getGroup();
            group.signalIndices.removeAll(signalIndex);
        }
    }
    
    // Find if this signal is now within a group
    // Look for the nearest group header above this signal
    int groupHeaderIndex = -1;
    for (int i = signalIndex - 1; i >= 0; i--) {
        if (isGroupItem(i)) {
            groupHeaderIndex = i;
            break;
        } else if (isSpaceItem(i) || (i > 0 && isSignalItem(i) && !isSignalInGroup(i))) {
            // Reached a space or regular signal, stop searching
            break;
        }
    }
    
    // If found a group header above, add this signal to that group
    if (groupHeaderIndex != -1) {
        GroupItem& group = displayItems[groupHeaderIndex].getGroup();
        // Check if this signal is below the group header and above the next group/space
        bool shouldBeInGroup = true;
        for (int i = groupHeaderIndex + 1; i < signalIndex; i++) {
            if (isGroupItem(i) || isSpaceItem(i)) {
                shouldBeInGroup = false;
                break;
            }
        }
        
        if (shouldBeInGroup && !group.signalIndices.contains(signalIndex)) {
            group.signalIndices.append(signalIndex);
            qDebug() << "Added signal" << signalIndex << "to group at" << groupHeaderIndex;
        }
    }
}

void WaveformWidget::performDrag(int mouseY)
{
    if (!isDraggingSignal || dragSignalIndex < 0) return;

    int signalAreaTop = topMargin + timeMarkersHeight;
    int newIndex = (mouseY - signalAreaTop) / signalHeight;
    newIndex = qMax(0, qMin(newIndex, displayItems.size() - 1));

    if (newIndex != dragSignalIndex) {
        // Store the item being moved
        DisplayItem movedItem = displayItems[dragSignalIndex];
        
        // Remove from current position
        displayItems.removeAt(dragSignalIndex);
        
        // Insert at new position
        displayItems.insert(newIndex, movedItem);
        
        // Update group membership based on new position
        updateSignalGroupMembership(newIndex);
        
        // Update selection if dragging a selected item
        if (selectedSignals.contains(dragSignalIndex)) {
            selectedSignals.remove(dragSignalIndex);
            selectedSignals.insert(newIndex);
            lastSelectedSignal = newIndex;
        }

        dragSignalIndex = newIndex;
        update();
    }
}

// Update mousePressEvent to allow dragging of all item types:
void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    // Check if click is in signal names column or waveform area
    bool inNamesColumn = event->pos().x() < signalNamesWidth;

    if (event->button() == Qt::MiddleButton)
    {
        // Start middle button drag for horizontal scrolling (waveform area only)
        if (!inNamesColumn)
        {
            isDragging = true;
            dragStartX = event->pos().x() - signalNamesWidth;
            dragStartOffset = timeOffset;
            setCursor(Qt::ClosedHandCursor);
        }
    }
    else if (event->button() == Qt::LeftButton)
    {
        int itemIndex = getItemAtPosition(event->pos());

        if (itemIndex >= 0)
        {
            // Handle multi-selection
            handleMultiSelection(itemIndex, event);

            // Prepare for drag (all item types)
            startDragSignal(itemIndex);
            update();
            emit signalSelected(itemIndex);
        }
        else if (!inNamesColumn)
        {
            // Clear selection when clicking empty space
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            {
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
    if (itemIndex >= 0) {
        if (isGroupItem(itemIndex) || isSpaceItem(itemIndex)) {
            renameItem(itemIndex);
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (isDraggingSignal)
    {
        performDrag(event->pos().y());
    }
    else if (isDragging)
    {
        int delta = dragStartX - (event->pos().x() - signalNamesWidth);
        timeOffset = dragStartOffset + delta;
        updateScrollBar();
        update();
    }

    // Emit time change for cursor position in waveform area
    if (event->pos().x() >= signalNamesWidth)
    {
        int currentTime = xToTime(event->pos().x() - signalNamesWidth);
        emit timeChanged(currentTime);
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)
    {
        if (isDraggingSignal)
        {
            isDraggingSignal = false;
            dragSignalIndex = -1;
            setCursor(Qt::ArrowCursor);
        }
        else if (isDragging)
        {
            isDragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

// In waveformwidget.cpp, update the keyPressEvent method:
void WaveformWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_A && event->modifiers() & Qt::ControlModifier)
    {
        selectAllSignals();
        event->accept();
    }
    else if (event->key() == Qt::Key_Delete)
    {
        // Delete any selected items (signals, spaces, or groups)
        removeSelectedSignals();
        event->accept();
    }
    else
    {
        QWidget::keyPressEvent(event);
    }
}

void WaveformWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier)
    {
        // Ctrl + Wheel for zoom
        if (event->angleDelta().y() > 0)
        {
            zoomIn();
        }
        else
        {
            zoomOut();
        }
    }
    else if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + Wheel for horizontal scrolling
        int scrollAmount = event->angleDelta().y();
        timeOffset += scrollAmount / 2;
        updateScrollBar();
        update();
    }
    else
    {
        // Regular wheel for vertical scrolling (if implemented) or default behavior
        QWidget::wheelEvent(event);
    }
}

void WaveformWidget::updateScrollBar()
{
    if (!vcdParser)
    {
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

    for (const auto &change : changes)
    {
        if (change.timestamp > time)
            break;
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
    if (displayItems.isEmpty())
    {
        qDebug() << "No display items";
        return -1;
    }

    int y = pos.y();
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop)
    {
        qDebug() << "Click above signal area";
        return -1;
    }

    int itemIndex = (y - signalAreaTop) / signalHeight;

    if (itemIndex >= 0 && itemIndex < displayItems.size())
    {
        qDebug() << "Found item at index:" << itemIndex;
        return itemIndex;
    }

    qDebug() << "Item index out of range:" << itemIndex << "display items count:" << displayItems.size();
    return -1;
}

// In waveformwidget.cpp, update the showContextMenu method to ensure the item is selected:
void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex)
{
    QMenu contextMenu(this);

    bool hasSelection = !selectedSignals.isEmpty();
    bool isSignal = isSignalItem(itemIndex);
    bool isSpace = isSpaceItem(itemIndex);
    bool isGroup = isGroupItem(itemIndex);

    // If right-clicking on a specific item, handle selection
    if (itemIndex >= 0)
    {
        // Store the current selection temporarily
        QSet<int> previousSelection = selectedSignals;
        int previousLastSelected = lastSelectedSignal;

        // If the clicked item is not in selection, select only it
        if (!selectedSignals.contains(itemIndex))
        {
            selectedSignals.clear();
            selectedSignals.insert(itemIndex);
            lastSelectedSignal = itemIndex;
            update();
        }

        // Create the context menu
        if (!selectedSignals.isEmpty())
        {
            QString removeText = "Remove Selected";
            contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
            contextMenu.addSeparator();
        }

        // Add rename option for spaces and groups
        if (isSpace || isGroup)
        {
            contextMenu.addAction("Rename", this, [this, itemIndex]()
                                  { renameItem(itemIndex); });
            contextMenu.addSeparator();
        }

        if (isSignal || isSpace)
        {
            contextMenu.addAction("Add Space Above", this, [this, itemIndex]()
                                  { addSpaceAbove(itemIndex); });
            contextMenu.addAction("Add Space Below", this, [this, itemIndex]()
                                  { addSpaceBelow(itemIndex); });
            contextMenu.addSeparator();
        }

        if (selectedSignals.size() > 1)
        {
            // Only allow grouping if all selected items are signals
            bool allSignals = true;
            for (int index : selectedSignals)
            {
                if (!isSignalItem(index))
                {
                    allSignals = false;
                    qDebug() << "Cannot group - item" << index << "is not a signal";
                    break;
                }
            }
            if (allSignals)
            {
                contextMenu.addAction("Group Signals", this, &WaveformWidget::addGroup);
                contextMenu.addSeparator();
            }
            else
            {
                qDebug() << "Grouping disabled - selection contains non-signal items";
            }
        }

        if (isSignal || isSpace || isGroup)
        {
            QString removeText = "Remove";
            if (isSignal)
                removeText = "Remove Signal";
            else if (isSpace)
                removeText = "Remove Space";
            else if (isGroup)
                removeText = "Remove Group";

            contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
        }

        // Execute the context menu
        QAction *selectedAction = contextMenu.exec(pos);

        // If no action was taken (user clicked away), restore previous selection
        if (!selectedAction)
        {
            selectedSignals = previousSelection;
            lastSelectedSignal = previousLastSelected;
            update();
        }
    } else {
        // Right-click on empty area
        if (hasSelection) {
            contextMenu.addAction("Remove Selected", this, &WaveformWidget::removeSelectedSignals);
            contextMenu.addSeparator();
        }
        contextMenu.addAction("Add Space", this, [this]() {
            // Add space at the end with empty name
            SpaceItem space;
            space.name = ""; // Empty name
            displayItems.append(DisplayItem(space));
            update();
        });
    }

    emit contextMenuRequested(pos, itemIndex);
}

// In waveformwidget.cpp, update the promptForName method to allow empty names:
QString WaveformWidget::promptForName(const QString &title, const QString &defaultName)
{
    bool ok;
    QString name = QInputDialog::getText(this, title, "Name:", QLineEdit::Normal, defaultName, &ok);
    if (ok) {
        return name; // Allow empty names
    }
    return defaultName;
}

void WaveformWidget::addSpaceAbove(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "");
    SpaceItem space;
    space.name = name; // Can be empty
    
    displayItems.insert(index, DisplayItem(space));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "");
    SpaceItem space;
    space.name = name; // Can be empty
    
    int insertIndex = index + 1;
    if (insertIndex > displayItems.size()) {
        insertIndex = displayItems.size();
    }
    
    displayItems.insert(insertIndex, DisplayItem(space));
    update();
}

// In waveformwidget.cpp, replace the addGroup method with this corrected version:
void WaveformWidget::renameItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size())
        return;

    DisplayItem &item = displayItems[itemIndex];
    QString currentName = item.getName();
    QString newName = promptForName("Rename", currentName);

    if (!newName.isEmpty() && newName != currentName)
    {
        switch (item.getType())
        {
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

// Add this debug method to help see what's happening:
void WaveformWidget::debugGroupInfo()
{
    qDebug() << "=== GROUP DEBUG INFO ===";
    qDebug() << "Total display items:" << displayItems.size();

    for (int i = 0; i < displayItems.size(); i++)
    {
        if (isGroupItem(i))
        {
            const GroupItem &group = displayItems[i].getGroup();
            qDebug() << "Group at index" << i << "name:" << group.name;
            qDebug() << "  Contains signals at indices:" << group.signalIndices;

            // Verify each signal exists and is a signal
            for (int sigIndex : group.signalIndices)
            {
                if (sigIndex >= 0 && sigIndex < displayItems.size())
                {
                    if (isSignalItem(sigIndex))
                    {
                        qDebug() << "    Signal" << sigIndex << "exists and is valid";
                    }
                    else
                    {
                        qDebug() << "    ERROR: Signal" << sigIndex << "is not a signal! Type:" << displayItems[sigIndex].getType();
                    }
                }
                else
                {
                    qDebug() << "    ERROR: Signal" << sigIndex << "is out of range!";
                }
            }
        }
    }
    qDebug() << "=== END DEBUG INFO ===";
}

// Call this in addGroup after creating the group:
void WaveformWidget::addGroup()
{
    if (selectedSignals.size() < 2) return;
    
    QString name = promptForName("Create Group", "Group");
    if (name.isEmpty()) return;
    
    // Get selected signal indices and sort them
    QList<int> signalIndices = selectedSignals.values();
    std::sort(signalIndices.begin(), signalIndices.end());
    
    qDebug() << "Creating group with explicitly selected signals:" << signalIndices;
    
    // Create group header above the first signal
    GroupItem group;
    group.name = name;
    
    // Only add the explicitly selected signals to the group
    // Don't automatically add other signals
    for (int sigIndex : signalIndices) {
        if (isSignalItem(sigIndex)) {
            group.signalIndices.append(sigIndex);
        }
    }
    
    // Insert group header at the position of the first selected signal
    int groupHeaderIndex = signalIndices.first();
    displayItems.insert(groupHeaderIndex, DisplayItem(group));
    
    qDebug() << "Group created at index" << groupHeaderIndex << "with" << group.signalIndices.size() << "explicitly selected signals";
    
    update();
}