#ifndef SIGNALSELECTIONDIALOG_H
#define SIGNALSELECTIONDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLabel>
#include <QProgressBar>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include "vcdparser.h"

class MainWindow; // Forward declaration

class SignalSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SignalSelectionDialog(QWidget *parent = nullptr);
    ~SignalSelectionDialog();

    void setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals);
    QList<VCDSignal> getSelectedSignals() const;

    // NEW: RTL processing setup
    void setRtlProcessingInfo(MainWindow* mainWindow, const QString& vcdFilePath, bool& rtlProcessed, QString& tempVcdPath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onScopeItemChanged(QTreeWidgetItem *item, int column);
    void selectAll();
    void deselectAll();
    void onSearchTextChanged(const QString &text);
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onSearchTimerTimeout();
    void onLoadProgress(int percentage);
    void onLoadFinished();
    
    // NEW: Filter button slots
    void onFilterInputPorts();
    void onFilterOutputPorts();
    void onFilterInoutPorts();
    void onFilterNetSignals();
    void onFilterRegisters();
    void onFilterAll();

private:
    void processNextChunk();
    void populateTopLevelScopes();
    void performSearch(const QString &text);
    void populateScopeChildren(const QString &scopePath, QTreeWidgetItem *parentItem);
    void displaySearchResults(const QString &text, int matches, const QMap<QString, QVector<VCDSignal>> &matchingSignalsByScope);
    void onSearchFinished();
    void applySignalFilter();

    // NEW: Method to check and process RTL for port filters
    bool ensureRtlProcessedForPortFilter(const QString& filterType);

    // UI Components
    QTreeWidget *signalTree;
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QDialogButtonBox *buttonBox;
    QLineEdit *searchEdit;
    QProgressBar *progressBar;
    QLabel *statusLabel;

    // NEW: Filter buttons
    QPushButton *filterInputButton;
    QPushButton *filterOutputButton;
    QPushButton *filterInoutButton;
    QPushButton *filterNetButton;
    QPushButton *filterRegButton;
    QPushButton *filterAllButton;

    // Loading and search management
    QTimer *searchTimer;
    QString pendingSearchText;
    bool isSearchInProgress;
    bool isLoadingInProgress;
    bool isInitialLoadComplete;

    // Loading state variables
    int currentLoadIndex;
    int totalSignalsToProcess;

    // Data storage with caching
    QVector<VCDSignal> allSignals;
    QSet<QString> visibleSignalIdentifiers;
    QSet<QString> selectedSignals;

    // Scope structure with loading state
    QMap<QString, QVector<VCDSignal>> scopeSignals;
    QMap<QString, QStringList> childScopes;
    QSet<QString> populatedScopes;
    QSet<QString> loadingScopes; // Scopes currently being loaded

    // Multi-selection support
    QTreeWidgetItem *lastSelectedItem;
    QString currentFilter;
    
    // NEW: Signal type filter
    QString currentTypeFilter;

    // Async loading
    QFutureWatcher<void> *loadWatcher;
    QFuture<void> loadFuture;

    // NEW: RTL processing members
    MainWindow* mainWindow;
    QString currentVcdFilePath;
    bool* rtlProcessed;
    QString* tempVcdFilePath;

    // NEW: Store the VCD parser reference
    VCDParser* vcdParser;

    // Methods
    void startInitialLoad();
    void performInitialLoad();
    void buildScopeStructure();
    void populateTopLevelScopesLazy();
    void buildScopeStructureChunked();
    void populateScopeChildrenLazy(const QString &scopePath, QTreeWidgetItem *parentItem);
    void updateScopeCheckState(QTreeWidgetItem *scopeItem);
    void updateParentScopeCheckState(QTreeWidgetItem *childItem);
    void setScopeSignalsSelection(const QString &scopePath, bool selected);
    void updateTreeWidgetCheckStates(const QString &scopePath, bool selected);
    void handleMultiSelection(QTreeWidgetItem *item);
};

#endif // SIGNALSELECTIONDIALOG_H