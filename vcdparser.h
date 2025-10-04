#ifndef VCDPARSER_H
#define VCDPARSER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QFile>
#include <QTextStream>
#include <QSet>

struct VCDSignal {
    QString identifier;
    QString name;
    QString scope;
    int width;
    QString type;
    QString fullName;  // ADD THIS: unique identifier for the signal

    bool operator==(const VCDSignal& other) const {
        return fullName == other.fullName;  // Compare using fullName instead of identifier
    }
};

Q_DECLARE_METATYPE(VCDSignal)

struct VCDValueChange {
    int timestamp;
    QString value;
};

class VCDParser : public QObject
{
    Q_OBJECT

public:
    explicit VCDParser(QObject *parent = nullptr);
    ~VCDParser();

    bool parseFile(const QString &filename);
    bool parseHeaderOnly(const QString &filename); // Fast header-only parsing
    QString getError() const { return errorString; }

    const QVector<VCDSignal>& getSignals() const { return vcdSignals; }
    QVector<VCDValueChange> getValueChangesForSignal(const QString &fullName);  // CHANGE: use fullName
    const QMap<QString, VCDSignal>& getIdentifierMap() const { return identifierMap; }
    const QMap<QString, VCDSignal>& getFullNameMap() const { return fullNameMap; }  // ADD THIS
    int getEndTime() const { return endTime; }
    
    // Load specific signals on demand
    bool loadSignalsData(const QList<QString> &fullNames);  // CHANGE: use fullNames

private:
    bool parseHeader(QTextStream &stream);
    bool parseValueChangesForSignals(QTextStream &stream, const QSet<QString> &signalsToLoad);
    void parseScopeLine(const QString &line);
    void parseVarLine(const QString &line);
    void parseTimescale(const QString &line);
    QString generateFullName(const QString &scope, const QString &name);  // ADD THIS

    QString errorString;
    QVector<VCDSignal> vcdSignals;
    QMap<QString, VCDSignal> identifierMap;
    QMap<QString, VCDSignal> fullNameMap;  // ADD THIS: maps fullName -> VCDSignal
    
    // Data storage
    QMap<QString, QVector<VCDValueChange>> valueChanges;
    QSet<QString> loadedSignals; // Track which signals have data loaded
    
    QString currentScope;
    int endTime;
    QString timescale;
    QString vcdFilename;
};

#endif // VCDPARSER_H