#include "SignalSelectionDialog.h"
#include <QHeaderView>

SignalSelectionDialog::SignalSelectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Signals to Waveform");
    setMinimumSize(600, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Signal tree
    signalTree = new QTreeWidget();
    signalTree->setHeaderLabels({"Signal", "Width", "Type"});
    signalTree->setAlternatingRowColors(true);
    signalTree->header()->setStretchLastSection(false);
    signalTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    signalTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    signalTree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);

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

    mainLayout->addWidget(signalTree);
    mainLayout->addLayout(controlsLayout);
    mainLayout->addWidget(buttonBox);
}

void SignalSelectionDialog::setSignals(const QVector<VCDSignal> &vcdSignals, const QList<VCDSignal> &currentSignals)
{
    signalTree->clear();

    QMap<QString, QTreeWidgetItem*> scopeItems;

    // Create a set of current signal identifiers for quick lookup
    QSet<QString> currentSignalIdentifiers;
    for (const auto& signal : currentSignals) {
        currentSignalIdentifiers.insert(signal.identifier);
    }

    for (const auto& signal : vcdSignals) {
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
        signalItem->setData(0, Qt::UserRole, QVariant::fromValue(signal));
        signalItem->setFlags(signalItem->flags() | Qt::ItemIsUserCheckable);

        // Check if this signal is already in the current visible signals
        if (currentSignalIdentifiers.contains(signal.identifier)) {
            signalItem->setCheckState(0, Qt::Checked);
        } else {
            signalItem->setCheckState(0, Qt::Unchecked);
        }

        scopeItems[scopePath]->addChild(signalItem);
    }

    signalTree->expandAll();
}

QList<VCDSignal> SignalSelectionDialog::getSelectedSignals() const
{
    QList<VCDSignal> selectedSignals;

    QTreeWidgetItemIterator it(signalTree);
    while (*it) {
        if ((*it)->checkState(0) == Qt::Checked) {
            QVariant data = (*it)->data(0, Qt::UserRole);
            if (data.canConvert<VCDSignal>()) {
                selectedSignals.append(data.value<VCDSignal>());
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
