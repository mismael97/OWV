#include "SignalSelectionDialog.h"
#include "mainwindow.h" // ADD THIS
#include <QHeaderView>
#include <QDebug>
#include <QApplication>
#include <QTreeWidgetItemIterator>
#include <QQueue>
#include <QCloseEvent>
#include <QShowEvent>
#include <QtConcurrent>
#include <QMessageBox> // ADD THIS

void SignalSelectionDialog::onSearchFinished()
{
    isSearchInProgress = false;
}

void SignalSelectionDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0)
        return;

    QVariant data = item->data(0, Qt::UserRole);

    if (data.canConvert<VCDSignal>())
    {
        // Signal item changed
        VCDSignal signal = data.value<VCDSignal>();
        if (item->checkState(0) == Qt::Checked)
        {
            selectedSignals.insert(signal.fullName); // CHANGE: use fullName
        }
        else
        {
            selectedSignals.remove(signal.fullName); // CHANGE: use fullName
        }

        // Update parent scope check state
        updateParentScopeCheckState(item);
    }
    else
    {
        // Scope item changed
        onScopeItemChanged(item, column);
        return;
    }

    // Update status
    statusLabel->setText(QString("%1 signal(s) selected").arg(selectedSignals.size()));
}

SignalSelectionDialog::SignalSelectionDialog(QWidget *parent)
    : QDialog(parent), lastSelectedItem(nullptr),
      isSearchInProgress(false), isLoadingInProgress(false), isInitialLoadComplete(false),
      currentLoadIndex(0), totalSignalsToProcess(0),
      currentTypeFilter("all"),  // Initialize with "all" filter
      mainWindow(nullptr),       // Initialize mainWindow
      rtlProcessed(nullptr)      // Initialize rtlProcessed pointer
{
    setWindowTitle("Add Signals to Waveform");
    setMinimumSize(800, 600);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Search bar
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel("Search:");
    searchEdit = new QLineEdit();
    searchEdit->setPlaceholderText("Type to search signals...");
    searchEdit->setClearButtonEnabled(true);

    // Setup search timer with 300ms delay
    searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(300);

    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        pendingSearchText = text;
        
        // Cancel any ongoing loading when user starts searching
        if (isLoadingInProgress && loadWatcher && loadWatcher->isRunning()) {
            loadWatcher->cancel();
            isLoadingInProgress = false;
            progressBar->setVisible(false);
        }
        
        if (text.isEmpty()) {
            searchTimer->stop();
            onSearchTextChanged(text);
        } else {
            searchTimer->start();
        }
    });

    connect(searchTimer, &QTimer::timeout, this, &SignalSelectionDialog::onSearchTimerTimeout);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit);

    // NEW: Filter buttons layout
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(2);
    
    filterInputButton = new QPushButton("Input Ports");
    filterOutputButton = new QPushButton("Output Ports");
    filterInoutButton = new QPushButton("Inout Ports");
    filterNetButton = new QPushButton("Net Signals");
    filterRegButton = new QPushButton("Registers");
    filterAllButton = new QPushButton("All Signals");
    
    // NEW: RTL directory button
    rtlDirectoryButton = new QPushButton("📁 Set RTL Directory");  // ADD THIS
    
    // Style the buttons
    QString buttonStyle = "QPushButton { padding: 6px; font-size: 11px; border: 1px solid #555; background-color: #333; color: white; }"
                         "QPushButton:checked { background-color: #4CAF50; color: white; border: 1px solid #4CAF50; }"
                         "QPushButton:hover { background-color: #555; }";
    
    filterInputButton->setStyleSheet(buttonStyle);
    filterOutputButton->setStyleSheet(buttonStyle);
    filterInoutButton->setStyleSheet(buttonStyle);
    filterNetButton->setStyleSheet(buttonStyle);
    filterRegButton->setStyleSheet(buttonStyle);
    filterAllButton->setStyleSheet(buttonStyle);
    rtlDirectoryButton->setStyleSheet(buttonStyle);  // ADD THIS
    
    // Make filter buttons checkable
    filterInputButton->setCheckable(true);
    filterOutputButton->setCheckable(true);
    filterInoutButton->setCheckable(true);
    filterNetButton->setCheckable(true);
    filterRegButton->setCheckable(true);
    filterAllButton->setCheckable(true);
    
    // Set "All Signals" as initially checked
    filterAllButton->setChecked(true);
    
    // Connect filter buttons
    connect(filterInputButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterInputPorts);
    connect(filterOutputButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterOutputPorts);
    connect(filterInoutButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterInoutPorts);
    connect(filterNetButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterNetSignals);
    connect(filterRegButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterRegisters);
    connect(filterAllButton, &QPushButton::clicked, this, &SignalSelectionDialog::onFilterAll);
    
    // NEW: Connect RTL directory button
    connect(rtlDirectoryButton, &QPushButton::clicked, this, &SignalSelectionDialog::onRtlDirectoryButtonClicked);
    
    filterLayout->addWidget(filterInputButton);
    filterLayout->addWidget(filterOutputButton);
    filterLayout->addWidget(filterInoutButton);
    filterLayout->addWidget(filterNetButton);
    filterLayout->addWidget(filterRegButton);
    filterLayout->addWidget(filterAllButton);
    filterLayout->addWidget(rtlDirectoryButton);  // ADD THIS
    filterLayout->addStretch();

    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(true);

    // Status label
    statusLabel = new QLabel("Ready");

    // Signal tree
    signalTree = new QTreeWidget();
    signalTree->setHeaderLabels({"Signal", "Width", "Type", "Identifier"});
    signalTree->setAlternatingRowColors(true);
    signalTree->header()->setStretchLastSection(false);
    signalTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    signalTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    signalTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    signalTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    signalTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

    // Connect signals for lazy loading and selection
    connect(signalTree, &QTreeWidget::itemExpanded, this, &SignalSelectionDialog::onItemExpanded);
    connect(signalTree, &QTreeWidget::itemChanged, this, &SignalSelectionDialog::onItemChanged);
    connect(signalTree, &QTreeWidget::itemClicked, this, &SignalSelectionDialog::onItemClicked);

    // Controls
    QHBoxLayout *controlsLayout = new QHBoxLayout();
    selectAllButton = new QPushButton("Select All");
    deselectAllButton = new QPushButton("Deselect All");

    connect(selectAllButton, &QPushButton::clicked, this, &SignalSelectionDialog::selectAll);
    connect(deselectAllButton, &QPushButton::clicked, this, &SignalSelectionDialog::deselectAll);

    controlsLayout->addWidget(selectAllButton);
    controlsLayout->addWidget(deselectAllButton);
    controlsLayout->addStretch();

    // Buttons
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    mainLayout->addLayout(searchLayout);
    mainLayout->addLayout(filterLayout); // NEW: Add filter buttons
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(signalTree, 1);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(buttonBox);

    // Initialize load watcher
    loadWatcher = new QFutureWatcher<void>(this);
    connect(loadWatcher, &QFutureWatcher<void>::finished, this, &SignalSelectionDialog::onLoadFinished);
}

void SignalSelectionDialog::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);

    // Only handle clicks on signal items (not scope items)
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<VCDSignal>())
    {
        handleMultiSelection(item);
    }
}

void SignalSelectionDialog::handleMultiSelection(QTreeWidgetItem *item)
{
    if (!item)
        return;

    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<VCDSignal>())
        return;

    VCDSignal signal = data.value<VCDSignal>();
    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();

    // Block signals to prevent recursive calls during bulk operations
    signalTree->blockSignals(true);

    if (modifiers & Qt::ShiftModifier && lastSelectedItem)
    {
        // Shift+click: select range from last selected to current item

        // Get all signal items in the tree
        QList<QTreeWidgetItem *> allSignalItems;
        QTreeWidgetItemIterator it(signalTree);
        while (*it)
        {
            QVariant itData = (*it)->data(0, Qt::UserRole);
            if (itData.canConvert<VCDSignal>())
            {
                allSignalItems.append(*it);
            }
            ++it;
        }

        // Find indices of last selected and current items
        int startIndex = allSignalItems.indexOf(lastSelectedItem);
        int endIndex = allSignalItems.indexOf(item);

        if (startIndex != -1 && endIndex != -1)
        {
            int low = qMin(startIndex, endIndex);
            int high = qMax(startIndex, endIndex);

            // Select all items in the range
            for (int i = low; i <= high; i++)
            {
                if (i < allSignalItems.size())
                {
                    QTreeWidgetItem *rangeItem = allSignalItems[i];
                    VCDSignal rangeSignal = rangeItem->data(0, Qt::UserRole).value<VCDSignal>();
                    selectedSignals.insert(rangeSignal.fullName); // CHANGE: use fullName
                    rangeItem->setCheckState(0, Qt::Checked);
                }
            }
        }
    }
    else if (modifiers & Qt::ControlModifier)
    {
        // Ctrl+click: toggle selection of current item
        if (selectedSignals.contains(signal.fullName)) // CHANGE: use fullName
        {
            selectedSignals.remove(signal.fullName); // CHANGE: use fullName
            item->setCheckState(0, Qt::Unchecked);
        }
        else
        {
            selectedSignals.insert(signal.fullName); // CHANGE: use fullName
            item->setCheckState(0, Qt::Checked);
        }
        lastSelectedItem = item;
    }
    else
    {
        // Regular click: single selection (clear others and select this one)
        // First, clear all selections
        QTreeWidgetItemIterator clearIt(signalTree);
        while (*clearIt)
        {
            QVariant clearData = (*clearIt)->data(0, Qt::UserRole);
            if (clearData.canConvert<VCDSignal>())
            {
                VCDSignal clearSignal = clearData.value<VCDSignal>();
                selectedSignals.remove(clearSignal.fullName); // CHANGE: use fullName
                (*clearIt)->setCheckState(0, Qt::Unchecked);
            }
            ++clearIt;
        }

        // Then select the clicked item
        selectedSignals.insert(signal.fullName); // CHANGE: use fullName
        item->setCheckState(0, Qt::Checked);
        lastSelectedItem = item;
    }

    // Unblock signals
    signalTree->blockSignals(false);

    // Update status
    statusLabel->setText(QString("%1 signal(s) selected").arg(selectedSignals.size()));
}

void SignalSelectionDialog::setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals)
{
    this->allSignals = allSignals;
    selectedSignals.clear();
    scopeSignals.clear();
    childScopes.clear();
    populatedScopes.clear();
    lastSelectedItem = nullptr;

    signalTree->clear();

    // Create a set of visible signal fullNames
    visibleSignalIdentifiers.clear();
    for (const auto &signal : visibleSignals)
    {
        visibleSignalIdentifiers.insert(signal.fullName);
    }

    statusLabel->setText(QString("Ready to load %1 signals").arg(allSignals.size()));
}

void SignalSelectionDialog::processNextChunk()
{
    if (!isLoadingInProgress || currentLoadIndex >= allSignals.size())
    {
        // Loading complete
        onLoadFinished();
        return;
    }

    const int CHUNK_SIZE = 500; // Process 500 signals per chunk
    int endIndex = qMin(currentLoadIndex + CHUNK_SIZE, allSignals.size());

    for (int i = currentLoadIndex; i < endIndex; i++)
    {
        const auto &signal = allSignals[i];

        // Skip signals that are already visible
        if (visibleSignalIdentifiers.contains(signal.fullName))
        {
            continue;
        }

        QString scopePath = signal.scope;

        // Add signal to its scope
        scopeSignals[scopePath].append(signal);

        // Build parent-child scope relationships
        if (!scopePath.isEmpty())
        {
            QStringList scopeParts = scopePath.split('.');
            QString currentPath;

            for (int j = 0; j < scopeParts.size(); j++)
            {
                if (!currentPath.isEmpty())
                    currentPath += ".";
                currentPath += scopeParts[j];

                if (j < scopeParts.size() - 1)
                {
                    QString parentPath = currentPath;
                    QString childName = scopeParts[j + 1];
                    QString childPath = parentPath + "." + childName;

                    if (!childScopes[parentPath].contains(childPath))
                    {
                        childScopes[parentPath].append(childPath);
                    }
                }
            }
        }
    }

    currentLoadIndex = endIndex;

    // Update progress
    int progress = (currentLoadIndex * 100) / allSignals.size();
    progressBar->setValue(progress);
    statusLabel->setText(QString("Building scope structure... %1% (%2/%3 signals)").arg(progress).arg(currentLoadIndex).arg(allSignals.size()));

    // Force UI update
    QApplication::processEvents();

    // Process next chunk after a brief delay to keep UI responsive
    QTimer::singleShot(1, this, &SignalSelectionDialog::processNextChunk);
}

void SignalSelectionDialog::startInitialLoad()
{
    if (isLoadingInProgress || isInitialLoadComplete)
    {
        return;
    }

    isLoadingInProgress = true;
    currentLoadIndex = 0;
    totalSignalsToProcess = allSignals.size();

    statusLabel->setText("Building scope structure...");
    progressBar->setVisible(true);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    // Process in chunks using a timer to avoid freezing
    QTimer::singleShot(0, this, &SignalSelectionDialog::processNextChunk);
}

void SignalSelectionDialog::onLoadProgress(int percentage)
{
    if (!isLoadingInProgress)
        return;

    progressBar->setValue(percentage);
    statusLabel->setText(QString("Building scope structure... %1%").arg(percentage));
}

void SignalSelectionDialog::onLoadFinished()
{
    isLoadingInProgress = false;
    isInitialLoadComplete = true;

    progressBar->setVisible(false);

    // Apply the current filter instead of just populating top-level scopes
    applySignalFilter();

    statusLabel->setText(QString("Ready - %1 signals in %2 scopes")
                             .arg(allSignals.size())
                             .arg(scopeSignals.size()));
}

void SignalSelectionDialog::populateTopLevelScopesLazy()
{
    signalTree->setUpdatesEnabled(false);

    // Add global signals (signals with no scope)
    if (scopeSignals.contains("") && !scopeSignals[""].isEmpty())
    {
        QTreeWidgetItem *globalItem = new QTreeWidgetItem();
        globalItem->setText(0, "Global Signals");
        globalItem->setData(0, Qt::UserRole, "");
        globalItem->setFlags(globalItem->flags() | Qt::ItemIsUserCheckable);
        signalTree->addTopLevelItem(globalItem);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        globalItem->addChild(placeholder);

        populatedScopes.insert("");
        updateScopeCheckState(globalItem);
    }

    // Find top-level scopes
    QSet<QString> topLevelScopes;
    for (const QString &scope : scopeSignals.keys())
    {
        if (!scope.isEmpty() && !scopeSignals[scope].isEmpty())
        {
            bool isTopLevel = true;
            for (const QString &potentialParent : scopeSignals.keys())
            {
                if (scope.startsWith(potentialParent + ".") && scope != potentialParent)
                {
                    isTopLevel = false;
                    break;
                }
            }
            if (isTopLevel)
            {
                topLevelScopes.insert(scope);
            }
        }
    }

    // Create tree items for top-level scopes
    for (const QString &scope : topLevelScopes)
    {
        QString displayName = scope;
        QStringList parts = scope.split('.');
        if (!parts.isEmpty())
        {
            displayName = parts.last();
        }

        QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
        scopeItem->setText(0, displayName);
        scopeItem->setToolTip(0, scope);
        scopeItem->setData(0, Qt::UserRole, scope);
        scopeItem->setFlags(scopeItem->flags() | Qt::ItemIsUserCheckable);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        scopeItem->addChild(placeholder);

        signalTree->addTopLevelItem(scopeItem);
        populatedScopes.insert(scope);
        updateScopeCheckState(scopeItem);
    }

    signalTree->setUpdatesEnabled(true);
}

void SignalSelectionDialog::populateTopLevelScopes()
{
    signalTree->setUpdatesEnabled(false);
    signalTree->clear();

    // Add global signals (signals with no scope)
    if (scopeSignals.contains("") && !scopeSignals[""].isEmpty())
    {
        QTreeWidgetItem *globalItem = new QTreeWidgetItem();
        globalItem->setText(0, "Global Signals");
        globalItem->setData(0, Qt::UserRole, "");
        globalItem->setFlags(globalItem->flags() | Qt::ItemIsUserCheckable);
        signalTree->addTopLevelItem(globalItem);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        globalItem->addChild(placeholder);

        populatedScopes.insert("");
        updateScopeCheckState(globalItem);

        // NEW: Expand the global signals item
        globalItem->setExpanded(true);
    }

    // Find top-level scopes
    QSet<QString> topLevelScopes;
    for (const QString &scope : scopeSignals.keys())
    {
        if (!scope.isEmpty() && !scopeSignals[scope].isEmpty())
        {
            bool isTopLevel = true;
            for (const QString &potentialParent : scopeSignals.keys())
            {
                if (scope.startsWith(potentialParent + ".") && scope != potentialParent)
                {
                    isTopLevel = false;
                    break;
                }
            }
            if (isTopLevel)
            {
                topLevelScopes.insert(scope);
            }
        }
    }

    // Create tree items for top-level scopes
    for (const QString &scope : topLevelScopes)
    {
        QString displayName = scope;
        QStringList parts = scope.split('.');
        if (!parts.isEmpty())
        {
            displayName = parts.last();
        }

        QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
        scopeItem->setText(0, displayName);
        scopeItem->setToolTip(0, scope);
        scopeItem->setData(0, Qt::UserRole, scope);
        scopeItem->setFlags(scopeItem->flags() | Qt::ItemIsUserCheckable);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        scopeItem->addChild(placeholder);

        signalTree->addTopLevelItem(scopeItem);
        populatedScopes.insert(scope);
        updateScopeCheckState(scopeItem);

        // NEW: Expand all top-level scope items
        scopeItem->setExpanded(true);
    }

    signalTree->setUpdatesEnabled(true);
}

void SignalSelectionDialog::onItemExpanded(QTreeWidgetItem *item)
{
    QString scopePath = item->data(0, Qt::UserRole).toString();
    if (scopePath != "PLACEHOLDER")
    {
        populateScopeChildren(scopePath, item);
    }
}

QList<VCDSignal> SignalSelectionDialog::getSelectedSignals() const
{
    QList<VCDSignal> result;

    for (const QString &fullName : selectedSignals) // CHANGE: use fullName
    {
        // Find the signal in allSignals
        for (const VCDSignal &signal : allSignals)
        {
            if (signal.fullName == fullName) // CHANGE: use fullName
            {
                result.append(signal);
                break;
            }
        }
    }

    return result;
}

void SignalSelectionDialog::selectAll()
{
    signalTree->blockSignals(true);

    // NEW: Only select currently displayed signals (filtered + searched)
    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);

        if (data.canConvert<VCDSignal>())
        {
            VCDSignal signal = data.value<VCDSignal>();
            // Only select if it's not already visible in waveform
            if (!visibleSignalIdentifiers.contains(signal.fullName))
            {
                selectedSignals.insert(signal.fullName);
                item->setCheckState(0, Qt::Checked);
            }
        }
        else if (data.toString() != "PLACEHOLDER" && !data.toString().isEmpty())
        {
            // Scope item - check it and update its state
            item->setCheckState(0, Qt::Checked);
            updateScopeCheckState(item);
        }
        ++it;
    }

    signalTree->blockSignals(false);
    
    // Update status with only the newly selected signals from current view
    int displayedSelectedCount = 0;
    QTreeWidgetItemIterator countIt(signalTree);
    while (*countIt)
    {
        QTreeWidgetItem *item = *countIt;
        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>())
        {
            VCDSignal signal = data.value<VCDSignal>();
            if (selectedSignals.contains(signal.fullName) && !visibleSignalIdentifiers.contains(signal.fullName))
            {
                displayedSelectedCount++;
            }
        }
        ++countIt;
    }
    
    statusLabel->setText(QString("%1 signal(s) selected from current view").arg(displayedSelectedCount));
}

void SignalSelectionDialog::deselectAll()
{
    signalTree->blockSignals(true);

    // NEW: Only deselect currently displayed signals (filtered + searched)
    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);

        if (data.canConvert<VCDSignal>())
        {
            VCDSignal signal = data.value<VCDSignal>();
            selectedSignals.remove(signal.fullName);
            item->setCheckState(0, Qt::Unchecked);
        }
        else if (data.toString() != "PLACEHOLDER" && !data.toString().isEmpty())
        {
            // Scope item - uncheck it
            item->setCheckState(0, Qt::Unchecked);
        }
        ++it;
    }

    lastSelectedItem = nullptr;
    signalTree->blockSignals(false);
    
    // Count how many signals were deselected from current view
    int totalDisplayedSignals = 0;
    QTreeWidgetItemIterator countIt(signalTree);
    while (*countIt)
    {
        QTreeWidgetItem *item = *countIt;
        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>())
        {
            totalDisplayedSignals++;
        }
        ++countIt;
    }
    
    statusLabel->setText(QString("Deselected all %1 signals from current view").arg(totalDisplayedSignals));
}

void SignalSelectionDialog::displaySearchResults(const QString &text, int matches, const QMap<QString, QVector<VCDSignal>> &matchingSignalsByScope)
{
    if (text != currentFilter)
    {
        return; // Stale results
    }

    signalTree->setUpdatesEnabled(false);
    signalTree->blockSignals(true);
    signalTree->clear();

    if (!matchingSignalsByScope.isEmpty())
    {
        for (auto it = matchingSignalsByScope.begin(); it != matchingSignalsByScope.end(); ++it)
        {
            QString scopePath = it.key();
            QVector<VCDSignal> signalsInScope = it.value();

            QTreeWidgetItem *scopeItem;
            if (scopePath.isEmpty())
            {
                scopeItem = new QTreeWidgetItem(signalTree);
                scopeItem->setText(0, "Global Signals");
            }
            else
            {
                scopeItem = new QTreeWidgetItem(signalTree);
                scopeItem->setText(0, scopePath);
            }

            scopeItem->setFlags(scopeItem->flags() | Qt::ItemIsUserCheckable);
            scopeItem->setData(0, Qt::UserRole, scopePath);
            updateScopeCheckState(scopeItem);

            for (const VCDSignal &signal : signalsInScope)
            {
                QTreeWidgetItem *signalItem = new QTreeWidgetItem(scopeItem);
                signalItem->setText(0, signal.name);
                signalItem->setText(1, QString::number(signal.width));
                signalItem->setText(2, signal.type);
                signalItem->setText(3, signal.identifier);
                signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
                signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);

                if (selectedSignals.contains(signal.fullName))
                {
                    signalItem->setCheckState(0, Qt::Checked);
                }
                else
                {
                    signalItem->setCheckState(0, Qt::Unchecked);
                }
            }
            scopeItem->setExpanded(true);
        }
    }
    else
    {
        QTreeWidgetItem *noResultsItem = new QTreeWidgetItem(signalTree);
        noResultsItem->setText(0, "No signals found matching: " + text);
        noResultsItem->setFlags(noResultsItem->flags() & ~Qt::ItemIsSelectable);
    }

    signalTree->blockSignals(false);
    signalTree->setUpdatesEnabled(true);

    // Updated status message to indicate search mode
    if (matches > 0)
    {
        statusLabel->setText(QString("Found %1 signals matching '%2' - Use Select All/Deselect All for displayed signals only")
                                 .arg(matches)
                                 .arg(text));
    }
    else
    {
        statusLabel->setText(QString("No signals found matching '%1'").arg(text));
    }
}

void SignalSelectionDialog::performSearch(const QString &text)
{
    isSearchInProgress = true;

    QString searchLower = text.toLower();
    QMap<QString, QVector<VCDSignal>> matchingSignalsByScope;
    int matches = 0;
    int processed = 0;

    const int CHUNK_SIZE = 500; // Process 500 signals at a time

    for (const auto &signal : allSignals)
    {
        if (visibleSignalIdentifiers.contains(signal.fullName))
        {
            processed++;
            continue;
        }

        QString signalPath = (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name).toLower();
        if (signalPath.contains(searchLower))
        {
            matchingSignalsByScope[signal.scope].append(signal);
            matches++;
        }

        processed++;

        // Process events every CHUNK_SIZE to keep UI responsive
        if (processed % CHUNK_SIZE == 0)
        {
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

            // Check if search was cancelled (new text entered)
            if (text != currentFilter)
            {
                isSearchInProgress = false;
                return;
            }
        }
    }

    // Display results
    displaySearchResults(text, matches, matchingSignalsByScope);
    isSearchInProgress = false;

    // Check if there's a pending search
    if (!pendingSearchText.isNull() && pendingSearchText != text)
    {
        QTimer::singleShot(0, this, [this]()
                           { onSearchTextChanged(pendingSearchText); });
    }
}

void SignalSelectionDialog::onSearchTextChanged(const QString &text)
{
    if (isSearchInProgress)
    {
        // If a search is already running, we'll let it finish
        // The new search will be handled when the current one completes
        pendingSearchText = text;
        return;
    }

    currentFilter = text;

    // For immediate feedback on empty search
    if (text.isEmpty())
    {
        applySignalFilter(); // Use the new filter function
        return;
    }

    // Show searching status
    statusLabel->setText("Searching...");

    QApplication::processEvents(); // Update UI

    // Perform search in main thread but with chunked processing
    performSearch(text);
}

SignalSelectionDialog::~SignalSelectionDialog()
{
    if (searchTimer && searchTimer->isActive())
    {
        searchTimer->stop();
    }
    if (loadWatcher && loadWatcher->isRunning())
    {
        loadWatcher->cancel();
        loadWatcher->waitForFinished();
    }
}

void SignalSelectionDialog::closeEvent(QCloseEvent *event)
{
    if (searchTimer && searchTimer->isActive())
    {
        searchTimer->stop();
    }
    if (loadWatcher && loadWatcher->isRunning())
    {
        loadWatcher->cancel();
        loadWatcher->waitForFinished();
    }
    QDialog::closeEvent(event);
}

void SignalSelectionDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    // Start initial loading when dialog is shown
    if (!isInitialLoadComplete && !isLoadingInProgress && !allSignals.isEmpty())
    {
        startInitialLoad();
    }
}

void SignalSelectionDialog::onSearchTimerTimeout()
{
    if (!pendingSearchText.isNull())
    {
        onSearchTextChanged(pendingSearchText);
    }
}

void SignalSelectionDialog::onScopeItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0)
        return;
    if (!item)
        return;

    QVariant data = item->data(0, Qt::UserRole);
    QString scopePath = data.toString();

    // Only process scope items (not signal items and not placeholders)
    if (scopePath == "PLACEHOLDER" || data.canConvert<VCDSignal>())
        return;

    // Block signals to prevent recursive calls
    signalTree->blockSignals(true);

    bool isChecked = (item->checkState(0) == Qt::Checked);

    // Select/deselect all signals in this scope and all sub-scopes
    setScopeSignalsSelection(scopePath, isChecked);

    // Update the visual check states in the tree for this scope and all children
    updateTreeWidgetCheckStates(scopePath, isChecked);

    // Update parent scopes' check states
    updateParentScopeCheckState(item);

    signalTree->blockSignals(false);

    // Update status
    statusLabel->setText(QString("%1 signal(s) selected").arg(selectedSignals.size()));
}

void SignalSelectionDialog::updateTreeWidgetCheckStates(const QString &scopePath, bool selected)
{
    // Recursively update all items in the tree for this scope and its children
    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);

        if (data.canConvert<VCDSignal>())
        {
            // Signal item - update if it belongs to this scope or sub-scope
            VCDSignal signal = data.value<VCDSignal>();
            if (signal.scope == scopePath || signal.scope.startsWith(scopePath + "."))
            {
                item->setCheckState(0, selected ? Qt::Checked : Qt::Unchecked);
            }
        }
        else
        {
            // Scope item - update if it's this scope or a sub-scope
            QString itemScope = data.toString();
            if ((itemScope == scopePath || itemScope.startsWith(scopePath + ".")) && itemScope != "PLACEHOLDER")
            {
                item->setCheckState(0, selected ? Qt::Checked : Qt::Unchecked);
            }
        }
        ++it;
    }
}

void SignalSelectionDialog::setScopeSignalsSelection(const QString &scopePath, bool selected)
{
    // Process current scope signals
    if (scopeSignals.contains(scopePath))
    {
        for (const VCDSignal &signal : scopeSignals[scopePath])
        {
            if (selected)
            {
                selectedSignals.insert(signal.fullName); // CHANGE: use fullName
            }
            else
            {
                selectedSignals.remove(signal.fullName); // CHANGE: use fullName
            }
        }
    }

    // Process child scopes recursively
    if (childScopes.contains(scopePath))
    {
        for (const QString &childScope : childScopes[scopePath])
        {
            setScopeSignalsSelection(childScope, selected);
        }
    }
}

void SignalSelectionDialog::updateScopeCheckState(QTreeWidgetItem *scopeItem)
{
    if (!scopeItem)
        return;

    QString scopePath = scopeItem->data(0, Qt::UserRole).toString();
    if (scopePath == "PLACEHOLDER")
        return;

    // Count ALL selected signals in this scope and ALL sub-scopes
    int totalSignals = 0;
    int selectedSignalsCount = 0;

    // Count signals in current scope
    if (scopeSignals.contains(scopePath))
    {
        totalSignals += scopeSignals[scopePath].size();
        for (const VCDSignal &signal : scopeSignals[scopePath])
        {
            if (selectedSignals.contains(signal.fullName)) // CHANGE: use fullName
            {
                selectedSignalsCount++;
            }
        }
    }

    // Count signals in all child scopes recursively using QList
    QSet<QString> processedScopes;
    QList<QString> scopesToProcess;

    if (childScopes.contains(scopePath))
    {
        for (const QString &childScope : childScopes[scopePath])
        {
            scopesToProcess.append(childScope);
        }
    }

    while (!scopesToProcess.isEmpty())
    {
        QString currentScope = scopesToProcess.takeFirst();
        if (processedScopes.contains(currentScope))
            continue;

        processedScopes.insert(currentScope);

        // Count signals in this child scope
        if (scopeSignals.contains(currentScope))
        {
            totalSignals += scopeSignals[currentScope].size();
            for (const VCDSignal &signal : scopeSignals[currentScope])
            {
                if (selectedSignals.contains(signal.fullName)) // CHANGE: use fullName
                {
                    selectedSignalsCount++;
                }
            }
        }

        // Add child scopes of this scope to the list
        if (childScopes.contains(currentScope))
        {
            for (const QString &childScope : childScopes[currentScope])
            {
                if (!processedScopes.contains(childScope))
                {
                    scopesToProcess.append(childScope);
                }
            }
        }
    }

    // Set check state based on selection
    if (totalSignals == 0)
    {
        scopeItem->setCheckState(0, Qt::Unchecked);
    }
    else if (selectedSignalsCount == 0)
    {
        scopeItem->setCheckState(0, Qt::Unchecked);
    }
    else if (selectedSignalsCount == totalSignals)
    {
        scopeItem->setCheckState(0, Qt::Checked);
    }
    else
    {
        scopeItem->setCheckState(0, Qt::PartiallyChecked);
    }
}

void SignalSelectionDialog::updateParentScopeCheckState(QTreeWidgetItem *childItem)
{
    if (!childItem)
        return;

    QTreeWidgetItem *parent = childItem->parent();
    if (!parent)
        return;

    // Update the parent's check state
    updateScopeCheckState(parent);

    // Recursively update grandparents
    updateParentScopeCheckState(parent);
}

void SignalSelectionDialog::populateScopeChildren(const QString &scopePath, QTreeWidgetItem *parentItem)
{
    if (!parentItem || populatedScopes.contains(scopePath + "_POPULATED"))
    {
        return;
    }

    // Remove placeholder
    while (parentItem->childCount() > 0)
    {
        QTreeWidgetItem *child = parentItem->child(0);
        if (child->data(0, Qt::UserRole).toString() == "PLACEHOLDER")
        {
            delete child;
            break;
        }
    }

    // Make parent item checkable if it's a scope
    parentItem->setFlags(parentItem->flags() | Qt::ItemIsUserCheckable);

    // Update initial check state for scope
    updateScopeCheckState(parentItem);

    // Add child scopes
    if (childScopes.contains(scopePath))
    {
        for (const QString &childScopePath : childScopes[scopePath])
        {
            QString displayName = childScopePath;
            // Extract just the last part for display
            QStringList parts = childScopePath.split('.');
            if (!parts.isEmpty())
            {
                displayName = parts.last();
            }

            QTreeWidgetItem *childScopeItem = new QTreeWidgetItem();
            childScopeItem->setText(0, displayName);
            childScopeItem->setToolTip(0, childScopePath); // Show full path
            childScopeItem->setData(0, Qt::UserRole, childScopePath);
            childScopeItem->setFlags(childScopeItem->flags() | Qt::ItemIsUserCheckable);

            // Set initial check state
            updateScopeCheckState(childScopeItem);

            // Add placeholder
            QTreeWidgetItem *placeholder = new QTreeWidgetItem();
            placeholder->setText(0, "Loading...");
            placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
            childScopeItem->addChild(placeholder);

            parentItem->addChild(childScopeItem);
        }
    }

    // Add signals in this scope
    if (scopeSignals.contains(scopePath))
    {
        for (const VCDSignal &signal : scopeSignals[scopePath])
        {
            // Apply filter if active
            if (!currentFilter.isEmpty())
            {
                QString signalPath = (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name).toLower();
                if (!signalPath.contains(currentFilter.toLower()))
                {
                    continue;
                }
            }

            QTreeWidgetItem *signalItem = new QTreeWidgetItem();
            signalItem->setText(0, signal.name);
            signalItem->setText(1, QString::number(signal.width));
            signalItem->setText(2, signal.type);
            signalItem->setText(3, signal.identifier);
            signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
            signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);

            if (selectedSignals.contains(signal.fullName))
            {
                signalItem->setCheckState(0, Qt::Checked);
            }
            else
            {
                signalItem->setCheckState(0, Qt::Unchecked);
            }

            parentItem->addChild(signalItem);
        }
    }

    populatedScopes.insert(scopePath + "_POPULATED");
}


void SignalSelectionDialog::onFilterInputPorts()
{
    // NEW: Ensure RTL is processed before applying port filter
    if (!ensureRtlProcessedForPortFilter("input")) {
        qDebug() << "RTL processing failed or cancelled for input ports";
        return; // RTL processing failed or was cancelled
    }
    
    // Double-check that RTL processing actually happened if we're filtering ports
    if (currentTypeFilter == "input" && !(*rtlProcessed)) {
        qDebug() << "RTL processing not completed, using original signals";
    }
    
    currentTypeFilter = "input";
    filterInputButton->setChecked(true);
    filterOutputButton->setChecked(false);
    filterInoutButton->setChecked(false);
    filterNetButton->setChecked(false);
    filterRegButton->setChecked(false);
    filterAllButton->setChecked(false);
    applySignalFilter();
}

void SignalSelectionDialog::onFilterOutputPorts()
{
    // NEW: Ensure RTL is processed before applying port filter
    if (!ensureRtlProcessedForPortFilter("output")) {
        return; // RTL processing failed or was cancelled
    }
    
    currentTypeFilter = "output";
    filterInputButton->setChecked(false);
    filterOutputButton->setChecked(true);
    filterInoutButton->setChecked(false);
    filterNetButton->setChecked(false);
    filterRegButton->setChecked(false);
    filterAllButton->setChecked(false);
    applySignalFilter();
}

void SignalSelectionDialog::onFilterInoutPorts()
{
    // NEW: Ensure RTL is processed before applying port filter
    if (!ensureRtlProcessedForPortFilter("inout")) {
        return; // RTL processing failed or was cancelled
    }
    
    currentTypeFilter = "inout";
    filterInputButton->setChecked(false);
    filterOutputButton->setChecked(false);
    filterInoutButton->setChecked(true);
    filterNetButton->setChecked(false);
    filterRegButton->setChecked(false);
    filterAllButton->setChecked(false);
    applySignalFilter();
}
void SignalSelectionDialog::onFilterNetSignals()
{
    currentTypeFilter = "wire";
    filterInputButton->setChecked(false);
    filterOutputButton->setChecked(false);
    filterInoutButton->setChecked(false);
    filterNetButton->setChecked(true);
    filterRegButton->setChecked(false);
    filterAllButton->setChecked(false);
    applySignalFilter();
}

void SignalSelectionDialog::onFilterRegisters()
{
    currentTypeFilter = "reg";
    filterInputButton->setChecked(false);
    filterOutputButton->setChecked(false);
    filterInoutButton->setChecked(false);
    filterNetButton->setChecked(false);
    filterRegButton->setChecked(true);
    filterAllButton->setChecked(false);
    applySignalFilter();
}

void SignalSelectionDialog::onFilterAll()
{
    currentTypeFilter = "all";
    filterInputButton->setChecked(false);
    filterOutputButton->setChecked(false);
    filterInoutButton->setChecked(false);
    filterNetButton->setChecked(false);
    filterRegButton->setChecked(false);
    filterAllButton->setChecked(true);
    applySignalFilter();
}

void SignalSelectionDialog::applySignalFilter()
{
    if (!isInitialLoadComplete) {
        return; // Wait until initial load is complete
    }

    signalTree->setUpdatesEnabled(false);
    signalTree->blockSignals(true);
    signalTree->clear();

    // NEW: Use RTL-processed signals if available
    QVector<VCDSignal> signalsToUse = allSignals;
    VCDParser* tempRtlParser = nullptr;
    
    if (*rtlProcessed && !tempVcdFilePath->isEmpty() && QFile::exists(*tempVcdFilePath)) {
        // Create a temporary parser for the RTL-processed VCD file
        tempRtlParser = new VCDParser(this);
        if (tempRtlParser->parseHeaderOnly(*tempVcdFilePath)) {
            // Use the RTL-processed signals
            signalsToUse = tempRtlParser->getSignals();
            qDebug() << "Using RTL-processed signals, total:" << signalsToUse.size();
            
            // Debug: Print some signal types to verify RTL processing worked
            int inputCount = 0, outputCount = 0, inoutCount = 0;
            for (const auto& signal : signalsToUse) {
                if (signal.type == "input") inputCount++;
                else if (signal.type == "output") outputCount++;
                else if (signal.type == "inout") inoutCount++;
            }
            qDebug() << "Signal types - Input:" << inputCount << "Output:" << outputCount << "Inout:" << inoutCount;
        } else {
            qDebug() << "Failed to parse RTL-processed VCD file";
            delete tempRtlParser;
            tempRtlParser = nullptr;
        }
    }

    // Build a map of signals by scope, filtered by type
    QMap<QString, QVector<VCDSignal>> filteredSignalsByScope;

    for (const auto &signal : signalsToUse) {
        // Skip signals that are already visible
        if (visibleSignalIdentifiers.contains(signal.fullName)) {
            continue;
        }

        // Apply type filter
        if (currentTypeFilter != "all" && signal.type.toLower() != currentTypeFilter) {
            continue;
        }

        // Apply search filter if active
        if (!currentFilter.isEmpty()) {
            QString signalPath = (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name).toLower();
            if (!signalPath.contains(currentFilter.toLower())) {
                continue;
            }
        }

        filteredSignalsByScope[signal.scope].append(signal);
    }

    // Populate the tree with filtered signals
    if (!filteredSignalsByScope.isEmpty()) {
        for (auto it = filteredSignalsByScope.begin(); it != filteredSignalsByScope.end(); ++it) {
            QString scopePath = it.key();
            QVector<VCDSignal> signalsInScope = it.value();

            QTreeWidgetItem *scopeItem;
            if (scopePath.isEmpty()) {
                scopeItem = new QTreeWidgetItem(signalTree);
                scopeItem->setText(0, "Global Signals");
            } else {
                scopeItem = new QTreeWidgetItem(signalTree);
                scopeItem->setText(0, scopePath);
            }

            scopeItem->setFlags(scopeItem->flags() | Qt::ItemIsUserCheckable);
            scopeItem->setData(0, Qt::UserRole, scopePath);
            updateScopeCheckState(scopeItem);

            for (const VCDSignal &signal : signalsInScope) {
                QTreeWidgetItem *signalItem = new QTreeWidgetItem(scopeItem);
                signalItem->setText(0, signal.name);
                signalItem->setText(1, QString::number(signal.width));
                signalItem->setText(2, signal.type);
                signalItem->setText(3, signal.identifier);
                signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
                signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);

                if (selectedSignals.contains(signal.fullName)) {
                    signalItem->setCheckState(0, Qt::Checked);
                } else {
                    signalItem->setCheckState(0, Qt::Unchecked);
                }
            }
            scopeItem->setExpanded(true);
        }
    } else {
        QTreeWidgetItem *noResultsItem = new QTreeWidgetItem(signalTree);
        noResultsItem->setText(0, QString("No %1 signals found").arg(currentTypeFilter == "all" ? "" : currentTypeFilter + " "));
        noResultsItem->setFlags(noResultsItem->flags() & ~Qt::ItemIsSelectable);
    }

    signalTree->blockSignals(false);
    signalTree->setUpdatesEnabled(true);

    // Update status
    int totalFilteredSignals = 0;
    for (const auto &signalList : filteredSignalsByScope) {
        totalFilteredSignals += signalList.size();
    }
    
    if (currentTypeFilter == "all") {
        statusLabel->setText(QString("Showing %1 signals").arg(totalFilteredSignals));
    } else {
        statusLabel->setText(QString("Showing %1 %2 signals").arg(totalFilteredSignals).arg(currentTypeFilter));
    }

    // Clean up the temporary parser if we created one
    if (tempRtlParser) {
        delete tempRtlParser;
    }
}

void SignalSelectionDialog::setRtlProcessingInfo(MainWindow* mainWindow, const QString& vcdFilePath, bool& rtlProcessed, QString& tempVcdPath)
{
    this->mainWindow = mainWindow;
    this->currentVcdFilePath = vcdFilePath;
    this->rtlProcessed = &rtlProcessed;
    this->tempVcdFilePath = &tempVcdPath;
}

bool MainWindow::hasRtlDirectoryForSignalDialog()
{
    QString rtlDir = findRtlDirectoryForSignalDialog(currentVcdFilePath);
    bool found = !rtlDir.isEmpty();
    qDebug() << "RTL directory check:" << (found ? "Found" : "Not found");
    if (found) {
        qDebug() << "RTL directory path:" << rtlDir;
    }
    return found;
}

bool SignalSelectionDialog::ensureRtlProcessedForPortFilter(const QString& filterType)
{
    if (filterType != "input" && filterType != "output" && filterType != "inout") {
        return true; // No RTL needed for non-port filters
    }
    
    if (*rtlProcessed) {
        qDebug() << "RTL already processed for signal dialog, using existing temp file";
        
        // Verify the temp file still exists
        if (!tempVcdFilePath->isEmpty() && QFile::exists(*tempVcdFilePath)) {
            return true; // Already processed and file exists
        } else {
            qDebug() << "Temp file missing, reprocessing RTL";
            *rtlProcessed = false; // Reset flag if file is missing
        }
    }
    
    // Check if RTL directory exists (automatic discovery)
    if (mainWindow->hasRtlDirectoryForSignalDialog()) {
        // Show processing status
        statusLabel->setText("Processing RTL files for port information...");
        QApplication::processEvents();
        
        bool success = mainWindow->processVcdWithRtlForSignalDialog(currentVcdFilePath);
        
        if (success) {
            *rtlProcessed = true;
            statusLabel->setText("RTL processing completed");
            qDebug() << "RTL processing successful, temp file:" << *tempVcdFilePath;
            
            // Verify the temp file exists and has content
            if (QFile::exists(*tempVcdFilePath)) {
                QFile file(*tempVcdFilePath);
                if (file.open(QIODevice::ReadOnly)) {
                    qDebug() << "Temp file size:" << file.size() << "bytes";
                    file.close();
                }
            }
            return true;
        } else {
            // Show warning about RTL mismatch
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("RTL Module Mismatch");
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText("RTL directory found, but no matching modules were detected.\n\n"
                          "The RTL files don't contain the modules found in the VCD file.\n\n"
                          "Would you like to specify a different RTL directory?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Ignore);
            msgBox.setDefaultButton(QMessageBox::Yes);
            
            int result = msgBox.exec();
            if (result == QMessageBox::Yes) {
                // NEW: Clean up any existing temp file before manual selection
                if (!tempVcdFilePath->isEmpty() && QFile::exists(*tempVcdFilePath)) {
                    QFile::remove(*tempVcdFilePath);
                }
                *rtlProcessed = false;
                
                mainWindow->showRtlDirectoryDialogForSignalDialog();
                if (*rtlProcessed) {
                    return true; // User selected a directory and processing succeeded
                }
            } else if (result == QMessageBox::Ignore) {
                // Continue without RTL processing
                statusLabel->setText("Continuing without RTL information...");
                return true;
            }
            return false;
        }
    } else {
        // No RTL directory found - ask user
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("RTL Information Not Available");
        msgBox.setIcon(QMessageBox::Information);
        msgBox.setText("No RTL directory found. Port directions (input/output/inout) will not be available.\n\n"
                      "Would you like to specify an RTL directory?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        
        if (msgBox.exec() == QMessageBox::Yes) {
            mainWindow->showRtlDirectoryDialogForSignalDialog();
            if (*rtlProcessed) {
                return true; // User selected a directory and processing succeeded
            }
        }
        return false;
    }
}

void SignalSelectionDialog::onRtlDirectoryButtonClicked()
{
    if (!mainWindow) {
        qDebug() << "MainWindow not set for RTL directory selection";
        return;
    }
    
    QString currentRtlDir = mainWindow->findRtlDirectoryForSignalDialog(currentVcdFilePath);
    QString startDir = currentRtlDir.isEmpty() ? QFileInfo(currentVcdFilePath).dir().path() : currentRtlDir;
    
    QString rtlDir = QFileDialog::getExistingDirectory(this, 
        "Select RTL Directory",
        startDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!rtlDir.isEmpty()) {
        // Verify the selected directory contains RTL files
        QDir dir(rtlDir);
        QStringList rtlFiles = dir.entryList(QStringList() << "*.v" << "*.sv", QDir::Files);
        
        if (rtlFiles.isEmpty()) {
            QMessageBox::warning(this, "No RTL Files", 
                "The selected directory does not contain any Verilog (.v) or SystemVerilog (.sv) files.\n\n"
                "Please select a directory that contains RTL source files.");
            return;
        }
        
        qDebug() << "User selected RTL directory:" << rtlDir;
        qDebug() << "RTL files found:" << rtlFiles.size();
        
        // NEW: Clean up existing temp file before processing with new RTL directory
        if (!tempVcdFilePath->isEmpty() && QFile::exists(*tempVcdFilePath)) {
            QFile::remove(*tempVcdFilePath);
            qDebug() << "Cleaned up previous temp file for new RTL directory";
        }
        
        // Reset RTL processed flag to force reprocessing
        *rtlProcessed = false;
        
        // NEW: Process VCD with the SPECIFICALLY SELECTED RTL directory
        statusLabel->setText("Processing RTL files from selected directory...");
        QApplication::processEvents();
        
        // Use the MainWindow method that accepts a specific RTL directory
        bool success = mainWindow->runVcdPortMapperForSignalDialog(currentVcdFilePath, *tempVcdFilePath, rtlDir);
        
        if (success) {
            *rtlProcessed = true;
            statusLabel->setText("RTL processing completed with manually selected directory");
            qDebug() << "Successfully processed VCD with manually selected RTL directory:" << rtlDir;
            
            // Update the filter to show the new RTL-processed signals
            if (currentTypeFilter == "input" || currentTypeFilter == "output" || currentTypeFilter == "inout") {
                applySignalFilter();
            }
        } else {
            statusLabel->setText("RTL processing failed");
            QMessageBox::warning(this, "RTL Processing Failed", 
                "Failed to process RTL files from the selected directory.\n\n"
                "Please check that the directory contains valid RTL source files.");
        }
    }
}