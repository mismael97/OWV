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
      signalHeight(24), // Only signalHeight remains
      lineWidth(1),
      isDragging(false),
      isDraggingItem(false),
      dragItemIndex(-1),
      dragStartX(0),
      dragStartOffset(0),
      dragStartY(0),
      lastSelectedItem(-1),
      busDisplayFormat(Hex),
      draggingNamesSplitter(false),
      draggingValuesSplitter(false),
      cursorTime(0),
      showCursor(true),
      verticalOffset(0),
      isSearchActive(false),
      MAX_CACHED_SIGNALS(1000)
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
        
        update(); });
    qDebug() << "WaveformWidget constructor completed";
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

void WaveformWidget::setBusDisplayFormat(BusFormat format)
{
    busDisplayFormat = format;
    update();
}

void WaveformWidget::drawSignalValuesColumn(QPainter &painter, int cursorTime)
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
    painter.drawText(valuesColumnStart + 5, timeMarkersHeight - 8, "Value");

    // Set up clipping to exclude pinned areas from scrolling
    painter.setClipRect(valuesColumnStart, timeMarkersHeight, valuesColumnWidth, height() - timeMarkersHeight);

    // FIXED: Use same starting position as names column
    int currentY = timeMarkersHeight - verticalOffset;

    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;

        // Skip drawing if item is outside visible area
        if (currentY + itemHeight <= timeMarkersHeight)
        {
            currentY += itemHeight;
            continue;
        }
        if (currentY >= height())
        {
            break;
        }

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

            // FIX: Use the signal's fullName to get the value
            QString value = getSignalValueAtTime(signal.fullName, cursorTime);

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

            // Center text vertically within the item
            QFontMetrics fm(painter.font());
            int textY = currentY + (itemHeight + fm.ascent() - fm.descent()) / 2;

            painter.setPen(QPen(Qt::white));
            painter.drawText(valuesColumnStart + 5, textY, displayValue);
        }

        // Draw horizontal separator
        painter.setPen(QPen(QColor(80, 80, 80)));
        painter.drawLine(valuesColumnStart, currentY + itemHeight,
                         valuesColumnStart + valuesColumnWidth, currentY + itemHeight);

        currentY += itemHeight;
    }

    // Reset clipping
    painter.setClipping(false);
}

void WaveformWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    // Global safety check - reset if zoom is completely unreasonable
    if (timeScale > 1000.0 || timeScale < 0.001)
    {
        qDebug() << "Global emergency: Resetting unreasonable zoom:" << timeScale;
        timeScale = 1.0;
        timeOffset = 0;
    }

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

    drawSignalNamesColumn(painter);
    drawSignalValuesColumn(painter, cursorTime); // FIX: Pass cursorTime here
    drawWaveformArea(painter);
    drawTimeCursor(painter);
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

    // Set up clipping to exclude pinned areas from scrolling
    painter.setClipRect(0, timeMarkersHeight, signalNamesWidth, height() - timeMarkersHeight);

    // FIXED: Start drawing signals right below the timeline header
    int currentY = timeMarkersHeight - verticalOffset;

    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;

        // Skip drawing if item is completely outside visible area
        if (currentY + itemHeight <= timeMarkersHeight)
        {
            currentY += itemHeight;
            continue;
        }
        if (currentY >= height())
        {
            break;
        }

        // Draw background based on selection and type
        bool isSelected = selectedItems.contains(i);
        bool isSearchMatch = searchResults.contains(i);

        if (isSelected)
        {
            painter.fillRect(0, currentY, signalNamesWidth, itemHeight, QColor(60, 60, 90));
        }
        else if (isSearchActive && isSearchMatch)
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
        else if (isSearchActive && isSearchMatch)
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

        // Center text vertically within the item
        QFontMetrics fm(painter.font());
        int textY = currentY + (itemHeight + fm.ascent() - fm.descent()) / 2;
        painter.drawText(textIndent, textY, displayName);

        // Draw horizontal separator
        painter.setPen(QPen(QColor(80, 80, 80)));
        painter.drawLine(0, currentY + itemHeight, signalNamesWidth, currentY + itemHeight);

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

    // FIXED: Apply translation that matches the columns
    painter.translate(waveformStartX, timeMarkersHeight - verticalOffset);

    // Draw background for scrollable area - use the full calculated height
    int totalHeight = calculateTotalHeight();
    painter.fillRect(0, 0, width() - waveformStartX, totalHeight, QColor(30, 30, 30));

    if (!displayItems.isEmpty())
    {
        drawSignals(painter);
    }

    // Reset translation and clipping
    painter.translate(-waveformStartX, -timeMarkersHeight + verticalOffset);
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
    // FIXED: Start at position 0 since we're already translated
    int currentY = 0;

    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;

        // Skip drawing if item is completely outside visible area
        int visibleTop = verticalOffset;
        int visibleBottom = verticalOffset + (height() - timeMarkersHeight);

        if (currentY + itemHeight < visibleTop)
        {
            currentY += itemHeight;
            continue;
        }
        if (currentY > visibleBottom)
        {
            break;
        }

        if (item.type == DisplayItem::Signal)
        {
            const VCDSignal &signal = item.signal.signal;

            // FIXED: Draw at the currentY position (no additional offset needed)
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
    const auto changes = vcdParser->getValueChangesForSignal(signal.fullName);
    if (changes.isEmpty())
        return;

    // Emergency check for extreme zoom
    if (timeScale > 1000.0 || timeScale < 0.001)
    {
        qDebug() << "Emergency: Skipping waveform drawing due to extreme zoom:" << timeScale;
        return;
    }

    // Check if user has set a custom color
    bool hasCustomColor = signalColors.contains(signal.fullName);
    QColor customColor = hasCustomColor ? signalColors[signal.fullName] : QColor();

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

        // Determine color for the HORIZONTAL segment
        QColor horizontalColor;

        bool prevIsX = (prevValue == "x" || prevValue == "X");
        bool prevIsZ = (prevValue == "z" || prevValue == "Z");
        bool isX = (change.value == "x" || change.value == "X");
        bool isZ = (change.value == "z" || change.value == "Z");

        // If user has chosen a custom color, use it for all horizontal segments
        if (hasCustomColor)
        {
            horizontalColor = customColor;
        }
        else
        {
            // No custom color - use value-based colors for horizontal segments
            if (prevIsX)
            {
                horizontalColor = QColor(255, 0, 0); // Red for X
            }
            else if (prevIsZ)
            {
                horizontalColor = QColor(255, 165, 0); // Orange for Z
            }
            else if (prevValue == "0")
            {
                horizontalColor = QColor(0x01, 0xFF, 0xFF); // Cyan for 0
            }
            else if (prevValue == "1")
            {
                horizontalColor = QColor(0, 255, 0); // Green for 1
            }
            else
            {
                horizontalColor = QColor(0xFF, 0xE6, 0xCD); // Default for other values
            }
        }

        // Draw the HORIZONTAL segment based on previous value
        painter.setPen(QPen(horizontalColor, lineWidth));
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

        // Draw VERTICAL transition line if value changed
        if (prevValue != change.value)
        {
            int fromY, toY;

            // Determine starting Y position based on PREVIOUS value
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

            // Determine ending Y position based on CURRENT value
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

            // Determine color for VERTICAL line
            QColor verticalColor;
            if (hasCustomColor)
            {
                // Use custom color for vertical lines too
                verticalColor = customColor;
            }
            else
            {
                // No custom color - vertical lines use CYAN
                verticalColor = QColor(0x01, 0xFF, 0xFF); // Cyan
            }

            painter.setPen(QPen(verticalColor, lineWidth));
            painter.drawLine(currentX, fromY, currentX, toY);
        }

        prevTime = change.timestamp;
        prevValue = change.value;
        prevX = currentX;
    }

    // Draw the final segment
    QColor finalColor;

    bool finalIsX = (prevValue == "x" || prevValue == "X");
    bool finalIsZ = (prevValue == "z" || prevValue == "Z");

    // If user has chosen a custom color, use it for the final segment
    if (hasCustomColor)
    {
        finalColor = customColor;
    }
    else
    {
        // No custom color - use value-based color for final segment
        if (finalIsX)
        {
            finalColor = QColor(255, 0, 0); // Red for X
        }
        else if (finalIsZ)
        {
            finalColor = QColor(255, 165, 0); // Orange for Z
        }
        else if (prevValue == "0")
        {
            finalColor = QColor(0x01, 0xFF, 0xFF); // Cyan for 0
        }
        else if (prevValue == "1")
        {
            finalColor = QColor(0, 255, 0); // Green for 1
        }
        else
        {
            finalColor = QColor(0xFF, 0xE6, 0xCD); // Default for other values
        }
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

    // Draw a prominent vertical line with the same line width
    painter.setPen(QPen(signalColor.lighter(150), lineWidth));
    painter.drawLine(x, top, x, bottom);

    // Add small cross markers for visibility
    int crossSize = 3;

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
    const auto changes = vcdParser->getValueChangesForSignal(signal.fullName);
    if (changes.isEmpty())
        return;

    // Emergency check for extreme zoom
    if (timeScale > 1000.0 || timeScale < 0.001)
    {
        qDebug() << "Emergency: Skipping bus drawing due to extreme zoom:" << timeScale;
        return;
    }

    // FIX: Use getSignalColor to get the color
    QColor signalColor = getSignalColor(signal.fullName);

    // USE EXACTLY THE SAME DIMENSIONS AS drawSignalWaveform
    int busTop = yPos + 3;                   // Same as signalTop
    int busBottom = yPos + signalHeight - 3; // Same as signalBottom
    int busMidY = yPos + signalHeight / 2;   // Same as signalMidY
    int textY = busMidY + 4;
    int waveformHeight = busBottom - busTop; // This should now be identical to signal waveform height

    int prevTime = 0;
    QString prevValue = getBusValueAtTime(signal.fullName, 0);
    int prevX = timeToX(prevTime);

    // Draw clean bus background - but make it the same visual thickness
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

        // Draw the value region - using same height as signals
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

        // Draw clean transition line - using same line width as signals
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

    // Draw clean bus outline - using same line width as signal transitions
    painter.setPen(QPen(signalColor, lineWidth)); // Use lineWidth instead of hardcoded 2
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

    // Horizontal scrolling (unchanged)
    const int LEFT_MARGIN_PIXELS = static_cast<int>(-10 * timeScale);
    const int RIGHT_MARGIN_PIXELS = static_cast<int>(100 * timeScale);
    int timelinePixelWidth = static_cast<int>(vcdParser->getEndTime() * timeScale);
    int totalPixelWidth = timelinePixelWidth + LEFT_MARGIN_PIXELS + RIGHT_MARGIN_PIXELS;
    int maxScrollOffset = qMax(0, totalPixelWidth - viewportWidth);

    horizontalScrollBar->setRange(0, maxScrollOffset);
    horizontalScrollBar->setPageStep(viewportWidth);
    horizontalScrollBar->setSingleStep(viewportWidth / 10);

    // Vertical scrolling - calculate total content height
    int totalHeight = calculateTotalHeight();
    int visibleHeight = height() - timeMarkersHeight; // Subtract pinned timeline

    // Only enable vertical scrollbar if content is taller than visible area
    if (totalHeight > visibleHeight)
    {
        int maxVerticalOffset = totalHeight - visibleHeight;
        verticalScrollBar->setRange(0, maxVerticalOffset);
        verticalScrollBar->setPageStep(visibleHeight);
        verticalScrollBar->setSingleStep(30);
        verticalScrollBar->setVisible(true);

        // Ensure current offset is within bounds
        if (verticalOffset > maxVerticalOffset)
        {
            verticalOffset = maxVerticalOffset;
            verticalScrollBar->setValue(verticalOffset);
        }
    }
    else
    {
        verticalScrollBar->setRange(0, 0);
        verticalScrollBar->setVisible(false);
        verticalOffset = 0;
    }

    qDebug() << "Scrollbar - Total height:" << totalHeight
             << "Visible height:" << visibleHeight
             << "Vertical offset:" << verticalOffset
             << "Max vertical:" << verticalScrollBar->maximum();
}

int WaveformWidget::calculateTotalHeight() const
{
    if (displayItems.isEmpty())
        return timeMarkersHeight; // Just the timeline area

    int totalHeight = topMargin + timeMarkersHeight;
    for (const auto &item : displayItems)
    {
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;
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

QString WaveformWidget::getSignalValueAtTime(const QString &fullName, int time) const // CHANGE: parameter name
{
    // Use lazy loading - use fullName
    const auto changes = vcdParser->getValueChangesForSignal(fullName); // CHANGE: use fullName
    QString value = "0";

    for (const auto &change : changes)
    {
        if (change.timestamp > time)
            break;
        value = change.value;
    }

    return value;
}

QString WaveformWidget::getBusValueAtTime(const QString &fullName, int time) const // CHANGE: parameter name
{
    // Use lazy loading - use fullName
    const auto changes = vcdParser->getValueChangesForSignal(fullName); // CHANGE: use fullName
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

    int yPos = timeMarkersHeight; // Start below the pinned timeline
    for (int i = 0; i < index; i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;
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
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;

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
        handleWaveformClick(event->pos());

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
        isSearchFocused = false;
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
    // Signal height adjustment shortcuts
    else if (event->modifiers() & Qt::ControlModifier)
    {
        if (event->key() == Qt::Key_Up)
        {
            setSignalHeight(signalHeight + 2);
            event->accept();
            return;
        }
        else if (event->key() == Qt::Key_Down)
        {
            setSignalHeight(signalHeight - 2);
            event->accept();
            return;
        }
    }
    else if (!event->text().isEmpty() && event->text().at(0).isPrint())
    {
        // Handle regular text input for search
        if (!isSearchActive)
        {
            handleSearchInput(event->text());
        }
        else
        {
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
        QList<QString> fullNames; // CHANGE: use fullNames
        for (const auto &signal : visibleSignals)
        {
            fullNames.append(signal.fullName); // CHANGE: use fullName
        }

        // Load signal data before displaying
        vcdParser->loadSignalsData(fullNames); // This now uses fullNames
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

    updateScrollBar();
    update();
    emit itemSelected(-1);
}

// Update WaveformWidget::contextMenuEvent to handle waveform area context menus
void WaveformWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int itemIndex = getItemAtPosition(event->pos());
    
    // If no item clicked but we're in waveform area, try to find signal under cursor
    if (itemIndex == -1) {
        int waveformStartX = signalNamesWidth + valuesColumnWidth;
        QPoint adjustedPos = event->pos();
        adjustedPos.setX(adjustedPos.x() - waveformStartX);
        itemIndex = getItemAtPosition(adjustedPos);
    }
    
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
    if (pos.y() < timeMarkersHeight)
        return -1;

    // FIXED: Adjust for vertical offset correctly
    int y = pos.y() + verticalOffset - timeMarkersHeight;

    if (y < 0)
        return -1;

    int currentY = 0;
    for (int i = 0; i < displayItems.size(); i++)
    {
        const auto &item = displayItems[i];
        int itemHeight = (item.type == DisplayItem::Signal) ? signalHeight : 30;

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

QColor WaveformWidget::getSignalColor(const QString &fullName) const
{
    // If user has set a custom color, use it
    if (signalColors.contains(fullName))
    {
        return signalColors[fullName];
    }

    // Default to #ffe6cd for all signals (this will be overridden for 0 and 1 values)
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
            currentColor = getSignalColor(signal.fullName);
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

        // Apply the color to all selected signals using fullName
        for (int index : selectedItems)
        {
            if (isSignalItem(index))
            {
                const VCDSignal &signal = displayItems[index].signal.signal;
                signalColors[signal.fullName] = newColor;
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

// Update the signal loading functions to use fullName
void WaveformWidget::ensureSignalLoaded(const QString &fullName) // CHANGE: parameter name
{
    if (!loadedSignalIdentifiers.contains(fullName)) // CHANGE: use fullName
    {
        // Load the signal data
        QList<QString> signalsToLoad = {fullName};
        vcdParser->loadSignalsData(signalsToLoad);
        loadedSignalIdentifiers.insert(fullName); // CHANGE: use fullName

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

void WaveformWidget::setNavigationMode(NavigationMode mode)
{
    navigationMode = mode;
    updateEventList();
    currentEventIndex = -1;
}

void WaveformWidget::navigateToPreviousEvent()
{
    if (eventTimestamps.isEmpty() || currentEventIndex <= 0)
        return;

    currentEventIndex--;
    int targetTime = eventTimestamps[currentEventIndex];

    // Center the view on the target time
    int viewportWidth = width() - signalNamesWidth - valuesColumnWidth;
    int targetX = timeToX(targetTime);
    timeOffset = qMax(0, targetX - viewportWidth / 2);

    // Update cursor
    cursorTime = targetTime;
    showCursor = true;

    updateScrollBar();
    update();
    emit timeChanged(cursorTime);
}

void WaveformWidget::navigateToNextEvent()
{
    if (eventTimestamps.isEmpty() || currentEventIndex >= eventTimestamps.size() - 1)
        return;

    if (currentEventIndex == -1)
    {
        currentEventIndex = 0;
    }
    else
    {
        currentEventIndex++;
    }

    int targetTime = eventTimestamps[currentEventIndex];

    // Center the view on the target time
    int viewportWidth = width() - signalNamesWidth - valuesColumnWidth;
    int targetX = timeToX(targetTime);
    timeOffset = qMax(0, targetX - viewportWidth / 2);

    // Update cursor
    cursorTime = targetTime;
    showCursor = true;

    updateScrollBar();
    update();
    emit timeChanged(cursorTime);
}

bool WaveformWidget::hasPreviousEvent() const
{
    return !eventTimestamps.isEmpty() && currentEventIndex > 0;
}

bool WaveformWidget::hasNextEvent() const
{
    return !eventTimestamps.isEmpty() &&
           (currentEventIndex < eventTimestamps.size() - 1 || currentEventIndex == -1);
}

void WaveformWidget::updateEventList()
{
    eventTimestamps.clear();

    if (selectedItems.isEmpty() || !vcdParser)
        return;

    // Get the first selected signal
    int selectedIndex = *selectedItems.begin();
    if (!isSignalItem(selectedIndex))
        return;

    const VCDSignal &signal = getSignalFromItem(selectedIndex);
    const auto changes = vcdParser->getValueChangesForSignal(signal.fullName);

    if (changes.isEmpty())
        return;

    // Collect events based on navigation mode
    QString prevValue;
    for (int i = 0; i < changes.size(); i++)
    {
        const auto &change = changes[i];
        bool includeEvent = false;

        switch (navigationMode)
        {
        case ValueChange:
            includeEvent = (i > 0); // All changes except first
            break;

        case SignalRise:
            includeEvent = (prevValue == "0" && change.value == "1");
            break;

        case SignalFall:
            includeEvent = (prevValue == "1" && change.value == "0");
            break;

        case XValues:
            includeEvent = (change.value.toLower() == "x");
            break;

        case ZValues:
            includeEvent = (change.value.toLower() == "z");
            break;
        }

        if (includeEvent)
        {
            eventTimestamps.append(change.timestamp);
        }

        prevValue = change.value;
    }

    // Set current index based on cursor position
    currentEventIndex = findEventIndexForTime(cursorTime);
}

int WaveformWidget::findEventIndexForTime(int time) const
{
    for (int i = 0; i < eventTimestamps.size(); i++)
    {
        if (eventTimestamps[i] >= time)
        {
            return i;
        }
    }
    return eventTimestamps.size() - 1;
}

int WaveformWidget::getCurrentEventTime() const
{
    if (currentEventIndex >= 0 && currentEventIndex < eventTimestamps.size())
    {
        return eventTimestamps[currentEventIndex];
    }
    return cursorTime;
}

void WaveformWidget::selectSignalAtPosition(const QPoint &pos)
{
    int itemIndex = getItemAtPosition(pos);
    if (itemIndex >= 0 && isSignalItem(itemIndex))
    {
        // Single selection
        selectedItems.clear();
        selectedItems.insert(itemIndex);
        lastSelectedItem = itemIndex;

        updateEventList(); // Update events for newly selected signal

        update();
        emit itemSelected(itemIndex);
    }
}

void WaveformWidget::handleWaveformClick(const QPoint &pos)
{
    int waveformStartX = signalNamesWidth + valuesColumnWidth;

    // Check if click is in waveform area (not in names or values columns)
    if (pos.x() >= waveformStartX && pos.y() >= timeMarkersHeight)
    {
        // Try to select signal at this position
        selectSignalAtPosition(pos);

        // Also set cursor time
        int clickXInWaveform = pos.x() - waveformStartX;
        cursorTime = xToTime(clickXInWaveform);
        showCursor = true;

        // Update event index based on new cursor position
        currentEventIndex = findEventIndexForTime(cursorTime);

        update();
        emit timeChanged(cursorTime);
    }
}