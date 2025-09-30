#ifndef VCDPARSER_H
#define VCDPARSER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QFile>
#include <QTextStream>

class VCDParser : public QObject
{
    Q_OBJECT

public:
    struct SignalValue {
        quint64 time;
        QString value;

        SignalValue(quint64 t, const QString& v) : time(t), value(v) {}
    };

    struct Signal {
        QString id;
        QString name;
        int width;
        QVector<SignalValue> values;
    };

    explicit VCDParser(QObject *parent = nullptr);

    bool parseFile(const QString& fileName);
    const QVector<Signal>& getSignals() const { return m_signals; }
    QString errorString() const { return m_errorString; }
    double getTimeScale() const { return m_timeScale; }

signals:
    void parsingCompleted();

private:
    bool parseHeader(QTextStream& stream);
    bool parseValueChanges(QTextStream& stream);
    void addValueChange(const QString& id, const QString& value, quint64 time);

    QVector<Signal> m_signals;
    QMap<QString, int> m_idToIndex;
    QString m_errorString;
    double m_timeScale;
    QString m_timeUnit;
    quint64 m_currentTime;
};

#endif // VCDPARSER_H
