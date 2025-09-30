#include "vcdparser.h"
#include <QRegularExpression>
#include <QDebug>

VCDParser::VCDParser(QObject *parent)
    : QObject(parent)
    , m_timeScale(1.0)
    , m_currentTime(0)
{
}

bool VCDParser::parseFile(const QString& fileName)
{
    qDebug() << "Parsing VCD file:" << fileName;

    m_signals.clear();
    m_idToIndex.clear();
    m_errorString.clear();
    m_timeScale = 1.0;
    m_currentTime = 0;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_errorString = "Cannot open file: " + file.errorString();
        qDebug() << "File open error:" << m_errorString;
        return false;
    }

    QTextStream stream(&file);
    bool headerSuccess = parseHeader(stream);
    qDebug() << "Header parsing success:" << headerSuccess;
    qDebug() << "Number of signals found:" << m_signals.size();
    qDebug() << "Time scale:" << m_timeScale;

    bool valueSuccess = parseValueChanges(stream);
    qDebug() << "Value parsing success:" << valueSuccess;

    file.close();

    if (headerSuccess && valueSuccess && !m_signals.isEmpty()) {
        emit parsingCompleted();
        return true;
    } else {
        m_errorString = "Failed to parse VCD file properly";
        qDebug() << "Parse failed - signals:" << m_signals.size();
        return false;
    }
}

bool VCDParser::parseHeader(QTextStream& stream)
{
    QString line;
    bool inScope = false;
    QString currentScope;

    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith("$enddefinitions")) {
            break;
        }

        if (line.startsWith("$timescale")) {
            // Read the next line for timescale value
            if (stream.readLineInto(&line)) {
                line = line.trimmed();
                // Parse timescale like "1ns" or "100ps"
                QRegularExpression re("(\\d+\\.?\\d*)\\s*(\\w+)");
                QRegularExpressionMatch match = re.match(line);
                if (match.hasMatch()) {
                    m_timeScale = match.captured(1).toDouble();
                    m_timeUnit = match.captured(2);
                    qDebug() << "Parsed timescale:" << m_timeScale << m_timeUnit;
                }
                // Skip until $end
                while (line != "$end" && !stream.atEnd()) {
                    stream.readLineInto(&line);
                }
            }
        }
        else if (line.startsWith("$scope")) {
            inScope = true;
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                currentScope = parts[2];
                qDebug() << "Scope:" << currentScope;
            }
        }
        else if (line.startsWith("$upscope")) {
            inScope = false;
            currentScope.clear();
        }
        else if (line.startsWith("$var")) {
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                Signal signal;
                signal.width = parts[2].toInt();
                signal.id = parts[3];
                signal.name = currentScope.isEmpty() ? parts[4] : currentScope + "." + parts[4];

                m_idToIndex[signal.id] = m_signals.size();
                m_signals.append(signal);
                qDebug() << "Added signal:" << signal.name << "ID:" << signal.id;
            }
        }
    }

    qDebug() << "Header parsing complete. Total signals:" << m_signals.size();
    return true;
}

bool VCDParser::parseValueChanges(QTextStream& stream)
{
    QString line;

    while (stream.readLineInto(&line)) {
        if (line.startsWith("#")) {
            m_currentTime = line.mid(1).toULongLong();
        }
        else if (line.startsWith("b")) {
            int spaceIndex = line.indexOf(' ');
            if (spaceIndex != -1) {
                QString value = line.mid(1, spaceIndex - 1);
                QString id = line.mid(spaceIndex + 1);
                addValueChange(id, value, m_currentTime);
            }
        }
        else if (line.length() >= 2) {
            QString value = line.left(1);
            QString id = line.mid(1);
            addValueChange(id, value, m_currentTime);
        }
    }

    return true;
}

void VCDParser::addValueChange(const QString& id, const QString& value, quint64 time)
{
    if (m_idToIndex.contains(id)) {
        int index = m_idToIndex[id];
        m_signals[index].values.append(SignalValue(time, value));
    }
}
