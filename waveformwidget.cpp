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

WaveformWidget::WaveformWidget(QWidget *parent)
    : QWidget(parent),
      vcdParser(nullptr),
      timeScale(1.0),
      timeOffset(0),
      signalNamesWidth(250),
      valuesColumnWidth(120),
      timeMarkersHeight(30),
      topMargin(0),
      signalHeight(24),
      busHeight(30),
      lineWidth(1),
      isDragging(false),
      isDraggingItem(false),
      dragItemIndex(-1),
      dragStartX(0),
      dragStartOffset(0),
      dragStartY(0),
      lastSelectedItem(-1),
      highlightBusses(false),
      busDisplayFormat(Hex),
      draggingNamesSplitter(false),
      draggingValuesSplitter(false),
      cursorTime(0),
      showCursor(true),
      verticalOffset(0),
      isSearchActive(false),
      visibleSignalBuffer(50),
      MAX_CACHED_SIGNALS(1000) // Add this initialization
{
    qDebug() << "WaveformWidget constructor started";
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    qDebug() << "Creating scrollbars...";

    // In the constructor, update the horizontal scrollbar connection:
    horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    connect(horizontalScrollBar, &QScrollBar::valueChanged, [this](int value)
            {
    timeOffset = value;
    update(); });

    // Add vertical scrollbar
    verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    connect(verticalScrollBar, &QScrollBar::valueChanged, [this](int value)
            {
        verticalOffset = value;
        updateVisibleSignals();
        update(); });
    qDebug() << "WaveformWidget constructor completed";

    // Add vertical scrollbar
    verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    connect(verticalScrollBar, &QScrollBar::valueChanged, [this](int value)
            {
        verticalOffset = value;
        updateVisibleSignals();
        update(); });
}

void WaveformWidget::setVcdData(VCDParser *parser)
{
    vcdParser = parser;
    displayItems.clear();

    // Reset zoom to safe levels when loading new data
    if (timeScale > 100.0 || timeScale < 0.01)
    {
        timeScale = 1.0;
        timeOffset = 0;
    }

    selectedItems.clear();
    lastSelectedItem = -1;

    // Apply zoom fit automatically when VCD data is loaded
    if (vcdParser && vcdParser->getEndTime() > 0)
    {
        zoomFit();
    }
    else
    {
        updateScrollBar();
    }

    update();
}

const DisplayItem *WaveformWidget::getItem(int index) const
{
    if (index >= 0 && index < displayItems.size())
    {
        return &displayItems[index];
    }
    return nullptr;
}

void WaveformWidget::removeSelectedSignals()
{
    if (selectedItems.isEmpty())
        return;

    // Remove items in reverse order
    QList<int> indices = selectedItems.values();
    std::sort(indices.begin(), indices.end(), std::greater<int>());

    for (int index : indices)
    {
        if (index >= 0 && index < displayItems.size())
        {
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
    // Very conservative maximum zoom
    if (timeScale >= 50.0)
    { // Reduced from 100.0
        qDebug() << "Maximum zoom reached at 50.0";
        return;
    }

    double newTimeScale = timeScale * 1.2;

    if (newTimeScale > 50.0)
    {
        newTimeScale = 50.0;
    }

    double oldTimeScale = timeScale;
    timeScale = newTimeScale;

    qDebug() << "Zoom in:" << oldTimeScale << "->" << timeScale;

    updateScrollBar();
    update();
}

void WaveformWidget::zoomOut()
{
    // Very conservative minimum zoom
    if (timeScale <= 0.1)
    { // Increased from 0.05
        qDebug() << "Minimum zoom reached at 0.1";
        return;
    }

    double newTimeScale = timeScale / 1.2;

    if (newTimeScale < 0.1)
    {
        newTimeScale = 0.1;
    }

    double oldTimeScale = timeScale;
    timeScale = newTimeScale;

    qDebug() << "Zoom out:" << oldTimeScale << "->" << timeScale;

    updateScrollBar();
    update();
}

void WaveformWidget::zoomFit()
{
    if (!vcdParser || vcdParser->getEndTime() <= 0)
    {
        timeScale = 1.0;
        timeOffset = 0;
        updateScrollBar();
        update();
        return;
    }

    int availableWidth = width() - signalNamesWidth - valuesColumnWidth - 20;

    // Use the same margins as scrolling - UPDATE THIS LINE:
    const int LEFT_MARGIN = -50; // -10 time units (negative time)
    const int RIGHT_MARGIN = 50; // 100 time units after end

    int totalTimeRange = vcdParser->getEndTime() + RIGHT_MARGIN - LEFT_MARGIN; // Note: subtract LEFT_MARGIN because it's negative

    if (availableWidth <= 10)
    {
        timeScale = 1.0;
    }
    else if (totalTimeRange <= 0)
    {
        timeScale = 1.0;
    }
    else
    {
        timeScale = static_cast<double>(availableWidth) / totalTimeRange;
    }

    timeScale = qMax(0.001, qMin(1000.0, timeScale));
    timeOffset = 0;

    updateScrollBar();
    update();
}

void WaveformWidget::resetSignalColors()
{
    signalColors.clear();
    update();
}

void WaveformWidget::setHighlightBusses(bool highlight)
{
    highlightBusses = highlight;
    update();
}

void WaveformWidget::setBusDisplayFormat(BusFormat format)
{
    busDisplayFormat = format;
    update();
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    // Global safety check - reset if zoom is completely unreasonable
    if (timeScale > 1000.0 || timeScale < 0.001) {
        qDebug() << "Global emergency: Resetting unreasonable zoom:" << timeScale;
        timeScale = 1.0;
        timeOffset = 0;
    }

    // Update which signals are visible before painting
    updateVisibleSignals();

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
    drawSignalValuesColumn(painter);
    drawWaveformArea(painter);
    drawTimeCursor(painter);
    
    // Debug: show scroll info
    // painter.setPen(QPen(Qt::yellow));
    // painter.drawText(10, 20, QString("VOffset: %1/%2").arg(verticalOffset).arg(verticalScrollBar->maximum()));
}

void WaveformWidget::drawSignalNamesColumn(QPainter &painter)
{
    // Draw signal names column background
    painter.fillRect(0, 0, signalNamesWidth, height(), QColor(37, 37, 38));

    // Draw names splitter
    painter.fillRect(signalNamesWidth - 1, 0, 2, height(), QColor(100, 100, 100));

    // Draw pinned header (always visible)
    painter.fillRect(0, 0, signalNamesWidth, timeMarkersHeight, QColor(60, 60, 60));
    painter.setPen(QPen(Qt::white));
    painter.drawText(5, timeMarkersHeight - 8, "Signal Name");

    // Draw pinned search bar

    // Set up clipping to exclude pinned areas from scrolling
    painter.setClipRect(0, topMargin + timeMarkersHeight, signalNamesWidth, height() - (topMargin + timeMarkersHeight));

    int currentY = topMargin + timeMarkersHeight - verticalOffset;

    // Only process visible signals
    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        // Calculate actual height based on signal type and configurable heights
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

        // Skip drawing if item is outside visible area
        if (currentY + itemHeight < topMargin + timeMarkersHeight)
        {
            currentY += itemHeight;
            continue;
        }
        if (currentY > height())
        {
            break;
        }

        // Only draw if this signal is in our visible set (or close to it)
        bool shouldDraw = visibleSignalIndices.contains(i);

        if (shouldDraw)
        {
            // Draw background based on selection and type
            bool isSelected = selectedItems.contains(i);
            bool isSearchMatch = searchResults.contains(i); // Keep this line

            if (isSelected)
            {
                painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(60, 60, 90));
            }
            else if (isSearchActive && isSearchMatch) // Keep this condition
            {
                painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(80, 80, 120, 150));
            }
            else if (item.type == DisplayItem::Space)
            {
                painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(80, 160, 80, 120));
            }
            else if (i % 2 == 0)
            {
                painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(45, 45, 48));
            }
            else
            {
                painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(40, 40, 43));
            }

            // Draw item name with appropriate styling
            if (isSelected)
            {
                painter.setPen(QPen(Qt::white));
            }
            else if (isSearchActive && isSearchMatch) // Keep this condition
            {
                painter.setPen(QPen(QColor(200, 200, 255)));
            }
            else if (item.type == DisplayItem::Space)
            {
                painter.setPen(QPen(QColor(150, 255, 150)));
            }
            else
            {
                painter.setPen(QPen(Qt::white));
            }

            QString displayName = item.getName();
            int textIndent = 5;

            painter.drawText(textIndent, currentY + itemHeight / 2 + 4, displayName);

            // Draw horizontal separator
            painter.setPen(QPen(QColor(80, 80, 80)));
            painter.drawLine(0, currentY + itemHeight, signalNamesWidth, currentY + itemHeight);
        }

        currentY += itemHeight;
    }

    // Reset clipping
    painter.setClipping(false);
}

void WaveformWidget::drawSignalValuesColumn(QPainter &painter)
{
    if (!showCursor || cursorTime < 0 || !vcdParser)
        return;

    int valuesColumnStart = signalNamesWidth;

    // Draw values column background
    painter.fillRect(valuesColumnStart, 0, valuesColumnWidth, height(), QColor(50, 50, 60));

    // Draw values splitter
    painter.fillRect(valuesColumnStart + valuesColumnWidth - 1, 0, 2, height(), QColor(100, 100, 100));

    // Draw pinned header (always visible)
    painter.fillRect(valuesColumnStart, 0, valuesColumnWidth, timeMarkersHeight, QColor(70, 70, 80));
    painter.setPen(QPen(Qt::white));
    painter.drawText(valuesColumnStart + 5, timeMarkersHeight - 8, "Value @ Time");

    // Set up clipping to exclude pinned areas from scrolling
    painter.setClipRect(valuesColumnStart, topMargin + timeMarkersHeight, valuesColumnWidth, height() - (topMargin + timeMarkersHeight));

    int currentY = topMargin + timeMarkersHeight - verticalOffset;

    // Only process visible signals
    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

        // Skip drawing if item is outside visible area
        if (currentY + itemHeight < topMargin + timeMarkersHeight)
        {
            currentY += itemHeight;
            continue;
        }
        if (currentY > height())
        {
            break;
        }

        // Only draw if this signal is in our visible set
        bool shouldDraw = visibleSignalIndices.contains(i);

        if (shouldDraw)
        {
            // Draw background for this row
            bool isSelected = selectedItems.contains(i);
            bool isSearchMatch = searchResults.contains(i);

            if (isSelected)
            {
                painter.fillRect(valuesColumnStart, currentY, valuesColumnWidth, itemHeight, QColor(60, 60, 90));
            }
            else if (isSearchActive && isSearchMatch)
            {
                painter.fillRect(valuesColumnStart, currentY, valuesColumnWidth, itemHeight, QColor(80, 80, 120, 150));
            }
            else if (i % 2 == 0)
            {
                painter.fillRect(valuesColumnStart, currentY, valuesColumnWidth, itemHeight, QColor(50, 50, 60));
            }
            else
            {
                painter.fillRect(valuesColumnStart, currentY, valuesColumnWidth, itemHeight, QColor(45, 45, 55));
            }

            if (item.type == DisplayItem::Signal)
            {
                const VCDSignal &signal = item.signal.signal;
                QString value = getSignalValueAtTime(signal.identifier, cursorTime);

                // Format the value based on signal type
                QString displayValue;
                if (signal.width > 1)
                {
                    displayValue = formatBusValue(value);
                }
                else
                {
                    displayValue = value.toUpper();
                }

                painter.setPen(QPen(Qt::white));
                painter.drawText(valuesColumnStart + 5, currentY + itemHeight / 2 + 4, displayValue);
            }

            // Draw horizontal separator
            painter.setPen(QPen(QColor(80, 80, 80)));
            painter.drawLine(valuesColumnStart, currentY + itemHeight,
                             valuesColumnStart + valuesColumnWidth, currentY + itemHeight);
        }

        currentY += itemHeight;
    }

    // Reset clipping
    painter.setClipping(false);
}

void WaveformWidget::drawWaveformArea(QPainter &painter)
{
    int waveformStartX = signalNamesWidth + valuesColumnWidth;

    // Draw pinned timeline background
    painter.fillRect(waveformStartX, 0, width() - waveformStartX, timeMarkersHeight, QColor(30, 30, 30));

    // Draw grid lines in timeline area
    painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    int startTime = xToTime(0);
    int endTime = xToTime(width() - waveformStartX);
    int timeStep = calculateTimeStep(startTime, endTime);

    for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep)
    {
        int x = timeToX(time);
        painter.drawLine(waveformStartX + x, 0, waveformStartX + x, timeMarkersHeight);

        painter.setPen(QPen(Qt::white));
        painter.drawText(waveformStartX + x + 2, timeMarkersHeight - 5, QString::number(time));
        painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    }

    // Set up clipping for scrollable waveform area (exclude pinned timeline)
    painter.setClipRect(waveformStartX, timeMarkersHeight, width() - waveformStartX, height() - timeMarkersHeight);
    painter.translate(waveformStartX, -verticalOffset); // Apply vertical offset

    // Draw background for scrollable area
    painter.fillRect(0, timeMarkersHeight, width() - waveformStartX, calculateTotalHeight(), QColor(30, 30, 30));

    if (!displayItems.isEmpty())
    {
        // drawGrid(painter);
        drawSignals(painter);
    }

    painter.translate(-waveformStartX, verticalOffset); // Reset translation
    painter.setClipping(false);
}

void WaveformWidget::drawTimeCursor(QPainter &painter)
{
    if (!showCursor || cursorTime < 0)
        return;

    int waveformStartX = signalNamesWidth + valuesColumnWidth;
    int cursorX = timeToX(cursorTime);

    // Only draw if cursor is within visible waveform area
    if (cursorX < 0 || cursorX > (width() - waveformStartX))
        return;

    // Draw vertical cursor line through entire height (including pinned areas)
    painter.setPen(QPen(Qt::yellow, 2, Qt::DashLine));
    painter.drawLine(waveformStartX + cursorX, 0, waveformStartX + cursorX, height());

    // Draw cursor time label at top (in pinned timeline area)
    painter.setPen(QPen(Qt::white));
    QString timeText = QString("Time: %1").arg(cursorTime);

    // Calculate text width to make rectangle dynamic
    int textWidth = painter.fontMetrics().horizontalAdvance(timeText) + 10; // +10 for padding
    int textHeight = 20;

    // Ensure the rectangle doesn't go off-screen to the right
    int maxX = width() - textWidth - 5; // 5px margin from right edge
    int labelX = waveformStartX + cursorX + 5;

    // If the label would go off-screen, position it to the left of the cursor
    if (labelX + textWidth > width())
    {
        labelX = waveformStartX + cursorX - textWidth - 5;
    }

    // Also ensure it doesn't go off-screen to the left
    labelX = qMax(waveformStartX + 5, labelX);

    QRect timeRect(labelX, 5, textWidth, textHeight);
    painter.fillRect(timeRect, QColor(0, 0, 0, 200));
    painter.drawText(timeRect, Qt::AlignCenter, timeText);
}

// void WaveformWidget::drawGrid(QPainter &painter)
// {
//     painter.setPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));

//     int startTime = xToTime(0);
//     int endTime = xToTime(width() - signalNamesWidth - valuesColumnWidth);

//     int timeStep = calculateTimeStep(startTime, endTime);
//     for (int time = (startTime / timeStep) * timeStep; time <= endTime; time += timeStep)
//     {
//         int x = timeToX(time);
//         // Draw vertical lines only in the scrollable area (starting from timeMarkersHeight)
//         painter.drawLine(x, timeMarkersHeight, x, calculateTotalHeight());
//     }

//     // Draw horizontal lines for items (only in scrollable area)
//     int currentY = topMargin + timeMarkersHeight;
//     for (int i = 0; i < displayItems.size(); i++)
//     {
//         const auto &item = displayItems[i];
//         int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

//         painter.drawLine(0, currentY, width() - signalNamesWidth - valuesColumnWidth, currentY);
//         currentY += itemHeight;
//     }

//     // Draw selection highlight for all selected items (only in scrollable area)
//     currentY = topMargin + timeMarkersHeight;
//     for (int i = 0; i < displayItems.size(); i++)
//     {
//         const auto &item = displayItems[i];
//         int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

//         if (selectedItems.contains(i))
//         {
//             painter.fillRect(0, currentY, width() - signalNamesWidth - valuesColumnWidth, itemHeight, QColor(60, 60, 90));
//         }
//         currentY += itemHeight;
//     }
// }

void WaveformWidget::drawSignals(QPainter &painter)
{
    int currentY = topMargin + timeMarkersHeight;

    // Only process visible signals for waveform rendering
    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

        // Skip drawing if item is outside visible area (considering vertical offset)
        int visibleY = currentY - verticalOffset;
        if (visibleY + itemHeight < 0)
        {
            currentY += itemHeight;
            continue;
        }
        if (visibleY > height())
        {
            break;
        }

        // Only draw waveform if this signal is in our visible set
        if (visibleSignalIndices.contains(i) && item.type == DisplayItem::Signal)
        {
            const VCDSignal &signal = item.signal.signal;
            if (signal.width > 1)
            {
                drawBusWaveform(painter, signal, currentY);
            }
            else
            {
                drawSignalWaveform(painter, signal, currentY);
            }
        }

        currentY += itemHeight;
    }
}

void WaveformWidget::drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos)
{
    // Use lazy loading to get value changes
    const auto changes = vcdParser->getValueChangesForSignal(signal.identifier);
    if (changes.isEmpty())
        return;

    // Emergency check for extreme zoom
    if (timeScale > 1000.0 || timeScale < 0.001)
    {
        qDebug() << "Emergency: Skipping waveform drawing due to extreme zoom:" << timeScale;
        return;
    }

    QColor defaultColor = QColor(0xFF, 0xE6, 0xCD); // Default color: #ffe6cd

    // Hardcoded small offset - 3 pixels from top and bottom
    int signalTop = yPos + 3;
    int signalBottom = yPos + signalHeight - 3;
    int signalMidY = yPos + signalHeight / 2;
    int highLevel = signalTop;    // Top of the waveform area
    int lowLevel = signalBottom;  // Bottom of the waveform area
    int middleLevel = signalMidY; // Middle for X/Z values

    int prevTime = 0;
    QString prevValue = "0";
    int prevX = timeToX(prevTime);

    for (const auto &change : changes)
    {
        int currentX = timeToX(change.timestamp);

        // Determine color based on value
        QColor drawColor = defaultColor; // Default to #ffe6cd
        bool isX = (change.value == "x" || change.value == "X");
        bool isZ = (change.value == "z" || change.value == "Z");
        bool isZero = (change.value == "0");
        bool prevIsX = (prevValue == "x" || prevValue == "X");
        bool prevIsZ = (prevValue == "z" || prevValue == "Z");
        bool prevIsZero = (prevValue == "0");

        if (isX)
        {
            drawColor = QColor(255, 0, 0); // Red for X
        }
        else if (isZ)
        {
            drawColor = QColor(255, 165, 0); // Orange for Z
        }
        else if (isZero)
        {
            drawColor = QColor(0x01, 0xFF, 0xFF); // Cyan #01ffff for zero
        }
        // For '1' values, it will use the default #ffe6cd

        painter.setPen(QPen(drawColor, lineWidth));

        // Draw the segment based on previous value
        if (prevIsX || prevIsZ)
        {
            // Previous value was X or Z - draw at middle level
            painter.drawLine(prevX, middleLevel, currentX, middleLevel);
        }
        else if (prevValue == "1")
        {
            painter.drawLine(prevX, highLevel, currentX, highLevel);
        }
        else
        {
            // Previous value was zero - draw at low level
            painter.drawLine(prevX, lowLevel, currentX, lowLevel);
        }

        // Draw transition line if value changed
        if (prevValue != change.value)
        {
            int fromY, toY;

            // Determine starting Y position
            if (prevIsX || prevIsZ)
            {
                fromY = middleLevel;
            }
            else if (prevValue == "1")
            {
                fromY = highLevel;
            }
            else
            {
                fromY = lowLevel;
            }

            // Determine ending Y position
            if (isX || isZ)
            {
                toY = middleLevel;
            }
            else if (change.value == "1")
            {
                toY = highLevel;
            }
            else
            {
                toY = lowLevel;
            }

            painter.drawLine(currentX, fromY, currentX, toY);
        }

        prevTime = change.timestamp;
        prevValue = change.value;
        prevX = currentX;
    }

    // Draw the final segment
    QColor finalColor = defaultColor;
    bool finalIsX = (prevValue == "x" || prevValue == "X");
    bool finalIsZ = (prevValue == "z" || prevValue == "Z");
    bool finalIsZero = (prevValue == "0");

    if (finalIsX)
    {
        finalColor = QColor(255, 0, 0);
    }
    else if (finalIsZ)
    {
        finalColor = QColor(255, 165, 0);
    }
    else if (finalIsZero)
    {
        finalColor = QColor(0x01, 0xFF, 0xFF); // Cyan #01ffff for zero
    }

    painter.setPen(QPen(finalColor, lineWidth));

    int endX = timeToX(vcdParser->getEndTime());

    if (finalIsX || finalIsZ)
    {
        painter.drawLine(prevX, middleLevel, endX, middleLevel);
    }
    else if (prevValue == "1")
    {
        painter.drawLine(prevX, highLevel, endX, highLevel);
    }
    else
    {
        painter.drawLine(prevX, lowLevel, endX, lowLevel);
    }
}

void WaveformWidget::drawCleanTransition(QPainter &painter, int x, int top, int bottom, const QColor &signalColor)
{
    int height = bottom - top;

    // Draw a prominent vertical line
    painter.setPen(QPen(signalColor.lighter(150), 2));
    painter.drawLine(x, top, x, bottom);

    // Add small cross markers for visibility
    int crossSize = 2;

    // Top cross
    painter.drawLine(x - crossSize, top + crossSize, x + crossSize, top + crossSize);
    painter.drawLine(x, top, x, top + crossSize * 2);

    // Bottom cross
    painter.drawLine(x - crossSize, bottom - crossSize, x + crossSize, bottom - crossSize);
    painter.drawLine(x, bottom - crossSize * 2, x, bottom);

    // Center dot for extra visibility
    int centerY = top + height / 2;
    painter.fillRect(x - 1, centerY - 1, 3, 3, signalColor);
}

void WaveformWidget::drawBusWaveform(QPainter &painter, const VCDSignal &signal, int yPos)
{
    // Use lazy loading to get value changes
    const auto changes = vcdParser->getValueChangesForSignal(signal.identifier);
    if (changes.isEmpty())
        return;

    // Emergency check for extreme zoom
    if (timeScale > 1000.0 || timeScale < 0.001)
    {
        qDebug() << "Emergency: Skipping bus drawing due to extreme zoom:" << timeScale;
        return;
    }

    QColor signalColor = getSignalColor(signal.identifier);

    // Clean styling parameters
    int busTop = yPos + 2;
    int busBottom = yPos + busHeight - 2;
    int busMidY = yPos + busHeight / 2;
    int textY = busMidY + 4;
    int waveformHeight = busBottom - busTop;

    int prevTime = 0;
    QString prevValue = getBusValueAtTime(signal.identifier, 0);
    int prevX = timeToX(prevTime);

    // Draw clean bus background
    painter.fillRect(prevX, busTop, width() - signalNamesWidth - valuesColumnWidth, waveformHeight, QColor(45, 45, 50));

    // Draw value regions with clear transitions
    for (int i = 0; i < changes.size(); i++)
    {
        const auto &change = changes[i];
        int currentX = timeToX(change.timestamp);

        // Clean region coloring
        QColor regionColor = QColor(60, 60, 70);

        if (prevValue.contains('x') || prevValue.contains('X'))
        {
            regionColor = QColor(120, 60, 60); // Dark red for X
        }
        else if (prevValue.contains('z') || prevValue.contains('Z'))
        {
            regionColor = QColor(120, 80, 40); // Dark orange for Z
        }
        else if (!prevValue.isEmpty() && prevValue != "0")
        {
            // Active value - slightly brighter
            regionColor = QColor(70, 70, 90);
        }

        // Draw the value region
        painter.fillRect(prevX, busTop, currentX - prevX, waveformHeight, regionColor);

        // Draw the value text with clean styling
        if (currentX - prevX > 50) // Only draw text if region is wide enough
        {
            QString displayValue = formatBusValue(prevValue);
            int textWidth = painter.fontMetrics().horizontalAdvance(displayValue);
            int centerX = prevX + (currentX - prevX) / 2;

            // Simple text with good contrast
            painter.setPen(QPen(Qt::white));
            painter.drawText(centerX - textWidth / 2, textY, displayValue);
        }

        // Draw clear transition line
        if (i > 0) // Don't draw transition for first value
        {
            drawCleanTransition(painter, currentX, busTop, busBottom, signalColor);
        }

        prevTime = change.timestamp;
        prevValue = change.value;
        prevX = currentX;
    }

    // Draw the final region
    int endX = timeToX(vcdParser->getEndTime());
    if (endX > prevX)
    {
        QColor finalRegionColor = QColor(60, 60, 70);
        if (prevValue.contains('x') || prevValue.contains('X'))
        {
            finalRegionColor = QColor(120, 60, 60);
        }
        else if (prevValue.contains('z') || prevValue.contains('Z'))
        {
            finalRegionColor = QColor(120, 80, 40);
        }

        painter.fillRect(prevX, busTop, endX - prevX, waveformHeight, finalRegionColor);

        if (endX - prevX > 50)
        {
            QString displayValue = formatBusValue(prevValue);
            int textWidth = painter.fontMetrics().horizontalAdvance(displayValue);
            int centerX = prevX + (endX - prevX) / 2;

            painter.setPen(QPen(Qt::white));
            painter.drawText(centerX - textWidth / 2, textY, displayValue);
        }
    }

    // Draw clean bus outline
    painter.setPen(QPen(signalColor, 2));
    painter.drawRect(timeToX(0), busTop, endX - timeToX(0), waveformHeight);
}

void WaveformWidget::updateScrollBar()
{
    if (!vcdParser)
    {
        horizontalScrollBar->setRange(0, 0);
        verticalScrollBar->setRange(0, 0);
        return;
    }

    // Calculate viewport dimensions safely
    int viewportWidth = width() - signalNamesWidth - valuesColumnWidth;
    if (viewportWidth < 10)
        viewportWidth = 10;

    int viewportHeight = height();
    if (viewportHeight < 10)
        viewportHeight = 10;

    // Define scroll margins in PIXELS
    const int LEFT_MARGIN_PIXELS = static_cast<int>(-10 * timeScale);  // -10 time units
    const int RIGHT_MARGIN_PIXELS = static_cast<int>(100 * timeScale); // 100 time units after end

    // Calculate the total pixel width of the timeline including margins
    int timelinePixelWidth = static_cast<int>(vcdParser->getEndTime() * timeScale);
    int totalPixelWidth = timelinePixelWidth + LEFT_MARGIN_PIXELS + RIGHT_MARGIN_PIXELS;

    // The maximum scroll offset is the difference between total width and viewport width
    int maxScrollOffset = qMax(0, totalPixelWidth - viewportWidth);

    // Set the horizontal scrollbar range
    horizontalScrollBar->setRange(0, maxScrollOffset);
    horizontalScrollBar->setPageStep(viewportWidth);
    horizontalScrollBar->setSingleStep(viewportWidth / 10);

    // Calculate vertical scrolling
    int totalHeight = calculateTotalHeight();
    int visibleHeight = viewportHeight - timeMarkersHeight; // Subtract pinned timeline area

    // Only enable vertical scrollbar if content is taller than visible area
    if (totalHeight > visibleHeight)
    {
        int maxVerticalOffset = totalHeight - visibleHeight;
        verticalScrollBar->setRange(0, maxVerticalOffset);
        verticalScrollBar->setPageStep(visibleHeight);
        verticalScrollBar->setSingleStep(30); // Scroll by approximately one signal height
        verticalScrollBar->setVisible(true);
    }
    else
    {
        verticalScrollBar->setRange(0, 0);
        verticalScrollBar->setVisible(false);
        verticalOffset = 0; // Reset offset if no scrolling needed
    }

    qDebug() << "Scrollbar update - Total height:" << totalHeight
             << "Visible height:" << visibleHeight
             << "Vertical range:" << verticalScrollBar->minimum() << "-" << verticalScrollBar->maximum();
}

int WaveformWidget::calculateTotalHeight() const
{
    if (displayItems.isEmpty())
        return timeMarkersHeight; // Just the timeline area

    int totalHeight = topMargin + timeMarkersHeight;
    for (const auto &item : displayItems)
    {
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height
        totalHeight += itemHeight;
    }

    // Add some extra padding at the bottom
    totalHeight += 10;

    return totalHeight;
}

int WaveformWidget::timeToX(int time) const
{
    if (!vcdParser)
        return 0;

    // Convert time to pixels, then subtract the scroll offset
    double pixelPosition = time * timeScale;
    double result = pixelPosition - timeOffset;

    // Clamp to safe integer range
    if (result > 1000000)
        return 1000000;
    if (result < -1000000)
        return -1000000;

    return static_cast<int>(result);
}

int WaveformWidget::xToTime(int x) const
{
    if (!vcdParser)
        return 0;

    // Handle invalid scale
    if (timeScale < 0.0001)
        return 0;

    // Convert pixel position (including scroll offset) back to time
    double result = (x + timeOffset) / timeScale;

    // Clamp to safe range
    if (result > 1000000000)
        return 1000000000;
    if (result < -1000000000)
        return -1000000000;

    return static_cast<int>(result);
}

QString WaveformWidget::getSignalValueAtTime(const QString &identifier, int time) const
{
    // Use lazy loading
    const auto changes = vcdParser->getValueChangesForSignal(identifier);
    QString value = "0";

    for (const auto &change : changes)
    {
        if (change.timestamp > time)
            break;
        value = change.value;
    }

    return value;
}

QString WaveformWidget::getBusValueAtTime(const QString &identifier, int time) const
{
    // Use lazy loading
    const auto changes = vcdParser->getValueChangesForSignal(identifier);
    QString value = "0";

    for (const auto &change : changes)
    {
        if (change.timestamp > time)
            break;
        value = change.value;
    }

    return value;
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

void WaveformWidget::handleMultiSelection(int itemIndex, QMouseEvent *event)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size())
        return;

    if (event->modifiers() & Qt::ShiftModifier && lastSelectedItem != -1)
    {
        // Shift-click: select range from last selected to current
        selectedItems.clear();
        int start = qMin(lastSelectedItem, itemIndex);
        int end = qMax(lastSelectedItem, itemIndex);
        for (int i = start; i <= end; i++)
        {
            selectedItems.insert(i);
        }
    }
    else if (event->modifiers() & Qt::ControlModifier)
    {
        // Ctrl-click: toggle selection
        if (selectedItems.contains(itemIndex))
        {
            selectedItems.remove(itemIndex);
        }
        else
        {
            selectedItems.insert(itemIndex);
        }
        lastSelectedItem = itemIndex;
    }
    else
    {
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
    if (index < 0 || index >= displayItems.size())
        return -1;

    int yPos = topMargin + timeMarkersHeight;
    for (int i = 0; i < index; i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height
        yPos += itemHeight;
    }
    return yPos;
}

void WaveformWidget::startDrag(int itemIndex)
{
    if (itemIndex < 0 || itemIndex >= displayItems.size())
        return;

    isDraggingItem = true;
    dragItemIndex = itemIndex;
    dragStartPos = QCursor::pos();
    dragStartY = getItemYPosition(itemIndex) - verticalOffset; // Account for vertical offset
    setCursor(Qt::ClosedHandCursor);
}

void WaveformWidget::performDrag(int mouseY)
{
    if (!isDraggingItem || dragItemIndex < 0)
        return;

    // Adjust mouseY by vertical offset to get the actual position in the content
    int adjustedMouseY = mouseY + verticalOffset;

    int newIndex = -1;
    int currentY = topMargin + timeMarkersHeight;

    // Find new position based on adjusted mouse Y
    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

        // Check if mouse is within the first half of the item height (insert above)
        if (adjustedMouseY >= currentY && adjustedMouseY < currentY + itemHeight / 2)
        {
            newIndex = i;
            break;
        }
        // Check if mouse is within the second half of the item height (insert below)
        else if (adjustedMouseY >= currentY + itemHeight / 2 && adjustedMouseY < currentY + itemHeight)
        {
            newIndex = i + 1;
            break;
        }
        currentY += itemHeight;
    }

    // If we reached the end without finding a position, put it at the end
    if (newIndex == -1)
    {
        newIndex = displayItems.size();
    }

    // Clamp the new index to valid range
    newIndex = qMax(0, qMin(newIndex, displayItems.size()));

    // Don't move if it's the same position
    if (newIndex == dragItemIndex || newIndex == dragItemIndex + 1)
        return;

    moveItem(dragItemIndex, newIndex);
}

void WaveformWidget::moveItem(int itemIndex, int newIndex)
{
    // If moving to a position after the current item, adjust for the removal
    if (newIndex > itemIndex)
    {
        newIndex--;
    }

    DisplayItem item = displayItems[itemIndex];
    displayItems.removeAt(itemIndex);
    displayItems.insert(newIndex, item);

    // Update drag item index to the new position
    dragItemIndex = newIndex;

    // Update selection
    if (selectedItems.contains(itemIndex))
    {
        selectedItems.remove(itemIndex);
        selectedItems.insert(newIndex);
        lastSelectedItem = newIndex;
    }

    update();
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
    // Check if click is in search bar area (pinned)

    if (event->button() == Qt::LeftButton)
    {
        if (isOverNamesSplitter(event->pos()))
        {
            draggingNamesSplitter = true;
            setCursor(Qt::SplitHCursor);
            event->accept();
            return;
        }
        else if (isOverValuesSplitter(event->pos()))
        {
            draggingValuesSplitter = true;
            setCursor(Qt::SplitHCursor);
            event->accept();
            return;
        }
    }

    // Check if click is in timeline area (pinned top part of waveform area)
    int waveformStartX = signalNamesWidth + valuesColumnWidth;
    bool inTimelineArea = event->pos().x() >= waveformStartX &&
                          event->pos().y() < timeMarkersHeight;

    if (event->button() == Qt::LeftButton && inTimelineArea)
    {
        updateCursorTime(event->pos());
        event->accept();
        return;
    }

    // Check if click is in signal names column or waveform area
    bool inNamesColumn = event->pos().x() < signalNamesWidth;
    bool inWaveformArea = event->pos().x() >= waveformStartX;

    if (event->button() == Qt::MiddleButton)
    {
        // Start middle button drag for horizontal scrolling (waveform area only, excluding pinned timeline)
        if (!inNamesColumn && inWaveformArea && event->pos().y() >= timeMarkersHeight)
        {
            isDragging = true;
            dragStartX = event->pos().x() - waveformStartX;
            dragStartOffset = timeOffset;
            setCursor(Qt::ClosedHandCursor);
        }
    }
    else if (event->button() == Qt::LeftButton)
    {
        // Also allow setting cursor time when clicking in the main waveform area (excluding pinned timeline)
        if (inWaveformArea && event->pos().y() >= timeMarkersHeight)
        {
            updateCursorTime(event->pos());
            event->accept();
            return;
        }
        else if (!inNamesColumn && inWaveformArea)
        {
            // Start timeline dragging with left button
            isDragging = true;
            dragStartX = event->pos().x() - waveformStartX;
            dragStartOffset = timeOffset; // Current scroll position
            setCursor(Qt::ClosedHandCursor);
        }

        // Item selection/dragging only works in the scrollable area (below pinned headers)
        if (event->pos().y() >= topMargin + timeMarkersHeight)
        {
            int itemIndex = getItemAtPosition(event->pos());

            if (itemIndex >= 0)
            {
                // Handle multi-selection
                handleMultiSelection(itemIndex, event);

                // Prepare for drag - update visible signals first
                updateVisibleSignals();
                startDrag(itemIndex);
                update();
                emit itemSelected(itemIndex);
            }
            else if (!inNamesColumn && inWaveformArea)
            {
                // Clear selection when clicking empty space
                if (!(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)))
                {
                    selectedItems.clear();
                    lastSelectedItem = -1;
                    update();
                    emit itemSelected(-1);
                }

                // Start timeline dragging with left button (waveform area only, excluding pinned timeline)
                isDragging = true;
                dragStartX = event->pos().x() - waveformStartX;
                dragStartOffset = timeOffset;
                setCursor(Qt::ClosedHandCursor);
            }
        }
    }
}

void WaveformWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    if (itemIndex >= 0)
    {
        if (isSpaceItem(itemIndex))
        {
            renameItem(itemIndex);
            event->accept();
            return;
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (draggingNamesSplitter)
    {
        signalNamesWidth = qMax(150, event->pos().x());
        updateSplitterPositions();
    }
    else if (draggingValuesSplitter)
    {
        valuesColumnWidth = qMax(80, event->pos().x() - signalNamesWidth);
        updateSplitterPositions();
    }
    else
    {
        // Update cursor when over splitter
        if (isOverNamesSplitter(event->pos()) || isOverValuesSplitter(event->pos()))
        {
            setCursor(Qt::SplitHCursor);
        }
        else
        {
            setCursor(Qt::ArrowCursor);
        }

        if (isDraggingItem)
        {
            performDrag(event->pos().y());
            updateVisibleSignals();
            update();
        }
        else if (isDragging)
        {
            int waveformStartX = signalNamesWidth + valuesColumnWidth;
            int delta = dragStartX - (event->pos().x() - waveformStartX);

            // Calculate new offset and clamp it to scrollbar range
            int newOffset = dragStartOffset + delta;
            int maxOffset = horizontalScrollBar->maximum();
            newOffset = qMax(0, qMin(newOffset, maxOffset));

            timeOffset = newOffset;

            // Update scrollbar position to match
            horizontalScrollBar->setValue(timeOffset);

            update();
        }

        // Emit time change for cursor position in waveform area
        int waveformStartX = signalNamesWidth + valuesColumnWidth;
        if (event->pos().x() >= waveformStartX)
        {
            int currentTime = xToTime(event->pos().x() - waveformStartX);
            emit timeChanged(currentTime);
        }
    }
}

void WaveformWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && (draggingNamesSplitter || draggingValuesSplitter))
    {
        draggingNamesSplitter = false;
        draggingValuesSplitter = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

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
    else if (event->key() == Qt::Key_Escape && isSearchActive)
    {
        // Clear search on Escape and lose focus
        handleSearchInput("");
        isSearchFocused = false; // Lose focus
        event->accept();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        // Keep search but lose focus on Enter
        if (isSearchActive)
        {
            isSearchFocused = false;
            update();
            event->accept();
        }
        else
        {
            QWidget::keyPressEvent(event);
        }
    }
    else if (event->key() == Qt::Key_Backspace)
    {
        // Handle backspace in search
        if (isSearchActive)
        {
            handleSearchInput(searchText.left(searchText.length() - 1));
            event->accept();
        }
        else
        {
            QWidget::keyPressEvent(event);
        }
    }
    // Add signal height adjustment shortcuts - UPDATED
    else if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_Up)
        {
            // Ctrl+Up to increase signal heights
            setSignalHeight(signalHeight + 2);
            setBusHeight(busHeight + 2);
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_Down)
        {
            // Ctrl+Down to decrease signal heights
            setSignalHeight(signalHeight - 2);
            setBusHeight(busHeight - 2);
            event->accept();
            return;
        }
    }
    else if (!event->text().isEmpty() && event->text().at(0).isPrint())
    {
        // Handle regular text input for search
        if (!isSearchActive)
        {
            // Start new search
            handleSearchInput(event->text());
        }
        else
        {
            // Append to existing search
            handleSearchInput(searchText + event->text());
        }
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
        // Regular wheel for vertical scrolling
        int scrollAmount = event->angleDelta().y();
        verticalOffset -= scrollAmount / 2;

        // Clamp vertical offset to valid range
        int maxVerticalOffset = verticalScrollBar->maximum();
        verticalOffset = qMax(0, qMin(verticalOffset, maxVerticalOffset));

        // Update scrollbar position
        verticalScrollBar->setValue(verticalOffset);

        updateVisibleSignals();
        update();

        qDebug() << "Vertical scroll - Offset:" << verticalOffset << "Max:" << maxVerticalOffset;
    }
}

void WaveformWidget::setVisibleSignals(const QList<VCDSignal> &visibleSignals)
{
    // If we're at an extreme zoom level, reset to reasonable zoom first
    if (timeScale > 100.0 || timeScale < 0.01)
    {
        qDebug() << "Resetting extreme zoom level before adding signals:" << timeScale;
        timeScale = 1.0;
        timeOffset = 0;
    }

    displayItems.clear();

    // Load data for the selected signals
    if (vcdParser && !visibleSignals.isEmpty())
    {
        QList<QString> identifiers;
        for (const auto &signal : visibleSignals)
        {
            identifiers.append(signal.identifier);
        }

        // Load signal data before displaying
        vcdParser->loadSignalsData(identifiers);
    }

    for (const auto &signal : visibleSignals)
    {
        displayItems.append(DisplayItem::createSignal(signal));
    }
    selectedItems.clear();
    lastSelectedItem = -1;

    // Auto-zoom to fit after adding signals
    if (!visibleSignals.isEmpty())
    {
        zoomFit();
    }

    updateVisibleSignals();
    updateScrollBar(); // Make sure scrollbar is updated
    update();
    emit itemSelected(-1);
}

void WaveformWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    showContextMenu(event->globalPos(), itemIndex);
}

void WaveformWidget::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    // Ensure minimum dimensions
    if (width() < 100 || height() < 100)
    {
        qDebug() << "Warning: Very small widget size" << width() << "x" << height();
    }

    updateScrollBar();

    // Position the scrollbars safely
    int scrollbarSize = 20;

    // Horizontal scrollbar - spans the entire bottom
    horizontalScrollBar->setGeometry(
        signalNamesWidth + valuesColumnWidth,
        qMax(0, height() - scrollbarSize),
        qMax(0, width() - signalNamesWidth - valuesColumnWidth),
        scrollbarSize);

    // Vertical scrollbar - spans the right side, above horizontal scrollbar
    verticalScrollBar->setGeometry(
        qMax(0, width() - scrollbarSize),
        0,
        scrollbarSize,
        qMax(0, height() - scrollbarSize)); // Leave space for horizontal scrollbar

    qDebug() << "Resize event - Widget:" << width() << "x" << height()
             << "Horizontal scrollbar:" << horizontalScrollBar->geometry()
             << "Vertical scrollbar:" << verticalScrollBar->geometry();
}

int WaveformWidget::getItemAtPosition(const QPoint &pos) const
{
    if (displayItems.isEmpty())
        return -1;

    // Only detect items in the scrollable area (below pinned headers)
    if (pos.y() < topMargin + timeMarkersHeight)
        return -1;

    int y = pos.y() + verticalOffset; // Add vertical offset to get actual position
    int signalAreaTop = topMargin + timeMarkersHeight;

    if (y < signalAreaTop)
        return -1;

    int currentY = signalAreaTop;
    for (int i = 0; i < displayItems.size(); i++)
    {
        // Use the same height calculation as in drawing functions
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? (item.signal.signal.width > 1 ? busHeight : signalHeight) : 30; // Space height

        // Check if click is within the full item height (not just the drawn area)
        if (y >= currentY && y < currentY + itemHeight)
            return i;
        currentY += itemHeight;
    }

    return -1;
}

QString WaveformWidget::promptForName(const QString &title, const QString &defaultName)
{
    bool ok;
    QString name = QInputDialog::getText(this, title, "Name:", QLineEdit::Normal, defaultName, &ok);
    if (ok)
    {
        return name;
    }
    return defaultName;
}

void WaveformWidget::addSpaceAbove(int index)
{
    if (index < 0 || index >= displayItems.size())
        return;

    QString name = promptForName("Add Space", "");
    displayItems.insert(index, DisplayItem::createSpace(name));
    update();
}

void WaveformWidget::addSpaceBelow(int index)
{
    if (index < 0 || index >= displayItems.size())
        return;

    QString name = promptForName("Add Space", "");
    int insertIndex = index + 1;
    if (insertIndex > displayItems.size())
    {
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

QColor WaveformWidget::getSignalColor(const QString &identifier) const
{
    // If user has set a custom color, use it
    if (signalColors.contains(identifier))
    {
        return signalColors[identifier];
    }

    // Default to #ffe6cd for all signals
    return QColor(0xFF, 0xE6, 0xCD);
}

void WaveformWidget::changeSignalColor(int itemIndex)
{
    if (selectedItems.isEmpty())
        return;

    // Get the first selected signal to use as current color reference
    QColor currentColor = Qt::green;
    for (int index : selectedItems)
    {
        if (isSignalItem(index))
        {
            const VCDSignal &signal = displayItems[index].signal.signal;
            currentColor = getSignalColor(signal.identifier);
            break;
        }
    }

    QMenu colorMenu(this);

    // Predefined colors
    QList<QPair<QString, QColor>> predefinedColors = {
        {"Red", QColor(255, 0, 0)},
        {"Green", QColor(0, 255, 0)},
        {"Blue", QColor(0, 0, 255)},
        {"Yellow", QColor(255, 255, 0)},
        {"Cyan", QColor(0, 255, 255)},
        {"Magenta", QColor(255, 0, 255)},
        {"Orange", QColor(255, 165, 0)},
        {"Purple", QColor(128, 0, 128)},
        {"Pink", QColor(255, 192, 203)},
        {"White", QColor(255, 255, 255)}};

    for (const auto &colorPair : predefinedColors)
    {
        QAction *colorAction = colorMenu.addAction(colorPair.first);
        colorAction->setData(colorPair.second);

        // Create color icon
        QPixmap pixmap(16, 16);
        pixmap.fill(colorPair.second);
        colorAction->setIcon(QIcon(pixmap));
    }

    colorMenu.addSeparator();
    colorMenu.addAction("Custom Color...");

    // Update menu title to show how many signals are selected
    if (selectedItems.size() > 1)
    {
        colorMenu.setTitle(QString("Change Color for %1 Signals").arg(selectedItems.size()));
    }

    QAction *selectedAction = colorMenu.exec(QCursor::pos());

    if (selectedAction)
    {
        QColor newColor;

        if (selectedAction->text() == "Custom Color...")
        {
            newColor = QColorDialog::getColor(currentColor, this,
                                              QString("Choose color for %1 signals").arg(selectedItems.size()));
            if (!newColor.isValid())
            {
                return; // User cancelled
            }
        }
        else
        {
            newColor = selectedAction->data().value<QColor>();
        }

        // Apply the color to all selected signals
        for (int index : selectedItems)
        {
            if (isSignalItem(index))
            {
                const VCDSignal &signal = displayItems[index].signal.signal;
                signalColors[signal.identifier] = newColor;
            }
        }
        update();
    }
}

bool WaveformWidget::isOverNamesSplitter(const QPoint &pos) const
{
    return (pos.x() >= signalNamesWidth - 3 && pos.x() <= signalNamesWidth + 3);
}

bool WaveformWidget::isOverValuesSplitter(const QPoint &pos) const
{
    int valuesColumnStart = signalNamesWidth;
    int valuesColumnEnd = valuesColumnStart + valuesColumnWidth;
    return (pos.x() >= valuesColumnEnd - 3 && pos.x() <= valuesColumnEnd + 3);
}

void WaveformWidget::updateSplitterPositions()
{
    // Ensure minimum widths
    signalNamesWidth = qMax(150, signalNamesWidth);
    valuesColumnWidth = qMax(80, valuesColumnWidth);

    // Ensure maximum widths
    if (signalNamesWidth + valuesColumnWidth > width() - 300)
    {
        valuesColumnWidth = width() - 300 - signalNamesWidth;
    }

    update();
}

void WaveformWidget::updateCursorTime(const QPoint &pos)
{
    int waveformStartX = signalNamesWidth + valuesColumnWidth;

    // Only set cursor if click is in the timeline area (top part)
    if (pos.x() < waveformStartX || pos.y() >= timeMarkersHeight)
    {
        return;
    }

    // Calculate cursor time based on the visible waveform area, accounting for horizontal scrolling
    int clickXInWaveform = pos.x() - waveformStartX;

    // Convert the click position to time, accounting for current zoom and scroll
    cursorTime = xToTime(clickXInWaveform);

    showCursor = true;
    update();
}

void WaveformWidget::showContextMenu(const QPoint &pos, int itemIndex)
{
    QMenu contextMenu(this);

    if (itemIndex >= 0)
    {
        // Ensure the clicked item is selected if no multi-selection
        if (!selectedItems.contains(itemIndex) && selectedItems.size() <= 1)
        {
            selectedItems.clear();
            selectedItems.insert(itemIndex);
            lastSelectedItem = itemIndex;
            update();
        }

        // Remove option - show count if multiple selected
        QString removeText = "Remove";
        if (selectedItems.size() > 1)
        {
            removeText = QString("Remove %1 Signals").arg(selectedItems.size());
        }
        else if (isSignalItem(itemIndex))
        {
            removeText = "Remove Signal";
        }
        else if (isSpaceItem(itemIndex))
        {
            removeText = "Remove Space";
        }

        contextMenu.addAction(removeText, this, &WaveformWidget::removeSelectedSignals);
        contextMenu.addSeparator();

        // Color change for signals - show count if multiple selected
        bool hasSignals = false;
        for (int index : selectedItems)
        {
            if (isSignalItem(index))
            {
                hasSignals = true;
                break;
            }
        }

        if (hasSignals)
        {
            QString colorText = "Change Color";
            if (selectedItems.size() > 1)
            {
                colorText = QString("Change Color for %1 Signals").arg(selectedItems.size());
            }
            contextMenu.addAction(colorText, this, [this, itemIndex]()
                                  { changeSignalColor(itemIndex); });
            contextMenu.addSeparator();
        }

        // Rename for spaces (only if single space selected)
        if (isSpaceItem(itemIndex) && selectedItems.size() == 1)
        {
            contextMenu.addAction("Rename", this, [this, itemIndex]()
                                  { renameItem(itemIndex); });
            contextMenu.addSeparator();
        }

        // Bus display options (only show if any multi-bit signals are selected)
        bool hasMultiBitSignals = false;
        for (int index : selectedItems)
        {
            if (isSignalItem(index) && getSignalFromItem(index).width > 1)
            {
                hasMultiBitSignals = true;
                break;
            }
        }

        if (hasMultiBitSignals)
        {
            QMenu *busFormatMenu = contextMenu.addMenu("Bus Display Format");

            QAction *hexAction = busFormatMenu->addAction("Hexadecimal", [this]()
                                                          { setBusDisplayFormat(WaveformWidget::Hex); });
            QAction *binAction = busFormatMenu->addAction("Binary", [this]()
                                                          { setBusDisplayFormat(WaveformWidget::Binary); });
            QAction *octAction = busFormatMenu->addAction("Octal", [this]()
                                                          { setBusDisplayFormat(WaveformWidget::Octal); });
            QAction *decAction = busFormatMenu->addAction("Decimal", [this]()
                                                          { setBusDisplayFormat(WaveformWidget::Decimal); });

            hexAction->setCheckable(true);
            binAction->setCheckable(true);
            octAction->setCheckable(true);
            decAction->setCheckable(true);

            hexAction->setChecked(busDisplayFormat == Hex);
            binAction->setChecked(busDisplayFormat == Binary);
            octAction->setChecked(busDisplayFormat == Octal);
            decAction->setChecked(busDisplayFormat == Decimal);

            contextMenu.addSeparator();
        }

        // Space management
        contextMenu.addAction("Add Space Above", this, [this, itemIndex]()
                              { addSpaceAbove(itemIndex); });
        contextMenu.addAction("Add Space Below", this, [this, itemIndex]()
                              { addSpaceBelow(itemIndex); });
    }
    else
    {
        // Global bus display options when clicking empty space
        QMenu *busFormatMenu = contextMenu.addMenu("Bus Display Format");

        QAction *hexAction = busFormatMenu->addAction("Hexadecimal", [this]()
                                                      { setBusDisplayFormat(WaveformWidget::Hex); });
        QAction *binAction = busFormatMenu->addAction("Binary", [this]()
                                                      { setBusDisplayFormat(WaveformWidget::Binary); });
        QAction *octAction = busFormatMenu->addAction("Octal", [this]()
                                                      { setBusDisplayFormat(WaveformWidget::Octal); });
        QAction *decAction = busFormatMenu->addAction("Decimal", [this]()
                                                      { setBusDisplayFormat(WaveformWidget::Decimal); });

        hexAction->setCheckable(true);
        binAction->setCheckable(true);
        octAction->setCheckable(true);
        decAction->setCheckable(true);

        hexAction->setChecked(busDisplayFormat == Hex);
        binAction->setChecked(busDisplayFormat == Binary);
        octAction->setChecked(busDisplayFormat == Octal);
        decAction->setChecked(busDisplayFormat == Decimal);
    }

    QAction *selectedAction = contextMenu.exec(pos);
    if (!selectedAction && itemIndex >= 0 && selectedItems.size() <= 1)
    {
        // Restore selection if menu was cancelled and only single item was selected
        selectedItems.clear();
        selectedItems.insert(itemIndex);
        update();
    }

    emit contextMenuRequested(pos, itemIndex);
}

QString WaveformWidget::formatBusValue(const QString &binaryValue) const
{
    if (binaryValue.isEmpty())
        return "x";

    // Handle special cases
    if (binaryValue == "x" || binaryValue == "X")
        return "x";
    if (binaryValue == "z" || binaryValue == "Z")
        return "z";

    // Check if it's a valid binary string
    if (!isValidBinary(binaryValue))
    {
        return binaryValue; // Return as-is if not pure binary
    }

    switch (busDisplayFormat)
    {
    case Hex:
        return binaryToHex(binaryValue);
    case Binary:
        return binaryValue;
    case Octal:
        return binaryToOctal(binaryValue);
    case Decimal:
        return binaryToDecimal(binaryValue);
    default:
        return binaryToHex(binaryValue);
    }
}

bool WaveformWidget::isValidBinary(const QString &value) const
{
    for (QChar ch : value)
    {
        if (ch != '0' && ch != '1')
        {
            return false;
        }
    }
    return true;
}

QString WaveformWidget::binaryToHex(const QString &binaryValue) const
{
    if (binaryValue.isEmpty())
        return "0";

    // Convert binary string to integer
    bool ok;
    unsigned long long value = binaryValue.toULongLong(&ok, 2);

    if (!ok)
    {
        return "x"; // Conversion failed
    }

    // Calculate number of hex digits needed
    int bitCount = binaryValue.length();
    int hexDigits = (bitCount + 3) / 4; // ceil(bitCount / 4)

    // Format as hex with appropriate number of digits
    return "0x" + QString::number(value, 16).rightJustified(hexDigits, '0').toUpper();
}

QString WaveformWidget::binaryToOctal(const QString &binaryValue) const
{
    if (binaryValue.isEmpty())
        return "0";

    // Convert binary to octal
    QString octal;
    QString paddedBinary = binaryValue;

    // Pad with zeros to make length multiple of 3
    while (paddedBinary.length() % 3 != 0)
    {
        paddedBinary = "0" + paddedBinary;
    }

    for (int i = 0; i < paddedBinary.length(); i += 3)
    {
        QString chunk = paddedBinary.mid(i, 3);
        int decimal = chunk.toInt(nullptr, 2);
        octal += QString::number(decimal);
    }

    return "0" + octal;
}

QString WaveformWidget::binaryToDecimal(const QString &binaryValue) const
{
    if (binaryValue.isEmpty())
        return "0";

    bool ok;
    unsigned long long value = binaryValue.toULongLong(&ok, 2);

    if (!ok)
    {
        return "x"; // Conversion failed
    }

    return QString::number(value);
}

void WaveformWidget::drawSearchBar(QPainter &painter)
{
    int searchBarHeight = 25;

    // Draw search bar background with focus indication
    QColor searchBgColor = isSearchFocused ? QColor(90, 90, 100) : QColor(70, 70, 80);
    painter.fillRect(0, timeMarkersHeight, signalNamesWidth, searchBarHeight, searchBgColor);

    // Draw border when focused
    if (isSearchFocused)
    {
        painter.setPen(QPen(QColor(100, 150, 255), 2));
        painter.drawRect(1, timeMarkersHeight + 1, signalNamesWidth - 2, searchBarHeight - 2);
    }

    // Draw search icon or label
    painter.setPen(QPen(Qt::white));
    painter.drawText(5, timeMarkersHeight + searchBarHeight - 8, "🔍");

    // Draw search text with cursor
    if (searchText.isEmpty())
    {
        painter.setPen(QPen(QColor(180, 180, 180)));
        painter.drawText(25, timeMarkersHeight + searchBarHeight - 8, "Search signals...");
    }
    else
    {
        painter.setPen(QPen(Qt::white));
        painter.drawText(25, timeMarkersHeight + searchBarHeight - 8, searchText);

        // Draw blinking cursor when focused (optional)
        if (isSearchFocused)
        {
            int textWidth = painter.fontMetrics().horizontalAdvance(searchText);
            int cursorX = 25 + textWidth + 2;
            painter.drawLine(cursorX, timeMarkersHeight + 5, cursorX, timeMarkersHeight + searchBarHeight - 5);
        }

        // Draw result count
        if (!searchResults.isEmpty())
        {
            QString resultText = QString("(%1)").arg(searchResults.size());
            int textWidth = painter.fontMetrics().horizontalAdvance(resultText);
            painter.drawText(signalNamesWidth - textWidth - 5, timeMarkersHeight + searchBarHeight - 8, resultText);
        }
    }

    // Update top margin to account for search bar
    // topMargin = searchBarHeight;
}

// Add this function to calculate which signals are visible
QList<int> WaveformWidget::getVisibleSignalIndices() const
{
    QList<int> visibleIndices;

    if (displayItems.isEmpty())
        return visibleIndices;

    int viewportTop = verticalOffset;
    int viewportBottom = verticalOffset + height();
    int buffer = visibleSignalBuffer * 30; // 30 pixels per signal

    int currentY = topMargin + timeMarkersHeight;

    for (int i = 0; i < displayItems.size(); i++)
    {
        int itemHeight = displayItems[i].getHeight();
        int itemBottom = currentY + itemHeight;

        // Check if item is within visible range (with buffer)
        if (itemBottom >= viewportTop - buffer && currentY <= viewportBottom + buffer)
        {
            visibleIndices.append(i);
        }

        currentY += itemHeight;

        // Early exit if we're past the visible area with buffer
        if (currentY > viewportBottom + buffer)
        {
            break;
        }
    }

    return visibleIndices;
}

// Add this function to update the visible signals
void WaveformWidget::updateVisibleSignals()
{
    visibleSignalIndices = getVisibleSignalIndices();
}

void WaveformWidget::searchSignals(const QString &searchText)
{
    handleSearchInput(searchText);
}

void WaveformWidget::clearSearch()
{
    handleSearchInput("");
}

void WaveformWidget::handleSearchInput(const QString &text)
{
    searchText = text;
    isSearchActive = !searchText.isEmpty();
    updateSearchResults();
    update();
}

void WaveformWidget::updateSearchResults()
{
    searchResults.clear();

    if (!isSearchActive || searchText.isEmpty())
    {
        // If no search, show all signals
        for (int i = 0; i < displayItems.size(); i++)
        {
            if (displayItems[i].type == DisplayItem::Signal)
            {
                searchResults.insert(i);
            }
        }
    }
    else
    {
        // Filter signals based on search text
        QString searchLower = searchText.toLower();
        for (int i = 0; i < displayItems.size(); i++)
        {
            if (displayItems[i].type == DisplayItem::Signal)
            {
                QString signalName = displayItems[i].getFullPath().toLower();
                if (signalName.contains(searchLower))
                {
                    searchResults.insert(i);
                }
            }
        }
    }

    // qDebug() << "Search results:" << searchResults;
    applySearchFilter();
}

void WaveformWidget::applySearchFilter()
{
    if (isSearchActive)
    {
        // Only select search results, don't filter them out
        selectedItems = searchResults;
        if (!selectedItems.isEmpty())
        {
            lastSelectedItem = *selectedItems.begin();
        }
        else
        {
            lastSelectedItem = -1;
        }
    }
    else
    {
        // Clear selection when search is inactive
        selectedItems.clear();
        lastSelectedItem = -1;
    }
    update();
    emit itemSelected(lastSelectedItem);
}

void WaveformWidget::ensureSignalLoaded(const QString &identifier)
{
    if (!loadedSignalIdentifiers.contains(identifier))
    {
        // Load the signal data - FIXED: use loadSignalsData instead of loadSignalData
        QList<QString> signalsToLoad = {identifier};
        vcdParser->loadSignalsData(signalsToLoad);
        loadedSignalIdentifiers.insert(identifier);

        // Manage cache size
        if (loadedSignalIdentifiers.size() > MAX_CACHED_SIGNALS)
        {
            // Remove least recently used signal
            if (!loadedSignalIdentifiers.isEmpty())
            {
                QString oldestSignal = *loadedSignalIdentifiers.begin();
                loadedSignalIdentifiers.remove(oldestSignal);
            }
        }
    }
}
