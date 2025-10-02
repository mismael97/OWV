#include "SignalSelectionDialog.h"
#include <QHeaderView>
#include <QDebug>

SignalSelectionDialog::SignalSelectionDialog(QWidget *parent)
    : QDialog(parent), lastHighlightedItem(nullptr)
{
    setWindowTitle("Add Signals to Waveform");
    setMinimumSize(600, 500);

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
    
    // Signal tree
    signalTree = new QTreeWidget();
    signalTree->setHeaderLabels({"Signal", "Width", "Type", "Identifier"});
    signalTree->setAlternatingRowColors(true);
    signalTree->header()->setStretchLastSection(false);
    signalTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    signalTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    signalTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    signalTree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);

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
    mainLayout->addWidget(signalTree);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(buttonBox);
}


void SignalSelectionDialog::setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals)
{
    signalTree->clear();
    lastHighlightedItem = nullptr;

    // Create a set of visible signal identifiers for quick lookup
    QSet<QString> visibleSignalIdentifiers;
    for (const auto& signal : visibleSignals) {
        visibleSignalIdentifiers.insert(signal.identifier);
    }

    QMap<QString, QTreeWidgetItem*> scopeItems;

    for (const auto& signal : allSignals) {
        // Skip signals that are already visible in the waveform
        if (visibleSignalIdentifiers.contains(signal.identifier)) {
            continue;
        }

        QString scopePath = signal.scope;
        if (!scopeItems.contains(scopePath)) {
            QStringList scopeParts = scopePath.split('.');
            QTreeWidgetItem *parent = nullptr;
            QString currentPath;

            for (const QString& part : scopeParts) {
                if (!currentPath.isEmpty()) currentPath += ".";
                currentPath += part;

                if (!scopeItems.contains(currentPath)) {
                    QTreeWidgetItem *item = new QTreeWidgetItem();
                    item->setText(0, part);
                    item->setData(0, Qt::UserRole, currentPath);

                    if (parent) {
                        parent->addChild(item);
                    } else {
                        signalTree->addTopLevelItem(item);
                    }

                    scopeItems[currentPath] = item;
                    parent = item;
                } else {
                    parent = scopeItems[currentPath];
                }
            }
        }

        QTreeWidgetItem *signalItem = new QTreeWidgetItem();
        signalItem->setText(0, signal.name);
        signalItem->setText(1, QString::number(signal.width));
        signalItem->setText(2, signal.type);
        signalItem->setText(3, signal.identifier);
        signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
        signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);
        signalItem->setCheckState(0, Qt::Unchecked);

        scopeItems[scopePath]->addChild(signalItem);
    }

    // Apply current search filter if any
    if (!searchEdit->text().isEmpty()) {
        filterTree(searchEdit->text());
    } else {
        signalTree->expandAll();
    }
}


QList<VCDSignal> SignalSelectionDialog::getSelectedSignals() const
{
    QList<VCDSignal> selectedSignals;

    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>()) {
                VCDSignal signal = data.value<VCDSignal>();
                selectedSignals.append(signal);
                qDebug() << "Selected signal:" << signal.name << "ID:" << signal.identifier;
            }
        }
        ++it;
    }

    return selectedSignals;
}

void SignalSelectionDialog::selectAll()
{
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            (*it)->setCheckState(0, Qt::Checked);
        }
        ++it;
    }
}

void SignalSelectionDialog::deselectAll()
{
    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>()) {
            (*it)->setCheckState(0, Qt::Unchecked);
        }
        ++it;
    }
}

void SignalSelectionDialog::onSearchTextChanged(const QString &text)
{
    filterTree(text);
}

void SignalSelectionDialog::filterTree(const QString &filter)
{
    if (filter.isEmpty()) {
        // Show all items and expand all
        QTreeWidgetItemIterator it(signalTree);
        while (*it) {
            (*it)->setHidden(false);
            ++it;
        }
        signalTree->expandAll();
        lastHighlightedItem = nullptr;
        return;
    }

    QString filterLower = filter.toLower();
    QTreeWidgetItemIterator it(signalTree);
    
    // First, hide all items
    while (*it) {
        (*it)->setHidden(true);
        (*it)->setBackground(0, QBrush()); // Clear any previous highlighting
        ++it;
    }

    // Show and highlight matching items and their parents
    it = QTreeWidgetItemIterator(signalTree);
    while (*it) {
        QTreeWidgetItem *item = *it;
        QVariant data = item->data(0, Qt::UserRole);
        
        if (data.canConvert<VCDSignal>()) {
            // This is a signal item
            VCDSignal signal = data.value<VCDSignal>();
            QString signalPath = (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name).toLower();
            
            if (signalPath.contains(filterLower)) {
                // Show this signal and all its parents
                item->setHidden(false);
                item->setBackground(0, QColor(255, 255, 200)); // Highlight matching signals
                
                // Expand and show all parent items
                expandAllParents(item);
                
                lastHighlightedItem = item;
            }
        } else {
            // This is a scope item - check if it has any visible children
            bool hasVisibleChildren = false;
            for (int i = 0; i < item->childCount(); ++i) {
                if (!item->child(i)->isHidden()) {
                    hasVisibleChildren = true;
                    break;
                }
            }
            
            if (hasVisibleChildren) {
                item->setHidden(false);
                item->setExpanded(true);
            }
        }
        ++it;
    }
}

void SignalSelectionDialog::expandAllParents(QTreeWidgetItem *item)
{
    QTreeWidgetItem *parent = item->parent();
    while (parent) {
        parent->setHidden(false);
        parent->setExpanded(true);
        parent = parent->parent();
    }
}