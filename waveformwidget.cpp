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
      isDragging(false), isDraggingItem(false), dragItemIndex(-1), lastSelectedItem(-1),
      lastContextSignalIndex(-1)
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
    displayItems.clear();
    timeScale = 1.0;
    timeOffset = 0;
    selectedItems.clear();
    lastSelectedItem = -1;
    updateScrollBar();
    update();
}


void WaveformWidget::setVisibleSignals(const QList<VCDSignal> &visibleSignals)
{
    displayItems.clear();
    for (const auto &signal : visibleSignals) {
        displayItems.append(DisplayItem::createSignal(signal));
    }
    selectedItems.clear();
    lastSelectedItem = -1;
    update();
    emit itemSelected(-1);
}

// Group Management
void WaveformWidget::createGroupFromSelection(const QString& name)
{
    if (selectedItems.size() < 2) return;
    
    // Collect all selected signals and their indices
    QList<VCDSignal> signalsToGroup;
    QList<int> indicesToRemove;
    
    for (int index : selectedItems) {
        if (isSignalItem(index)) {
            signalsToGroup.append(getSignalFromItem(index));
            indicesToRemove.append(index);
        }
    }
    
    if (signalsToGroup.size() < 2) return;
    
    // Remove signals from display (in reverse order)
    std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<int>());
    for (int index : indicesToRemove) {
        displayItems.removeAt(index);
    }
    
    // Create group at the position of the first signal
    int insertPosition = indicesToRemove.last();
    DisplayItem groupItem = DisplayItem::createGroup(name);
    
    // Add signals to the group
    for (const auto& signal : signalsToGroup) {
        groupItem.group.addSignal(DisplayItem::createSignal(signal, true, name).signal);
    }
    
    displayItems.insert(insertPosition, groupItem);
    
    // Clear selection and select the new group
    selectedItems.clear();
    selectedItems.insert(insertPosition);
    lastSelectedItem = insertPosition;
    
    update();
    emit itemSelected(insertPosition);
}

int WaveformWidget::findGroupIndexByName(const QString& name) const {
    for (int i = 0; i < displayItems.size(); i++) {
        if (displayItems[i].type == DisplayItem::Group && displayItems[i].group.name == name) {
            return i;
        }
    }
    return -1;
}

QList<QString> WaveformWidget::getGroupNames() const
{
    QList<QString> names;
    for (const auto& item : displayItems) {
        if (item.type == DisplayItem::Group) {
            names.append(item.group.name);
        }
    }
    return names;
}

const DisplayItem* WaveformWidget::getItem(int index) const
{
    if (index >= 0 && index < displayItems.size()) {
        return &displayItems[index];
    }
    return nullptr;
}


void WaveformWidget::addSignalsToGroup(const QList<int>& signalIndices, const QString& groupName) {
    int groupIndex = findGroupIndexByName(groupName);
    if (groupIndex == -1) return;
    
    // Collect signals to add
    QList<VCDSignal> signalsToAdd;
    QList<int> indicesToRemove;
    
    for (int index : signalIndices) {
        if (isSignalItem(index)) {
            signalsToAdd.append(getSignalFromItem(index));
            indicesToRemove.append(index);
        }
    }
    
    if (signalsToAdd.isEmpty()) return;
    
    // Remove signals from display (in reverse order)
    std::sort(indicesToRemove.begin(), indicesToRemove.end(), std::greater<int>());
    for (int index : indicesToRemove) {
        displayItems.removeAt(index);
    }
    
    // Add signals to group
    DisplayItem& groupItem = displayItems[groupIndex];
    for (const auto& signal : signalsToAdd) {
        groupItem.group.addSignal(DisplayItem::createSignal(signal, true, groupName).signal);
    }
    
    update();
}

void WaveformWidget::onAddToGroupActionTriggered()
{
    if (lastContextSignalIndex != -1 && !lastContextGroupName.isEmpty()) {
        QList<int> indices = {lastContextSignalIndex};
        addSignalsToGroup(indices, lastContextGroupName);
    }
}

void WaveformWidget::convertToFreeSignal(int itemIndex) {
    if (!isSignalItem(itemIndex)) return;
    
    DisplayItem& item = displayItems[itemIndex];
    if (item.signal.isInGroup) {
        // Remove from group
        int groupIndex = findGroupIndexByName(item.signal.groupName);
        if (groupIndex != -1) {
            displayItems[groupIndex].group.removeSignal(item.signal.signal.identifier);
        }
        // Convert to free signal
        item.signal.isInGroup = false;
        item.signal.groupName.clear();
    }
}

void WaveformWidget::removeSignalsFromGroup(const QList<int>& signalIndices)
{
    for (int index : signalIndices) {
        convertToFreeSignal(index);
    }
    update();
}

void WaveformWidget::removeSignalFromAllGroups(const QString& identifier) {
    for (int i = 0; i < displayItems.size(); i++) {
        if (displayItems[i].type == DisplayItem::Group) {
            displayItems[i].group.removeSignal(identifier);
        }
    }
}

void WaveformWidget::removeSelectedSignals() {
    if (selectedItems.isEmpty()) return;

    // First remove signals from any groups they belong to
    for (int index : selectedItems) {
        if (isSignalItem(index)) {
            removeSignalFromAllGroups(displayItems[index].signal.signal.identifier);
        }
    }

    // Remove items in reverse order
    QList<int> indices = selectedItems.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());
    
    for (int index : indices) {
        if (index >= 0 && index < displayItems.size()) {
            displayItems.removeAt(index);
        }
    }
    
    selectedItems.clear();
    lastSelectedItem = -1;
    update();
    emit itemSelected(-1);
}

void WaveformWidget::updateAllGroupIndices()
{
    // Clear all group signal indices first
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            displayItems[i].group.groupSignals.clear();
        }
    }
    
    // For each group, only add signals that were explicitly selected when creating the group
    // Don't automatically add signals based on position
    // Groups should only contain signals that were explicitly added to them
    qDebug() << "Group system: Manual signal tracking - no automatic grouping";
}

void WaveformWidget::updateGroupIndicesAfterDeletion(const QList<int> &deletedIndices)
{
    for (int i = 0; i < displayItems.size(); i++)
    {
        if (isGroupItem(i))
        {
            // For the new architecture, we don't need to update indices
            // because signals are stored by identifier, not by display index
            Q_UNUSED(deletedIndices)
        }
    }
}

// Update the selectAllSignals method to select all item types:
void WaveformWidget::selectAllSignals()
{
    selectedItems.clear();
    for (int i = 0; i < displayItems.size(); i++)
    {
        selectedItems.insert(i);
    }
    lastSelectedItem = displayItems.size() - 1;
    update();
    emit itemSelected(selectedItems.isEmpty() ? -1 : *selectedItems.begin());
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

// Painting
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

    drawSignalNamesColumn(painter);
    drawWaveformArea(painter);
}

bool WaveformWidget::isSignalInGroup(int index) const
{
    if (!isSignalItem(index)) return false;

    const DisplayItem& item = displayItems[index];
    return item.signal.isInGroup;
}

int WaveformWidget::getGroupBottomIndex(int groupIndex) const
{
    if (!isGroupItem(groupIndex))
        return -1;

    const SignalGroup& group = displayItems[groupIndex].group;
    if (group.groupSignals.isEmpty())
        return -1;

    // In the new architecture, signals are stored within the group object
    // not as separate display items, so we don't have a "bottom index" concept
    return -1;
}


void WaveformWidget::selectGroup(int groupIndex)
{
    if (!isGroupItem(groupIndex))
        return;

    selectedItems.clear();
    const SignalGroup& group = displayItems[groupIndex].group;

    // Select the group header
    selectedItems.insert(groupIndex);

    lastSelectedItem = groupIndex;
    update();
    emit itemSelected(groupIndex);
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

    // Draw selection count if multiple items selected
    if (selectedItems.size() > 1) {
        painter.drawText(signalNamesWidth - 50, timeMarkersHeight - 8, 
                        QString("(%1 selected)").arg(selectedItems.size()));
    }
    
    int currentY = topMargin + timeMarkersHeight;
    
    for (int i = 0; i < displayItems.size(); i++) {
        const auto& item = displayItems[i];
        int itemHeight = item.getHeight();
        
        // Draw background based on selection and type
        if (selectedItems.contains(i)) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(60, 60, 90));
        } else if (item.type == DisplayItem::Group) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(80, 80, 160, 180));
        } else if (item.type == DisplayItem::Space) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(80, 160, 80, 120));
        } else if (item.signal.isInGroup) {
            // Grouped signal - indented background
            painter.fillRect(20, currentY, signalNamesWidth - 20, itemHeight, 
                           i % 2 == 0 ? QColor(50, 50, 80, 100) : QColor(45, 45, 70, 100));
        } else if (i % 2 == 0) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(40, 40, 43));
        }

        // Draw item name with appropriate styling and indentation
        painter.setPen(QPen(Qt::white));
        QString displayName = item.getName();
        int textIndent = 5;
        
        if (item.type == DisplayItem::Group) {
            painter.setPen(QPen(QColor(200, 200, 255)));
        } else if (item.type == DisplayItem::Space) {
            painter.setPen(QPen(QColor(150, 255, 150)));
        } else if (item.signal.isInGroup) {
            textIndent = 25;
            displayName = "    " + displayName;
        }
        
        painter.drawText(textIndent, currentY + itemHeight / 2 + 4, displayName);

        // Draw signal width for signals
        if (item.type == DisplayItem::Signal) {
            const VCDSignal& signal = item.signal.signal;
            painter.setPen(QPen(QColor(180, 180, 180)));
            int widthX = signalNamesWidth - 35;
            if (item.signal.isInGroup) {
                widthX -= 20; // Adjust for indentation
            }
            painter.drawText(widthX, currentY + itemHeight / 2 + 4,
                            QString("[%1:0]").arg(signal.width - 1));
        }

        // Draw horizontal separator
        painter.setPen(QPen(QColor(80, 80, 80)));
        painter.drawLine(0, currentY + itemHeight, signalNamesWidth, currentY + itemHeight);
        
        currentY += itemHeight;
    }
}

void WaveformWidget::drawWaveformArea(QPainter &painter) {
    // Implementation similar to previous version
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
    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep)
    {
        int x = timeToX(time);
        painter.drawLine(x, 0, x, height());

        painter.setPen(QPen(Qt::white));
        painter.drawText(x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    }

    // Draw horizontal lines for items
    int currentY = topMargin + timeMarkersHeight;
    for (int i = 0; i <= displayItems.size(); i++)
    {
        painter.drawLine(0, currentY, width() - signalNamesWidth, currentY);
        if (i < displayItems.size()) {
            currentY += displayItems[i].getHeight();
        }
    }

    // Draw selection highlight for all selected items
    currentY = topMargin + timeMarkersHeight;
    for (int i = 0; i < displayItems.size(); i++)
    {
        int itemHeight = displayItems[i].getHeight();
        if (selectedItems.contains(i))
        {
            painter.fillRect(0, currentY, width() - signalNamesWidth, itemHeight, QColor(60, 60, 90));
        }
        currentY += itemHeight;
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

void WaveformWidget::drawSignals(QPainter &painter) {
    int currentY = topMargin + timeMarkersHeight;
    
    for (int i = 0; i < displayItems.size(); i++) {
        const auto& item = displayItems[i];
        
        if (item.type == DisplayItem::Signal) {
            drawSignalWaveform(painter, item.signal.signal, currentY);
        } else if (item.type == DisplayItem::Group && !item.group.collapsed) {
            // Draw all signals in the group
            int signalY = currentY + 30;
            for (const auto& groupSignal : item.group.groupSignals) {
                drawSignalWaveform(painter, groupSignal.signal, signalY);
                signalY += 30;
            }
        }
        
        currentY += item.getHeight();
    }
}

void WaveformWidget::drawGroupWaveforms(QPainter &painter, const SignalGroup &group, int groupY) {
    int signalY = groupY + 30; // Start below group header
    
    for (const auto& groupSignal : group.groupSignals) {
        drawSignalWaveform(painter, groupSignal.signal, signalY);
        signalY += 30;
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos) {
    // Keep existing waveform drawing implementation
    const auto &changes = vcdParser->getValueChanges().value(signal.identifier);
    if (changes.isEmpty()) return;

    QColor signalColor = QColor::fromHsv((qHash(signal.identifier) % 360), 180, 220);
    painter.setPen(QPen(signalColor, 2));

    int signalMidY = yPos + 15;
    int highLevel = yPos + 5;
    int lowLevel = yPos + 25;

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
            selectedItems.clear();
            selectedItems.insert(itemIndex);
            lastSelectedItem = itemIndex;
            update();
            emit itemSelected(itemIndex);
            return;
        }
    }

    if (event->modifiers() & Qt::ShiftModifier && lastSelectedItem != -1) {
        // Shift-click: select range from last selected to current
        selectedItems.clear();
        int start = qMin(lastSelectedItem, itemIndex);
        int end = qMax(lastSelectedItem, itemIndex);
        for (int i = start; i <= end; i++) {
            selectedItems.insert(i);
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl-click: toggle selection (all item types)
        if (selectedItems.contains(itemIndex)) {
            selectedItems.remove(itemIndex);
        } else {
            selectedItems.insert(itemIndex);
        }
        lastSelectedItem = itemIndex;
    } else {
        // Regular click: single selection (all item types)
        selectedItems.clear();
        selectedItems.insert(itemIndex);
        lastSelectedItem = itemIndex;
    }
    
    update();
    emit itemSelected(itemIndex);
}

void WaveformWidget::updateSelection(int itemIndex, bool isMultiSelect)
{
    if (!isMultiSelect)
    {
        selectedItems.clear();
        if (isSignalItem(itemIndex))
        {
            selectedItems.insert(itemIndex);
        }
        lastSelectedItem = itemIndex;
    }
    update();
    emit itemSelected(itemIndex);
}

int WaveformWidget::getItemYPosition(int index) const
{
    if (index < 0 || index >= displayItems.size()) return -1;
    
    int yPos = topMargin + timeMarkersHeight;
    for (int i = 0; i < index; i++) {
        yPos += displayItems[i].getHeight();
    }
    return yPos;
}

void WaveformWidget::startDrag(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size()) return;
    
    isDraggingItem = true;
    dragItemIndex = itemIndex;
    dragStartPos = QCursor::pos();
    dragStartY = getItemYPosition(itemIndex);
    setCursor(Qt::ClosedHandCursor);
}

void WaveformWidget::updateSignalGroupMembership(int signalIndex)
{
    if (!isSignalItem(signalIndex)) return;
    
    // Remove signal from all groups first
    for (int i = 0; i < displayItems.size(); i++) {
        if (isGroupItem(i)) {
            displayItems[i].group.removeSignal(displayItems[signalIndex].signal.signal.identifier);
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
        SignalGroup& group = displayItems[groupHeaderIndex].group;
        // Check if this signal is below the group header and above the next group/space
        bool shouldBeInGroup = true;
        for (int i = groupHeaderIndex + 1; i < signalIndex; i++) {
            if (isGroupItem(i) || isSpaceItem(i)) {
                shouldBeInGroup = false;
                break;
            }
        }
        
        if (shouldBeInGroup && !group.containsSignal(displayItems[signalIndex].signal.signal.identifier)) {
            DisplaySignal displaySignal;
            displaySignal.signal = displayItems[signalIndex].signal.signal;
            displaySignal.isInGroup = true;
            displaySignal.groupName = group.name;
            group.addSignal(displaySignal);
            qDebug() << "Added signal" << signalIndex << "to group at" << groupHeaderIndex;
        }
    }
}

void WaveformWidget::performDrag(int mouseY)
{
    if (!isDraggingItem || dragItemIndex < 0) return;

    int newIndex = -1;
    int currentY = topMargin + timeMarkersHeight;
    
    // Find new position based on mouse Y
    for (int i = 0; i < displayItems.size(); i++) {
        int itemHeight = displayItems[i].getHeight();
        if (mouseY >= currentY && mouseY < currentY + itemHeight) {
            newIndex = i;
            break;
        }
        currentY += itemHeight;
    }
    
    if (newIndex == -1) newIndex = displayItems.size() - 1;
    if (newIndex == dragItemIndex) return;

    // Handle group drag - move all signals together
    if (isGroupItem(dragItemIndex)) {
        moveGroup(dragItemIndex, newIndex);
    } else {
        moveFreeItem(dragItemIndex, newIndex);
    }
}

void WaveformWidget::moveFreeItem(int itemIndex, int newIndex)
{
    DisplayItem item = displayItems[itemIndex];
    displayItems.removeAt(itemIndex);
    
    // Adjust new index if we're moving past the original position
    if (newIndex > itemIndex) newIndex--;
    
    displayItems.insert(newIndex, item);
    dragItemIndex = newIndex;
    
    // Update selection
    if (selectedItems.contains(itemIndex)) {
        selectedItems.remove(itemIndex);
        selectedItems.insert(newIndex);
        lastSelectedItem = newIndex;
    }
    
    update();
}

void WaveformWidget::updateGroupDisplayIndices()
{
    // In the new architecture, groups store signals by identifier
    // so we don't need to update display indices
    // This method is kept for compatibility with older code
}

void WaveformWidget::moveGroup(int groupIndex, int newIndex)
{
    auto group = displayItems[groupIndex];
    displayItems.removeAt(groupIndex);
    
    // Adjust new index if we're moving past the original position
    if (newIndex > groupIndex) newIndex--;
    
    displayItems.insert(newIndex, group);
    dragItemIndex = newIndex;
    
    // Update selection
    if (selectedItems.contains(groupIndex)) {
        selectedItems.remove(groupIndex);
        selectedItems.insert(newIndex);
        lastSelectedItem = newIndex;
    }
    
    update();
}

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
            startDrag(itemIndex);
            update();
            emit itemSelected(itemIndex);
        }
        else if (!inNamesColumn)
        {
            // Clear selection when clicking empty space
            if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
            {
                selectedItems.clear();
                lastSelectedItem = -1;
                update();
                emit itemSelected(-1);
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
    if (isDraggingItem)
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
        if (isDraggingItem)
        {
            isDraggingItem = false;
            dragItemIndex = -1;
            setCursor(Qt::ArrowCursor);
        }
        else if (isDragging)
        {
            isDragging = false;
            setCursor(Qt::ArrowCursor);
        }
    }
}

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

    int currentY = signalAreaTop;
    for (int i = 0; i < displayItems.size(); i++)
    {
        int itemHeight = displayItems[i].getHeight();
        if (y >= currentY && y < currentY + itemHeight)
        {
            qDebug() << "Found item at index:" << i;
            return i;
        }
        currentY += itemHeight;
    }

    qDebug() << "Click below all items";
    return -1;
}

void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex) {
    QMenu contextMenu(this);

    bool hasSelection = !selectedItems.isEmpty();
    bool isSignal = isSignalItem(itemIndex);
    bool isSpace = isSpaceItem(itemIndex);
    bool isGroup = isGroupItem(itemIndex);

    if (itemIndex >= 0) {
        // Ensure the clicked item is selected
        if (!selectedItems.contains(itemIndex)) {
            selectedItems.clear();
            selectedItems.insert(itemIndex);
            lastSelectedItem = itemIndex;
            update();
        }

        // Remove option
        QString removeText = "Remove";
        if (isSignal) removeText = "Remove Signal";
        else if (isSpace) removeText = "Remove Space";
        else if (isGroup) removeText = "Remove Group";
        
        contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
        contextMenu.addSeparator();

        // Rename for spaces and groups
        if (isSpace || isGroup) {
            contextMenu.addAction("Rename", this, [this, itemIndex]() {
                renameItem(itemIndex);
            });
            contextMenu.addSeparator();
        }

        // Group management
        if (isSignal) {
            QMenu* addToGroupMenu = contextMenu.addMenu("Add to Group");
            QList<QString> groupNames = getGroupNames();
            
            for (const QString& groupName : groupNames) {
                addToGroupMenu->addAction(groupName, this, [this, itemIndex, groupName]() {
                    QList<int> indices = {itemIndex};
                    addSignalsToGroup(indices, groupName);
                });
            }
            
            if (groupNames.isEmpty()) {
                addToGroupMenu->setEnabled(false);
            }
            contextMenu.addSeparator();
        }

        // Space management
        if (isSignal || isSpace) {
            contextMenu.addAction("Add Space Above", this, [this, itemIndex]() {
                addSpaceAbove(itemIndex);
            });
            contextMenu.addAction("Add Space Below", this, [this, itemIndex]() {
                addSpaceBelow(itemIndex);
            });
            contextMenu.addSeparator();
        }

        // Group creation
        if (selectedItems.size() > 1) {
            bool allSignals = true;
            for (int index : selectedItems) {
                if (!isSignalItem(index)) {
                    allSignals = false;
                    break;
                }
            }
            if (allSignals) {
                contextMenu.addAction("Create Group", this, [this]() {
                    QString name = promptForName("Create Group", "Group");
                    if (!name.isEmpty()) {
                        createGroupFromSelection(name);
                    }
                });
            }
        }
    }

    QAction* selectedAction = contextMenu.exec(pos);
    if (!selectedAction && itemIndex >= 0) {
        // Restore selection if menu was cancelled
        selectedItems.clear();
        selectedItems.insert(itemIndex);
        update();
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
    DisplaySpace space;
    space.name = name;
    
    displayItems.insert(index, DisplayItem::createSpace(name));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "");
    DisplaySpace space;
    space.name = name;
    
    int insertIndex = index + 1;
    if (insertIndex > displayItems.size()) {
        insertIndex = displayItems.size();
    }
    
    displayItems.insert(insertIndex, DisplayItem::createSpace(name));
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
        switch (item.type)
        {
        case DisplayItem::Space:
            item.space.name = newName;
            break;
        case DisplayItem::Group:
            item.group.name = newName;
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
            const SignalGroup &group = displayItems[i].group;
            qDebug() << "Group at index" << i << "name:" << group.name;
            qDebug() << "  Contains signals:" << group.groupSignals.size();

            // Verify each signal exists
            for (const auto& groupSignal : group.groupSignals)
            {
                qDebug() << "    Signal:" << groupSignal.signal.name << "identifier:" << groupSignal.signal.identifier;
            }
        }
    }
    qDebug() << "=== END DEBUG INFO ===";
}

// Call this in addGroup after creating the group:
void WaveformWidget::addGroup()
{
    if (selectedItems.size() < 2) return;
    
    QString name = promptForName("Create Group", "Group");
    if (name.isEmpty()) return;
    
    // Get selected signal indices and sort them
    QList<int> signalIndices = selectedItems.values();
    std::sort(signalIndices.begin(), signalIndices.end());
    
    qDebug() << "Creating group with explicitly selected signals:" << signalIndices;
    
    // Create group header above the first signal
    SignalGroup group;
    group.name = name;
    
    // Only add the explicitly selected signals to the group
    // Don't automatically add other signals
    for (int sigIndex : signalIndices) {
        if (isSignalItem(sigIndex)) {
            DisplaySignal displaySignal;
            displaySignal.signal = getSignalFromItem(sigIndex);
            displaySignal.isInGroup = true;
            displaySignal.groupName = name;
            group.groupSignals.append(displaySignal);
        }
    }
    
    // Insert group header at the position of the first selected signal
    int groupHeaderIndex = signalIndices.first();
    displayItems.insert(groupHeaderIndex, DisplayItem::createGroup(name));
    
    // Update the group with the signals
    displayItems[groupHeaderIndex].group = group;
    
    qDebug() << "Group created at index" << groupHeaderIndex << "with" << group.groupSignals.size() << "explicitly selected signals";
    
    update();
}
