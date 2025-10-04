#include "SignalSelectionDialog.h"
#include <QHeaderView>
#include <QDebug>
#include <QApplication>
#include <QTreeWidgetItemIterator>

SignalSelectionDialog::SignalSelectionDialog(QWidget *parent)
    : QDialog(parent)
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
    mainLayout->addWidget(signalTree, 1);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(buttonBox);
}

void SignalSelectionDialog::setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals)
{
    this->allSignals = allSignals;

    // Create a set of visible signal identifiers for quick lookup
    visibleSignalIdentifiers.clear();
    for (const auto &signal : visibleSignals)
    {
        visibleSignalIdentifiers.insert(signal.identifier);
    }

    // Show progress for large files
    if (allSignals.size() > 1000)
    {
        progressBar->setVisible(true);
        progressBar->setRange(0, allSignals.size());
        progressBar->setValue(0);
    }

    buildSignalTree();

    progressBar->setVisible(false);
}

void SignalSelectionDialog::buildSignalTree()
{
    signalTree->clear();

    if (allSignals.isEmpty())
    {
        return;
    }

    // For very large files, use a flat list instead of tree structure
    if (allSignals.size() > 50000)
    {
        progressBar->setVisible(true);
        progressBar->setRange(0, allSignals.size());
        progressBar->setValue(0);

        int processed = 0;
        for (const auto &signal : allSignals)
        {
            // Skip signals that are already visible
            if (visibleSignalIdentifiers.contains(signal.identifier))
            {
                processed++;
                continue;
            }

            QTreeWidgetItem *signalItem = new QTreeWidgetItem();
            QString displayName = signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name;
            signalItem->setText(0, displayName);
            signalItem->setText(1, QString::number(signal.width));
            signalItem->setText(2, signal.type);
            signalItem->setText(3, signal.identifier);
            signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
            signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);
            signalItem->setCheckState(0, Qt::Unchecked);

            signalTree->addTopLevelItem(signalItem);

            processed++;
            if (processed % 100 == 0)
            {
                progressBar->setValue(processed);
                QApplication::processEvents();
            }
        }
    }
    else
    {

        QMap<QString, QTreeWidgetItem *> scopeItems;
        int processed = 0;

        for (const auto &signal : allSignals)
        {
            // Skip signals that are already visible in the waveform
            if (visibleSignalIdentifiers.contains(signal.identifier))
            {
                processed++;
                continue;
            }

            QString scopePath = signal.scope;
            if (!scopeItems.contains(scopePath))
            {
                QStringList scopeParts = scopePath.split('.');
                QTreeWidgetItem *parent = nullptr;
                QString currentPath;

                for (const QString &part : scopeParts)
                {
                    if (!currentPath.isEmpty())
                        currentPath += ".";
                    currentPath += part;

                    if (!scopeItems.contains(currentPath))
                    {
                        QTreeWidgetItem *item = new QTreeWidgetItem();
                        item->setText(0, part);
                        item->setData(0, Qt::UserRole, currentPath);

                        if (parent)
                        {
                            parent->addChild(item);
                        }
                        else
                        {
                            signalTree->addTopLevelItem(item);
                        }

                        scopeItems[currentPath] = item;
                        parent = item;
                    }
                    else
                    {
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

            if (scopeItems.contains(scopePath))
            {
                scopeItems[scopePath]->addChild(signalItem);
            }

            // Update progress and process events for large files
            processed++;
            if (allSignals.size() > 1000 && processed % 100 == 0)
            {
                progressBar->setValue(processed);
                QApplication::processEvents();
            }
        }
    }

    // Expand all for better usability
    signalTree->expandAll();

    qDebug() << "Signal tree built with" << signalTree->topLevelItemCount() << "top-level items";
    
}

QList<VCDSignal> SignalSelectionDialog::getSelectedSignals() const
{
    QList<VCDSignal> selectedSignals;

    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        if ((*it)->checkState(0) == Qt::Checked)
        {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>())
            {
                VCDSignal signal = data.value<VCDSignal>();
                selectedSignals.append(signal);
            }
        }
        ++it;
    }

    return selectedSignals;
}

void SignalSelectionDialog::selectAll()
{
    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>())
        {
            (*it)->setCheckState(0, Qt::Checked);
        }
        ++it;
    }
}

void SignalSelectionDialog::deselectAll()
{
    QTreeWidgetItemIterator it(signalTree);
    while (*it)
    {
        QVariant data = (*it)->data(0, Qt::UserRole);
        if (data.canConvert<VCDSignal>())
        {
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
    if (filter.isEmpty())
    {
        // Show all items
        QTreeWidgetItemIterator it(signalTree);
        while (*it)
        {
            (*it)->setHidden(false);
            ++it;
        }
        signalTree->expandAll();
        return;
    }

    QString filterLower = filter.toLower();
    int visibleCount = 0;

    // First, hide all items
    QTreeWidgetItemIterator hideIt(signalTree);
    while (*hideIt)
    {
        (*hideIt)->setHidden(true);
        ++hideIt;
    }

    // Show matching items and their parents
    QTreeWidgetItemIterator showIt(signalTree);
    while (*showIt)
    {
        QTreeWidgetItem *item = *showIt;
        QVariant data = item->data(0, Qt::UserRole);

        if (data.canConvert<VCDSignal>())
        {
            VCDSignal signal = data.value<VCDSignal>();
            QString signalPath = (signal.scope.isEmpty() ? signal.name : signal.scope + "." + signal.name).toLower();

            if (signalPath.contains(filterLower))
            {
                // Show this signal
                item->setHidden(false);
                visibleCount++;

                // Show all parent items
                QTreeWidgetItem *parent = item->parent();
                while (parent)
                {
                    parent->setHidden(false);
                    parent->setExpanded(true);
                    parent = parent->parent();
                }
            }
        }
        ++showIt;
    }

    qDebug() << "Search filter applied:" << filter << "-" << visibleCount << "signals visible";
}