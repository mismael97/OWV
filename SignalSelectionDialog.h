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
#include "vcdparser.h"


class SignalSelectionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SignalSelectionDialog(QWidget *parent = nullptr);
    void setAvailableSignals(const QVector<VCDSignal> &allSignals, const QList<VCDSignal> &visibleSignals);
    QList<VCDSignal> getSelectedSignals() const;

private slots:
    void selectAll();
    void deselectAll();
    void onSearchTextChanged(const QString &text);
    void onItemClicked(QTreeWidgetItem *item, int column);

private:
    void filterTree(const QString &filter);
    void expandAllParents(QTreeWidgetItem *item);
    void handleMultiSelection(QTreeWidgetItem *item);

    QTreeWidget *signalTree;
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QDialogButtonBox *buttonBox;
    QLineEdit *searchEdit;
    QTreeWidgetItem *lastHighlightedItem;
    QTreeWidgetItem *lastSelectedItem;
};

#endif // SIGNALSELECTIONDIALOG_H
