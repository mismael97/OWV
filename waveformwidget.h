#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QVector>
#include <QList>
#include <QLabel>
#include <QMenu>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSet>
#include <QInputDialog>
#include <QColorDialog>

#include "vcdparser.h"

// Simple signal display structure
struct DisplaySignal
{
    VCDSignal signal;
};

// Space structure
struct DisplaySpace
{
    QString name;
};

// Unified display item
struct DisplayItem
{
    enum Type
    {
        Signal,
        Space
    };
    Type type;

    // Only one of these is valid based on type
    DisplaySignal signal;
    DisplaySpace space;

    // Constructor for signal
    static DisplayItem createSignal(const VCDSignal &sig)
    {
        DisplayItem item;
        item.type = Signal;
        item.signal = {sig};
        return item;
    }

    // Constructor for space
    static DisplayItem createSpace(const QString &name = "")
    {
        DisplayItem item;
        item.type = Space;
        item.space = {name};
        return item;
    }

    QString getName() const
    {
        switch (type)
        {
        case Signal:
        {
            QString name = signal.signal.scope.isEmpty() ? signal.signal.name : signal.signal.scope + "." + signal.signal.name;
            // Remove any width information like "[3:0]" from the name
            int bracketPos = name.indexOf('[');
            if (bracketPos != -1)
            {
                name = name.left(bracketPos).trimmed();
            }
            return name;
        }
        case Space:
            return space.name.isEmpty() ? "⏐" : "⏐ " + space.name;
        }
        return "";
    }

    // Helper function to get full scope path for searching
    QString getFullPath() const
    {
        if (type == Signal)
        {
            QString fullPath = signal.signal.scope.isEmpty() ? signal.signal.name : signal.signal.scope + "." + signal.signal.name;
            // Remove any width information for consistency
            int bracketPos = fullPath.indexOf('[');
            if (bracketPos != -1)
            {
                fullPath = fullPath.left(bracketPos).trimmed();
            }
            return fullPath;
        }
        return getName();
    }

    int getHeight() const
    {
        // Use reasonable default heights - these will be overridden by the actual drawing functions
        // The actual drawing will use the configurable heights from WaveformWidget
        switch (type)
        {
        case Signal:
            return 30; // Default height for signals
        case Space:
            return 30; // Fixed height for spaces
        }
        return 30;
    }

    bool isSelectable() const { return true; }
    bool isMovable() const { return true; }
};

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    int getCursorTime() const { return cursorTime; }
    void navigateToTime(int time);
    enum NavigationMode
    {
        ValueChange,
        SignalRise,
        SignalFall,
        XValues,
        ZValues
    };
    void setNavigationMode(NavigationMode mode);
    void navigateToPreviousEvent();
    void navigateToNextEvent();
    bool hasPreviousEvent() const;
    bool hasNextEvent() const;

    void selectSignalAtPosition(const QPoint &pos);

    void ensureSignalLoaded(const QString &identifier);
    void searchSignals(const QString &searchText);
    void clearSearch();

    // Add these to the public section of WaveformWidget class
    int getSignalHeight() const { return signalHeight; }
    int getLineWidth() const { return lineWidth; }
    void setSignalHeight(int height)
    {
        signalHeight = qMax(5, qMin(50, height)); // Clamp between 5 and 50
        update();
    }
    void setLineWidth(int width)
    {
        lineWidth = qMax(1, qMin(5, width)); // Clamp between 1 and 5
        update();
    }
    enum BusFormat
    {
        Hex,
        Binary,
        Octal,
        Decimal
    };

    explicit WaveformWidget(QWidget *parent = nullptr);
    void setVcdData(VCDParser *parser);
    void setVisibleSignals(const QList<VCDSignal> &visibleSignals);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void removeSelectedSignals();
    void selectAllSignals();
    void resetSignalColors();
    void setBusDisplayFormat(BusFormat format);
    BusFormat getBusDisplayFormat() const { return busDisplayFormat; }
    int getSelectedSignal() const { return selectedItems.isEmpty() ? -1 : *selectedItems.begin(); }
    QList<int> getSelectedItemIndices() const { return selectedItems.values(); }

    // Item management
    int getItemCount() const { return displayItems.size(); }
    const DisplayItem *getItem(int index) const;

signals:
    void timeChanged(int time);
    void itemSelected(int itemIndex);
    void contextMenuRequested(const QPoint &pos, int itemIndex);
        void cursorTimeChanged(int time);  // ADD THIS - for yellow timeline cursor


protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    double calculateZoomFitScale() const
    {
        if (!vcdParser || vcdParser->getEndTime() <= 0)
        {
            return 1.0;
        }

        int availableWidth = width() - signalNamesWidth - valuesColumnWidth;

        // Use the same calculation as zoomFit but just return the scale
        const int PADDING = 10;
        int totalTimeRange = vcdParser->getEndTime() + (2 * PADDING);
        totalTimeRange = qMax(20, totalTimeRange);

        if (availableWidth <= 10)
        {
            return 1.0;
        }

        double zoomFitScale = static_cast<double>(availableWidth - (2 * PADDING)) / vcdParser->getEndTime();
        return qMax(0.02, qMin(50.0, zoomFitScale));
    }
    void resetNavigationForCurrentSignal();

    // void navigateToTime(int targetTime);
    int findEventIndexForTime(int time, const QString &signalFullName) const;

    bool isSignalSelected(const VCDSignal &signal) const
    {
        for (int i = 0; i < displayItems.size(); i++)
        {
            if (selectedItems.contains(i) &&
                displayItems[i].type == DisplayItem::Signal &&
                displayItems[i].signal.signal.fullName == signal.fullName)
            {
                return true;
            }
        }
        return false;
    }
    int selectedLineWidth = 3;

    // Navigation
    QMap<QString, QVector<int>> signalEventTimestamps; // Maps signal fullName to its events
    QMap<QString, int> signalCurrentEventIndex;        // Maps signal fullName to its current event index
    QString currentlyNavigatedSignal;                  // Which signal we're currently navigating
    NavigationMode navigationMode = ValueChange;
    int currentEventIndex = -1;
    QVector<int> eventTimestamps;

    void updateEventList();
    int findEventIndexForTime(int time) const;
    int getCurrentEventTime() const;

    // Signal selection from waveform area
    void handleWaveformClick(const QPoint &pos);
    // Track which signals have been loaded to avoid reloading
    QSet<QString> loadedSignalIdentifiers;

    // Signal data cache with limits
    QMap<QString, QVector<VCDValueChange>> signalDataCache;
    const int MAX_CACHED_SIGNALS = 1000; // Limit cache size
    QStringList recentlyUsedSignals;     // For LRU cache management

    // Make sure these search methods exist:
    void handleSearchInput(const QString &text);
    void updateSearchResults();
    void applySearchFilter();
    // Add these to the private section of WaveformWidget class
    int signalHeight = 24; // Configurable signal height for both signals and buses
    int lineWidth = 1;     // Configurable line width

    // Helper methods for virtual rendering
    int calculateTotalHeight() const;
    void updateCursorTime(const QPoint &pos);
    void drawSignalNamesColumn(QPainter &painter);
    void drawSignalValuesColumn(QPainter &painter, int cursorTime);
    void drawWaveformArea(QPainter &painter);
    void drawTimeCursor(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawSignals(QPainter &painter);
    void drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void drawBusWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void updateScrollBar();
    int timeToX(int time) const;
    int xToTime(int x) const;
    QString getSignalValueAtTime(const QString &identifier, int time) const;
    QString getBusValueAtTime(const QString &identifier, int time) const;
    int calculateTimeStep(int startTime, int endTime) const;
    int getItemAtPosition(const QPoint &pos) const;
    int getItemYPosition(int index) const;
    void showContextMenu(const QPoint &pos, int itemIndex);
    void addSpaceAbove(int index);
    void addSpaceBelow(int index);
    void renameItem(int itemIndex);
    QString promptForName(const QString &title, const QString &defaultName = "");
    void drawCleanTransition(QPainter &painter, int x, int top, int bottom, const QColor &signalColor);

    // Color management
    void changeSignalColor(int itemIndex);
    QColor getSignalColor(const QString &identifier) const;

    // Splitter handling
    bool isOverNamesSplitter(const QPoint &pos) const;
    bool isOverValuesSplitter(const QPoint &pos) const;
    void updateSplitterPositions();

    // Search functionality
    QString searchText;
    bool isSearchActive = false;
    bool isSearchFocused = false;
    QSet<int> searchResults;
    void drawSearchBar(QPainter &painter);

    // Drag and movement
    void startDrag(int itemIndex);
    void performDrag(int mouseY);
    void moveItem(int itemIndex, int newIndex);

    // Selection
    void handleMultiSelection(int itemIndex, QMouseEvent *event);

    // Helper methods
    bool isSignalItem(int index) const
    {
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Signal;
    }
    bool isSpaceItem(int index) const
    {
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Space;
    }
    VCDSignal getSignalFromItem(int index) const
    {
        return isSignalItem(index) ? displayItems[index].signal.signal : VCDSignal();
    }

    // Bus display helpers
    QString formatBusValue(const QString &binaryValue) const;
    bool isValidBinary(const QString &value) const;
    QString binaryToHex(const QString &binaryValue) const;
    QString binaryToOctal(const QString &binaryValue) const;
    QString binaryToDecimal(const QString &binaryValue) const;

    VCDParser *vcdParser;

    // Layout parameters
    int signalNamesWidth = 250;
    int valuesColumnWidth = 120;
    double timeScale;
    int timeOffset;
    int timeMarkersHeight;
    int topMargin;

    // Display items
    QList<DisplayItem> displayItems;

    // Signal colors
    QMap<QString, QColor> signalColors;
    BusFormat busDisplayFormat = Hex;

    // Splitter state
    bool draggingNamesSplitter = false;
    bool draggingValuesSplitter = false;

    // Drag state
    bool isDragging;
    bool isDraggingItem;
    int dragStartX;
    int dragStartOffset;
    int dragItemIndex;
    int dragStartY;
    QPoint dragStartPos;

    // Selection state
    QSet<int> selectedItems;
    int lastSelectedItem;

    // Time cursor and values display
    int cursorTime = 0;
    bool showCursor = true;

    QScrollBar *horizontalScrollBar;
    QScrollBar *verticalScrollBar;
    int verticalOffset = 0;
};

#endif // WAVEFORMWIDGET_H