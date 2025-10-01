#include "vcdparser.h"
#include <QRegularExpression>
#include <QDebug>

VCDParser::VCDParser(QObject *parent)
    : QObject(parent), endTime(0)
{
}

VCDParser::~VCDParser()
{
}

bool VCDParser::parseFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorString = "Cannot open file: " + filename;
        return false;
    }

    QTextStream stream(&file);
    vcdSignals.clear();
    valueChanges.clear();
    identifierMap.clear();
    currentScope.clear();
    endTime = 0;

    if (!parseHeader(stream)) {
        return false;
    }

    if (!parseValueChanges(stream)) {
        return false;
    }

    file.close();

    qDebug() << "VCD parsing completed";
    qDebug() << "Signals:" << vcdSignals.size() << "End time:" << endTime;

    return true;
}

bool VCDParser::parseHeader(QTextStream &stream)
{
    QRegularExpression scopeRegex("^\\$scope\\s+(\\w+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpression varRegex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpression timescaleRegex("^\\$timescale\\s+(\\S+)\\s*\\$end$");

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.startsWith("$date")) {
            while (!stream.atEnd() && !line.endsWith("$end")) {
                line = stream.readLine().trimmed();
            }
        }
        else if (line.startsWith("$version")) {
            while (!stream.atEnd() && !line.endsWith("$end")) {
                line = stream.readLine().trimmed();
            }
        }
        else if (line.startsWith("$timescale")) {
            parseTimescale(line);
        }
        else if (line.startsWith("$scope")) {
            parseScopeLine(line);
        }
        else if (line.startsWith("$var")) {
            parseVarLine(line);
        }
        else if (line.startsWith("$upscope")) {
            int lastDot = currentScope.lastIndexOf('.');
            if (lastDot != -1) {
                currentScope = currentScope.left(lastDot);
            } else {
                currentScope.clear();
            }
        }
        else if (line.startsWith("$enddefinitions")) {
            break;
        }
    }

    return true;
}

void VCDParser::parseTimescale(const QString &line)
{
    QRegularExpression regex("^\\$timescale\\s+(\\S+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    if (match.hasMatch()) {
        timescale = match.captured(1);
    }
}

void VCDParser::parseScopeLine(const QString &line)
{
    QRegularExpression regex("^\\$scope\\s+(\\w+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    if (match.hasMatch()) {
        QString scopeName = match.captured(2);
        if (!currentScope.isEmpty()) {
            currentScope += "." + scopeName;
        } else {
            currentScope = scopeName;
        }
    }
}

void VCDParser::parseVarLine(const QString &line)
{
    QRegularExpression regex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    if (match.hasMatch()) {
        VCDSignal signal;
        signal.type = match.captured(1);
        signal.width = match.captured(2).toInt();
        signal.identifier = match.captured(3);
        signal.name = match.captured(4);
        signal.scope = currentScope;

        vcdSignals.append(signal);
        identifierMap[signal.identifier] = signal;
    }
}

bool VCDParser::parseValueChanges(QTextStream &stream)
{
    QRegularExpression timestampRegex("^#(\\d+)$");
    QRegularExpression valueChangeRegex("^([01xXzZ])(\\S+)$");
    QRegularExpression vectorValueRegex("^[bB]([01xXzZ]+)\\s+(\\S+)$");

    int currentTime = 0;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty()) continue;

        QRegularExpressionMatch timestampMatch = timestampRegex.match(line);
        if (timestampMatch.hasMatch()) {
            currentTime = timestampMatch.captured(1).toInt();
            endTime = qMax(endTime, currentTime);
            continue;
        }

        QRegularExpressionMatch valueMatch = valueChangeRegex.match(line);
        if (valueMatch.hasMatch()) {
            QString value = valueMatch.captured(1).toUpper();
            QString identifier = valueMatch.captured(2);

            VCDValueChange change;
            change.timestamp = currentTime;
            change.value = value;

            valueChanges[identifier].append(change);
            continue;
        }

        QRegularExpressionMatch vectorMatch = vectorValueRegex.match(line);
        if (vectorMatch.hasMatch()) {
            QString value = vectorMatch.captured(1);
            QString identifier = vectorMatch.captured(2);

            VCDValueChange change;
            change.timestamp = currentTime;
            change.value = value;

            valueChanges[identifier].append(change);
        }
    }

    return true;
}
