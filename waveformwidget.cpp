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
      isDragging(false), isDraggingItem(false), dragItemIndex(-1), lastSelectedItem(-1)
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

const DisplayItem* WaveformWidget::getItem(int index) const
{
    if (index >= 0 && index < displayItems.size()) {
        return &displayItems[index];
    }
    return nullptr;
}

void WaveformWidget::removeSelectedSignals() {
    if (selectedItems.isEmpty()) return;

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
        } else if (item.type == DisplayItem::Space) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(80, 160, 80, 120));
        } else if (i % 2 == 0) {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(45, 45, 48));
        } else {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(40, 40, 43));
        }

        // Draw item name with appropriate styling
        painter.setPen(QPen(Qt::white));
        QString displayName = item.getName();
        int textIndent = 5;
        
        if (item.type == DisplayItem::Space) {
            painter.setPen(QPen(QColor(150, 255, 150)));
        }
        
        painter.drawText(textIndent, currentY + itemHeight / 2 + 4, displayName);

        // Draw signal width for signals
        if (item.type == DisplayItem::Signal) {
            const VCDSignal& signal = item.signal.signal;
            painter.setPen(QPen(QColor(180, 180, 180)));
            int widthX = signalNamesWidth - 35;
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
        }
        
        currentY += item.getHeight();
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos) {
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

    if (event->modifiers() & Qt::ShiftModifier && lastSelectedItem != -1) {
        // Shift-click: select range from last selected to current
        selectedItems.clear();
        int start = qMin(lastSelectedItem, itemIndex);
        int end = qMax(lastSelectedItem, itemIndex);
        for (int i = start; i <= end; i++) {
            selectedItems.insert(i);
        }
    } else if (event->modifiers() & Qt::ControlModifier) {
        // Ctrl-click: toggle selection
        if (selectedItems.contains(itemIndex)) {
            selectedItems.remove(itemIndex);
        } else {
            selectedItems.insert(itemIndex);
        }
        lastSelectedItem = itemIndex;
    } else {
        // Regular click: single selection
        selectedItems.clear();
        selectedItems.insert(itemIndex);
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

    moveItem(dragItemIndex, newIndex);
}

void WaveformWidget::moveItem(int itemIndex, int newIndex)
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

            // Prepare for drag
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
        if (isSpaceItem(itemIndex)) {
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
        return -1;

    int y = pos.y();
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop)
        return -1;

    int currentY = signalAreaTop;
    for (int i = 0; i < displayItems.size(); i++)
    {
        int itemHeight = displayItems[i].getHeight();
        if (y >= currentY && y < currentY + itemHeight)
            return i;
        currentY += itemHeight;
    }

    return -1;
}

void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex) {
    QMenu contextMenu(this);

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
        if (isSignalItem(itemIndex)) removeText = "Remove Signal";
        else if (isSpaceItem(itemIndex)) removeText = "Remove Space";
        
        contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
        contextMenu.addSeparator();

        // Rename for spaces
        if (isSpaceItem(itemIndex)) {
            contextMenu.addAction("Rename", this, [this, itemIndex]() {
                renameItem(itemIndex);
            });
            contextMenu.addSeparator();
        }

        // Space management
        contextMenu.addAction("Add Space Above", this, [this, itemIndex]() {
            addSpaceAbove(itemIndex);
        });
        contextMenu.addAction("Add Space Below", this, [this, itemIndex]() {
            addSpaceBelow(itemIndex);
        });
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

QString WaveformWidget::promptForName(const QString &title, const QString &defaultName)
{
    bool ok;
    QString name = QInputDialog::getText(this, title, "Name:", QLineEdit::Normal, defaultName, &ok);
    if (ok) {
        return name;
    }
    return defaultName;
}

void WaveformWidget::addSpaceAbove(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "");
    displayItems.insert(index, DisplayItem::createSpace(name));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    if (index < 0 || index >= displayItems.size()) return;
    
    QString name = promptForName("Add Space", "");
    int insertIndex = index + 1;
    if (insertIndex > displayItems.size()) {
        insertIndex = displayItems.size();
    }
    
    displayItems.insert(insertIndex, DisplayItem::createSpace(name));
    update();
}

void WaveformWidget::renameItem(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size())
        return;

    DisplayItem &item = displayItems[itemIndex];
    QString currentName = item.getName();
    QString newName = promptForName("Rename", currentName);

    if (!newName.isEmpty() && newName != currentName && item.type == DisplayItem::Space)
    {
        item.space.name = newName;
        update();
    }
}