#include "SignalSelectionDialog.h"
#include <QHeaderView>
#include <QDebug>
#include <QApplication>
#include <QTreeWidgetItemIterator>

SignalSelectionDialog::SignalSelectionDialog(QWidget *parent)
    : QDialog(parent), lastSelectedItem(nullptr)
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

    connect(searchEdit, &QLineEdit::textChanged, this, &SignalSelectionDialog::onSearchTextChanged);

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(searchEdit);

    // Progress bar
    progressBar = new QProgressBar();
    progressBar->setVisible(false);

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
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(signalTree, 1);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(buttonBox);
}

void SignalSelectionDialog::onItemClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    // Only handle clicks on signal items (not scope items)
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<VCDSignal>()) {
        handleMultiSelection(item);
    }
}

void SignalSelectionDialog::handleMultiSelection(QTreeWidgetItem *item)
{
    if (!item) return;
    
    QVariant data = item->data(0, Qt::UserRole);
    if (!data.canConvert<VCDSignal>()) return;
    
    Qt::KeyboardModifiers modifiers = QApplication::keyboardModifiers();
    
    // Block signals to prevent recursive calls during bulk operations
    signalTree->blockSignals(true);
    
    if (modifiers & Qt::ShiftModifier && lastSelectedItem) {
        // Shift+click: select range from last selected to current item
        
        // Get all signal items in the tree
        QList<QTreeWidgetItem*> allSignalItems;
        QTreeWidgetItemIterator it(signalTree);
        while (*it) {
            QVariant itData = (*it)->data(0, Qt::UserRole);
            if (itData.canConvert<VCDSignal>()) {
                allSignalItems.append(*it);
            }
            ++it;
        }
        
        // Find indices of last selected and current items
        int startIndex = allSignalItems.indexOf(lastSelectedItem);
        int endIndex = allSignalItems.indexOf(item);
        
        if (startIndex != -1 && endIndex != -1) {
            int low = qMin(startIndex, endIndex);
            int high = qMax(startIndex, endIndex);
            
            // Select all items in the range
            for (int i = low; i <= high; i++) {
                if (i < allSignalItems.size()) {
                    QTreeWidgetItem *rangeItem = allSignalItems[i];
                    VCDSignal signal = rangeItem->data(0, Qt::UserRole).value<VCDSignal>();
                    selectedSignals.insert(signal.identifier);
                    rangeItem->setCheckState(0, Qt::Checked);
                }
            }
        }
    } else if (modifiers & Qt::ControlModifier) {
        // Ctrl+click: toggle selection of current item
        VCDSignal signal = data.value<VCDSignal>();
        if (selectedSignals.contains(signal.identifier)) {
            selectedSignals.remove(signal.identifier);
            item->setCheckState(0, Qt::Unchecked);
        } else {
            selectedSignals.insert(signal.identifier);
            item->setCheckState(0, Qt::Checked);
        }
        lastSelectedItem = item;
    } else {
        // Regular click: single selection (clear others and select this one)
        // First, clear all selections
        QTreeWidgetItemIterator clearIt(signalTree);
        while (*clearIt) {
            QVariant clearData = (*clearIt)->data(0, Qt::UserRole);
            if (clearData.canConvert<VCDSignal>()) {
                VCDSignal clearSignal = clearData.value<VCDSignal>();
                selectedSignals.remove(clearSignal.identifier);
                (*clearIt)->setCheckState(0, Qt::Unchecked);
            }
            ++clearIt;
        }
        
        // Then select the clicked item
        VCDSignal signal = data.value<VCDSignal>();
        selectedSignals.insert(signal.identifier);
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
    lastSelectedItem = nullptr; // Reset last selection

    signalTree->clear();

    // Create a set of visible signal identifiers
    visibleSignalIdentifiers.clear();
    for (const auto &signal : visibleSignals)
    {
        visibleSignalIdentifiers.insert(signal.identifier);
    }

    statusLabel->setText("Building scope structure...");
    progressBar->setVisible(true);
    progressBar->setRange(0, allSignals.size());

    // Build scope structure
    buildScopeStructure();

    progressBar->setVisible(false);

    // Populate top-level scopes
    populateTopLevelScopes();

    statusLabel->setText(QString("Ready - %1 signals in %2 scopes")
                             .arg(allSignals.size())
                             .arg(scopeSignals.size()));
}

void SignalSelectionDialog::buildScopeStructure()
{
    int processed = 0;

    for (const auto &signal : allSignals)
    {
        // Skip signals that are already visible
        if (visibleSignalIdentifiers.contains(signal.identifier))
        {
            processed++;
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

            for (int i = 0; i < scopeParts.size(); i++)
            {
                if (!currentPath.isEmpty())
                    currentPath += ".";
                currentPath += scopeParts[i];

                if (i < scopeParts.size() - 1)
                {
                    QString parentPath = currentPath;
                    QString childName = scopeParts[i + 1];
                    QString childPath = parentPath + "." + childName;

                    if (!childScopes[parentPath].contains(childPath))
                    {
                        childScopes[parentPath].append(childPath);
                    }
                }
            }
        }

        processed++;
        if (processed % 1000 == 0)
        {
            progressBar->setValue(processed);
            QApplication::processEvents();
        }
    }
}

void SignalSelectionDialog::populateTopLevelScopes()
{
    // Add global signals (signals with no scope)
    if (scopeSignals.contains("") && !scopeSignals[""].isEmpty())
    {
        QTreeWidgetItem *globalItem = new QTreeWidgetItem();
        globalItem->setText(0, "Global Signals");
        globalItem->setData(0, Qt::UserRole, "");
        signalTree->addTopLevelItem(globalItem);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        globalItem->addChild(placeholder);

        populatedScopes.insert("");
    }

    // Find all unique scopes that have signals
    QSet<QString> scopesWithSignals;
    for (const QString &scope : scopeSignals.keys())
    {
        if (!scope.isEmpty() && !scopeSignals[scope].isEmpty())
        {
            scopesWithSignals.insert(scope);
        }
    }

    // Also include scopes that have children (even if they don't have direct signals)
    for (const QString &scope : childScopes.keys())
    {
        if (!scope.isEmpty())
        {
            scopesWithSignals.insert(scope);
        }
    }

    // Build the hierarchy - find top-level scopes
    QSet<QString> topLevelScopes;
    for (const QString &scope : scopesWithSignals)
    {
        bool isTopLevel = true;

        // Check if this scope is a child of any other scope in our data
        for (const QString &potentialParent : scopesWithSignals)
        {
            if (scope.startsWith(potentialParent + ".") && scope != potentialParent)
            {
                isTopLevel = false;
                break;
            }
        }

        if (isTopLevel && !scope.isEmpty())
        {
            topLevelScopes.insert(scope);
        }
    }

    // If no top-level scopes found but we have signals, show all scopes with signals
    if (topLevelScopes.isEmpty() && !scopesWithSignals.isEmpty())
    {
        qDebug() << "No clear hierarchy found, showing all scopes with signals";
        for (const QString &scope : scopesWithSignals)
        {
            topLevelScopes.insert(scope);
        }
    }

    // Create tree items for top-level scopes
    for (const QString &scope : topLevelScopes)
    {
        QString displayName = scope;
        // Extract just the last part for display, but show full path as tooltip
        QStringList parts = scope.split('.');
        if (!parts.isEmpty())
        {
            displayName = parts.last();
        }

        QTreeWidgetItem *scopeItem = new QTreeWidgetItem();
        scopeItem->setText(0, displayName);
        scopeItem->setToolTip(0, scope); // Show full path as tooltip
        scopeItem->setData(0, Qt::UserRole, scope);

        // Add placeholder for lazy loading
        QTreeWidgetItem *placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
        scopeItem->addChild(placeholder);

        signalTree->addTopLevelItem(scopeItem);
        populatedScopes.insert(scope);
    }

    // If we still have no items but there are signals, create a fallback display
    if (signalTree->topLevelItemCount() == 0 && !allSignals.isEmpty())
    {
        qDebug() << "Using fallback display for signals";

        // Group all available signals by their first scope part
        QMap<QString, QVector<VCDSignal>> signalsByFirstPart;

        for (const VCDSignal &signal : allSignals)
        {
            if (visibleSignalIdentifiers.contains(signal.identifier))
            {
                continue;
            }

            QString firstPart = "Other Signals";
            if (!signal.scope.isEmpty())
            {
                QStringList parts = signal.scope.split('.');
                firstPart = parts.first();
            }

            signalsByFirstPart[firstPart].append(signal);
        }

        // Create tree items for each group
        for (auto it = signalsByFirstPart.begin(); it != signalsByFirstPart.end(); ++it)
        {
            QTreeWidgetItem *groupItem = new QTreeWidgetItem();
            groupItem->setText(0, it.key());
            groupItem->setData(0, Qt::UserRole, it.key());
            signalTree->addTopLevelItem(groupItem);

            // Add placeholder
            QTreeWidgetItem *placeholder = new QTreeWidgetItem();
            placeholder->setText(0, "Loading...");
            placeholder->setData(0, Qt::UserRole, "PLACEHOLDER");
            groupItem->addChild(placeholder);

            // Store the signals for this group
            scopeSignals[it.key()] = it.value();
            populatedScopes.insert(it.key());
        }
    }

    qDebug() << "Displaying" << signalTree->topLevelItemCount() << "top-level items";
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

    // Check if this is a fallback group (not a real scope path)
    bool isFallbackGroup = !scopePath.contains('.') && scopePath != "" &&
                           scopeSignals.contains(scopePath) &&
                           !childScopes.contains(scopePath);

    if (isFallbackGroup)
    {
        // This is a fallback group - just show the signals directly
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

            if (selectedSignals.contains(signal.identifier))
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
    else
    {
        // Normal scope processing
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

                if (selectedSignals.contains(signal.identifier))
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
    }

    populatedScopes.insert(scopePath + "_POPULATED");
}

void SignalSelectionDialog::onItemExpanded(QTreeWidgetItem *item)
{
    QString scopePath = item->data(0, Qt::UserRole).toString();
    if (scopePath != "PLACEHOLDER")
    {
        populateScopeChildren(scopePath, item);
    }
}

void SignalSelectionDialog::onItemChanged(QTreeWidgetItem *item, int column)
{
    if (column != 0) return;
    
    QVariant data = item->data(0, Qt::UserRole);
    if (data.canConvert<VCDSignal>()) {
        VCDSignal signal = data.value<VCDSignal>();
        if (item->checkState(0) == Qt::Checked) {
            selectedSignals.insert(signal.identifier);
        } else {
            selectedSignals.remove(signal.identifier);
        }
        
        // Update status
        statusLabel->setText(QString("%1 signal(s) selected").arg(selectedSignals.size()));
    }
}

QList<VCDSignal> SignalSelectionDialog::getSelectedSignals() const
{
    QList<VCDSignal> result;

    for (const QString &identifier : selectedSignals)
    {
        // Find the signal in allSignals
        for (const VCDSignal &signal : allSignals)
        {
            if (signal.identifier == identifier)
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
    
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            VCDSignal signal = data.value<VCDSignal>();
            selectedSignals.insert(signal.identifier);
            item->setCheckState(0, Qt::Checked);
        }
        ++it;
    }
    
    signalTree->blockSignals(false);
    statusLabel->setText(QString("%1 signal(s) selected").arg(selectedSignals.size()));
}

void SignalSelectionDialog::deselectAll()
{
    signalTree->blockSignals(true);
    
    selectedSignals.clear();
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            item->setCheckState(0, Qt::Unchecked);
        }
        ++it;
    }
    
    lastSelectedItem = nullptr;
    signalTree->blockSignals(false);
    statusLabel->setText("All signals deselected");
}

void SignalSelectionDialog::onSearchTextChanged(const QString &text)
{
    currentFilter = text;

    if (text.isEmpty())
    {
        // Clear any existing filtering - rebuild the tree
        signalTree->clear();
        populatedScopes.clear();
        populateTopLevelScopes();
    }
    else
    {
        // For search, we'll do a simpler flat display of matching signals
        signalTree->clear();

        progressBar->setVisible(true);
        progressBar->setRange(0, allSignals.size());

        QString searchLower = text.toLower();
        int matches = 0;
        int processed = 0;

        // Group matching signals by scope for the tree display
        QMap<QString, QVector<VCDSignal>> matchingSignalsByScope;

        for (const auto &signal : allSignals)
        {
            if (visibleSignalIdentifiers.contains(signal.identifier))
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
            if (processed % 1000 == 0)
            {
                progressBar->setValue(processed);
                QApplication::processEvents();
            }
        }

        progressBar->setVisible(false);

        // Display matching signals in a tree
        QMap<QString, QVector<VCDSignal>>::iterator it;
        for (it = matchingSignalsByScope.begin(); it != matchingSignalsByScope.end(); ++it)
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

            for (const VCDSignal &signal : signalsInScope)
            {
                QTreeWidgetItem *signalItem = new QTreeWidgetItem(scopeItem);
                signalItem->setText(0, signal.name);
                signalItem->setText(1, QString::number(signal.width));
                signalItem->setText(2, signal.type);
                signalItem->setText(3, signal.identifier);
                signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
                signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);

                if (selectedSignals.contains(signal.identifier))
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

        statusLabel->setText(QString("Found %1 signals matching '%2'").arg(matches).arg(text));
    }
}