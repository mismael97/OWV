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
#include "vcdparser.h"

class SignalSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SignalSelectionDialog(QWidget *parent = nullptr);
    void setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals);
    QList<VCDSignal> getSelectedSignals() const;

private slots:
    void onScopeItemChanged(QTreeWidgetItem *item, int column); // Add this
    void selectAll();
    void deselectAll();
    void onSearchTextChanged(const QString &text);
    void onItemExpanded(QTreeWidgetItem *item);
    void onItemChanged(QTreeWidgetItem *item, int column);
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    void buildScopeStructure();
    void populateTopLevelScopes();
    void populateScopeChildren(const QString &scopePath, QTreeWidgetItem *parentItem);
    void filterTree(const QString &filter);
    void handleMultiSelection(QTreeWidgetItem *item);
    void updateScopeCheckState(QTreeWidgetItem *scopeItem);                 // Add this
    void setScopeSignalsSelection(const QString &scopePath, bool selected); // Add this
    void updateParentScopeCheckState(QTreeWidgetItem *childItem);
    void updateTreeWidgetCheckStates(const QString &scopePath, bool selected);
    QTreeWidget *signalTree;
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QDialogButtonBox *buttonBox;
    QLineEdit *searchEdit;
    QProgressBar *progressBar;
    QLabel *statusLabel;

    // Data storage
    QVector<VCDSignal> allSignals;
    QSet<QString> visibleSignalIdentifiers;
    QSet<QString> selectedSignals;

    // Scope structure: scopePath -> list of signals in that scope
    QMap<QString, QVector<VCDSignal>> scopeSignals;
    QMap<QString, QStringList> childScopes; // scopePath -> list of immediate child scopes

    // Track which scopes have been populated
    QSet<QString> populatedScopes;

    // Multi-selection support
    QTreeWidgetItem *lastSelectedItem;

    QString currentFilter;
};

#endif // SIGNALSELECTIONDIALOG_H