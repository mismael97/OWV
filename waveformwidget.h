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
#include <memory>

#include "vcdparser.h"

// Simple data structures instead of complex inheritance
struct DisplaySignal {
    VCDSignal signal;
    bool isInGroup = false;
    QString groupName;
};

struct SignalGroup {
    QString name;
    QList<DisplaySignal> groupSignals;  // Changed from 'signals'
    bool collapsed = false;
    int displayIndex = -1;
    
    bool isEmpty() const { return groupSignals.isEmpty(); }
    int getSignalCount() const { return groupSignals.size(); }
    void addSignal(const DisplaySignal& signal) { groupSignals.append(signal); }
    void removeSignal(const QString& identifier) {
        for (int i = 0; i < groupSignals.size(); i++) {
            if (groupSignals[i].signal.identifier == identifier) {
                groupSignals.removeAt(i);
                break;
            }
        }
    }
    bool containsSignal(const QString& identifier) const {
        for (const auto& sig : groupSignals) {
            if (sig.signal.identifier == identifier) {
                return true;
            }
        }
        return false;
    }
};

struct DisplaySpace {
    QString name;
};

// Unified display item
struct DisplayItem {
    enum Type { Signal, Group, Space };
    Type type;
    
    // Only one of these is valid based on type
    DisplaySignal signal;
    SignalGroup group;
    DisplaySpace space;
    
    // Constructor for signal
    static DisplayItem createSignal(const VCDSignal& sig, bool inGroup = false, const QString& groupName = "") {
        DisplayItem item;
        item.type = Signal;
        item.signal = {sig, inGroup, groupName};
        return item;
    }
    
    // Constructor for group
    static DisplayItem createGroup(const QString& name) {
        DisplayItem item;
        item.type = Group;
        item.group = {name, {}, false, -1};
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
            case Group: 
                return "⫿ " + group.name;
            case Space: 
                return space.name.isEmpty() ? "⏐" : "⏐ " + space.name;
        }
        return "";
    }
    
    int getHeight() const {
        switch(type) {
            case Signal: return 30;
            case Group: return group.collapsed ? 30 : (30 + group.groupSignals.size() * 30);
            case Space: return 30;
        }
        return 30;
    }
    
    bool isSelectable() const { return true; }
    bool isMovable() const { return type != Group; }
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
    
    // Group management
    void createGroupFromSelection(const QString& name);
    void addSignalsToGroup(const QList<int>& signalIndices, const QString& groupName);
    void removeSignalsFromGroup(const QList<int>& signalIndices);
    QList<QString> getGroupNames() const;
    
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
    

private slots:
    void onAddToGroupActionTriggered();

private:
    void drawSignalNamesColumn(QPainter &painter);
    void drawWaveformArea(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawSignals(QPainter &painter);
    void drawSignalWaveform(QPainter &painter, const VCDSignal &signal, int yPos);
    void drawGroupWaveforms(QPainter &painter, const SignalGroup &group, int yPos);
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
     void updateAllGroupIndices();
    void updateGroupIndicesAfterDeletion(const QList<int> &deletedIndices);
    bool isSignalInGroup(int index) const;
    int getGroupBottomIndex(int groupIndex) const;
    void selectGroup(int groupIndex);
        // Drag and movement
    void moveFreeItem(int itemIndex, int newIndex);
    void updateGroupDisplayIndices();
    void updateSelection(int itemIndex, bool isMultiSelect);
    void startDragSignal(int signalIndex);
    void updateSignalGroupMembership(int signalIndex);
    void debugGroupInfo();
    void addGroup();
    void addSignalToGroup(int signalIndex, const QString& groupName);
    
    // Drag and drop
    void startDrag(int itemIndex);
    void performDrag(int mouseY);
    void moveGroup(int groupIndex, int newIndex);

    
    // Selection
    void handleMultiSelection(int itemIndex, QMouseEvent *event);
    
    // Helper methods
    bool isSignalItem(int index) const { 
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Signal; 
    }
    bool isSpaceItem(int index) const { 
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Space; 
    }
    bool isGroupItem(int index) const { 
        return index >= 0 && index < displayItems.size() && displayItems[index].type == DisplayItem::Group; 
    }
    VCDSignal getSignalFromItem(int index) const {
        return isSignalItem(index) ? displayItems[index].signal.signal : VCDSignal();
    }
    int findGroupIndexByName(const QString& name) const;
    void convertToFreeSignal(int itemIndex);
    void removeSignalFromAllGroups(const QString& identifier);

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

    // Context menu state
    QString lastContextGroupName;
    int lastContextSignalIndex;

    QScrollBar *horizontalScrollBar;
};

#endif // WAVEFORMWIDGET_H