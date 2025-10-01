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

// New data structures for spaces and groups
struct SpaceItem {
    QString name;
    bool operator==(const SpaceItem& other) const { return name == other.name; }
};

struct GroupItem {
    QString name;
    QList<int> signalIndices; // Indices of signals in this group
    bool collapsed = false;   // Optional: for future collapse/expand functionality
    bool operator==(const GroupItem& other) const { return name == other.name; }
};

// Class for display items using proper memory management
class DisplayItem {
public:
    enum Type { Signal, Space, Group };
    
    DisplayItem() : type(Signal), signalData(nullptr) {}
    DisplayItem(const VCDSignal& sig) : type(Signal), signalData(new VCDSignal(sig)) {}
    DisplayItem(const SpaceItem& sp) : type(Space), spaceData(new SpaceItem(sp)) {}
    DisplayItem(const GroupItem& grp) : type(Group), groupData(new GroupItem(grp)) {}
    
    // Copy constructor
    DisplayItem(const DisplayItem& other) {
        type = other.type;
        switch(type) {
            case Signal: signalData.reset(other.signalData ? new VCDSignal(*other.signalData) : nullptr); break;
            case Space: spaceData.reset(other.spaceData ? new SpaceItem(*other.spaceData) : nullptr); break;
            case Group: groupData.reset(other.groupData ? new GroupItem(*other.groupData) : nullptr); break;
        }
    }
    
    // Assignment operator
    DisplayItem& operator=(const DisplayItem& other) {
        if (this != &other) {
            type = other.type;
            switch(type) {
                case Signal: signalData.reset(other.signalData ? new VCDSignal(*other.signalData) : nullptr); break;
                case Space: spaceData.reset(other.spaceData ? new SpaceItem(*other.spaceData) : nullptr); break;
                case Group: groupData.reset(other.groupData ? new GroupItem(*other.groupData) : nullptr); break;
            }
        }
        return *this;
    }
    
    // Move constructor
    DisplayItem(DisplayItem&& other) = default;
    
    // Move assignment
    DisplayItem& operator=(DisplayItem&& other) = default;
    
    ~DisplayItem() = default; // Now we can have a default destructor
    
    Type getType() const { return type; }
    
    const VCDSignal& getSignal() const { 
        if (type != Signal || !signalData) throw std::runtime_error("Not a signal");
        return *signalData; 
    }
    
    const SpaceItem& getSpace() const { 
        if (type != Space || !spaceData) throw std::runtime_error("Not a space");
        return *spaceData; 
    }
    
    const GroupItem& getGroup() const { 
        if (type != Group || !groupData) throw std::runtime_error("Not a group");
        return *groupData; 
    }
    
    SpaceItem& getSpace() { 
        if (type != Space || !spaceData) throw std::runtime_error("Not a space");
        return *spaceData; 
    }
    
    GroupItem& getGroup() { 
        if (type != Group || !groupData) throw std::runtime_error("Not a group");
        return *groupData; 
    }
    
    QString getName() const {
        switch(type) {
            case Signal: return signalData ? signalData->name : "";
            case Space: return spaceData ? spaceData->name : "";
            case Group: return groupData ? groupData->name : "";
        }
        return "";
    }

private:
    Type type;
    std::unique_ptr<VCDSignal> signalData;
    std::unique_ptr<SpaceItem> spaceData;
    std::unique_ptr<GroupItem> groupData;
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
    int getSelectedSignal() const { return selectedSignals.isEmpty() ? -1 : *selectedSignals.begin(); }
    QList<int> getSelectedSignalIndices() const { return selectedSignals.values(); }
    int signalHeight = 30;

    QList<DisplayItem> displayItems; // Replaces visibleSignals

signals:
    void timeChanged(int time);
    void signalSelected(int signalIndex);
    void contextMenuRequested(const QPoint &pos, int signalIndex);

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
    bool isSignalItem(int index) const;
    bool isSpaceItem(int index) const;
    bool isGroupItem(int index) const;
    void startDragSignal(int signalIndex);
    void performDrag(int mouseY);
    void showContextMenu(const QPoint &pos, int itemIndex);
    void addSpaceAbove(int index);
    void addSpaceBelow(int index);
    void addGroup();
    void handleMultiSelection(int itemIndex, QMouseEvent *event);
    void updateSelection(int itemIndex, bool isMultiSelect);
    void renameItem(int itemIndex);
    QString promptForName(const QString &title, const QString &defaultName = "");
    bool isSignalInGroup(int index) const;
    int getGroupBottomIndex(int groupIndex) const;
    bool isGroupBoundary(int index) const;
    void selectGroup(int groupIndex);
    void updateGroupIndicesAfterDeletion(const QList<int>& deletedIndices);
    void updateAllGroupIndices();
    void debugGroupInfo();
    void updateSignalGroupMembership(int signalIndex);


    VCDParser *vcdParser;

    // Layout parameters
    int signalNamesWidth = 250;
    double timeScale;
    int timeOffset;
    int timeMarkersHeight;
    int topMargin;

    bool isDragging;
    bool isDraggingSignal;
    int dragStartX;
    int dragStartOffset;
    int dragSignalIndex;
    int dragStartY;

    QSet<int> selectedSignals;
    int lastSelectedSignal;

    QScrollBar *horizontalScrollBar;
};

#endif // WAVEFORMWIDGET_H
