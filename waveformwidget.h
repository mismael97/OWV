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

#include "vcdparser.h"

// Simple signal display structure
struct DisplaySignal {
    VCDSignal signal;
};

// Space structure
struct DisplaySpace {
    QString name;
};

// Unified display item
struct DisplayItem {
    enum Type { Signal, Space };
    Type type;
    
    // Only one of these is valid based on type
    DisplaySignal signal;
    DisplaySpace space;
    
    // Constructor for signal
    static DisplayItem createSignal(const VCDSignal& sig) {
        DisplayItem item;
        item.type = Signal;
        item.signal = {sig};
        return item;
    }
    
    // Constructor for space
    static DisplayItem createSpace(const QString& name = "") {
        DisplayItem item;
        item.type = Space;
        item.space = {name};
        return item;
    }
    
    QString getName() const {
        switch(type) {
            case Signal: 
                return signal.signal.scope.isEmpty() ? signal.signal.name : signal.signal.scope + "." + signal.signal.name;
            case Space: 
                return space.name.isEmpty() ? "⏐" : "⏐ " + space.name;
        }
        return "";
    }
    
    int getHeight() const {
        return 30; // Fixed height for all items
    }
    
    bool isSelectable() const { return true; }
    bool isMovable() const { return true; }
};

class WaveformWidget : public QWidget
{
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget *parent = nullptr);
    void setVcdData(VCDParser *parser);
    void setVisibleSignals(const QList<VCDSignal> &visibleSignals);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void removeSelectedSignals();
    void selectAllSignals();
    int getSelectedSignal() const { return selectedItems.isEmpty() ? -1 : *selectedItems.begin(); }
    QList<int> getSelectedItemIndices() const { return selectedItems.values(); }
    
    // Item management
    int getItemCount() const { return displayItems.size(); }
    const DisplayItem* getItem(int index) const;

signals:
    void timeChanged(int time);
    void itemSelected(int itemIndex);
    void contextMenuRequested(const QPoint &pos, int itemIndex);

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
    void drawSignalNamesColumn(QPainter &painter);
    void drawWaveformArea(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawSignals(QPainter &painter);
    void drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void updateScrollBar();
    int timeToX(int time) const;
    int xToTime(int x) const;
    QString getSignalValueAtTime(const QString &identifier, int time) const;
    int calculateTimeStep(int startTime, int endTime) const;
    int getItemAtPosition(const QPoint &pos) const;
    int getItemYPosition(int index) const;
    void showContextMenu(const QPoint &pos, int itemIndex);
    void addSpaceAbove(int index);
    void addSpaceBelow(int index);
    void renameItem(int itemIndex);
    QString promptForName(const QString &title, const QString &defaultName = "");
    
    // Drag and movement
    void startDrag(int itemIndex);
    void performDrag(int mouseY);
    void moveItem(int itemIndex, int newIndex);
    
    // Selection
    void handleMultiSelection(int itemIndex, QMouseEvent *event);
    
    // Helper methods
    bool isSignalItem(int index) const { 
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Signal; 
    }
    bool isSpaceItem(int index) const { 
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Space; 
    }
    VCDSignal getSignalFromItem(int index) const {
        return isSignalItem(index) ? displayItems[index].signal.signal : VCDSignal();
    }

    VCDParser *vcdParser;

    // Layout parameters
    int signalNamesWidth = 250;
    double timeScale;
    int timeOffset;
    int timeMarkersHeight;
    int topMargin;

    // Display items
    QList<DisplayItem> displayItems;

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

    QScrollBar *horizontalScrollBar;
};

#endif // WAVEFORMWIDGET_H