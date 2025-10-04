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

private:
    void buildSignalTree();
    void filterTree(const QString &filter);
    
    QTreeWidget *signalTree;
    QPushButton *selectAllButton;
    QPushButton *deselectAllButton;
    QDialogButtonBox *buttonBox;
    QLineEdit *searchEdit;
    QProgressBar *progressBar;
    
    QVector<VCDSignal> allSignals;
    QSet<QString> visibleSignalIdentifiers;
};

#endif // SIGNALSELECTIONDIALOG_H