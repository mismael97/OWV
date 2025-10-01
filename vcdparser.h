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

    bool operator==(const VCDSignal& other) const {
        return identifier == other.identifier;
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
    QString getError() const { return errorString; }

    const QVector<VCDSignal>& getSignals() const { return vcdSignals; }
    const QMap<QString, QVector<VCDValueChange>>& getValueChanges() const { return valueChanges; }
    int getEndTime() const { return endTime; }
    const QMap<QString, VCDSignal>& getIdentifierMap() const { return identifierMap; }

private:
    bool parseHeader(QTextStream &stream);
    bool parseValueChanges(QTextStream &stream);
    void parseScopeLine(const QString &line);
    void parseVarLine(const QString &line);
    void parseTimescale(const QString &line);

    QString errorString;
    QVector<VCDSignal> vcdSignals;
    QMap<QString, VCDSignal> identifierMap;
    QMap<QString, QVector<VCDValueChange>> valueChanges;
    QString currentScope;
    int endTime;
    QString timescale;
};

#endif // VCDPARSER_H
