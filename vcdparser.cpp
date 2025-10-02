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
    qDebug() << "Signals found:" << vcdSignals.size();
    for (const auto& signal : vcdSignals) {
        qDebug() << "Signal:" << signal.name << "Identifier:" << signal.identifier << "Scope:" << signal.scope;
    }
    qDebug() << "End time:" << endTime;

    return true;
}

bool VCDParser::parseHeader(QTextStream &stream)
{
    QRegularExpression scopeRegex("^\\$scope\\s+(\\w+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpression varRegex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(\\S+.*\\S+)\\s*\\$end$");
    QRegularExpression timescaleRegex("^\\$timescale\\s+(\\S+)\\s*\\$end$");

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.startsWith("$date")) {
            // Skip date section
            while (!stream.atEnd() && !line.contains("$end")) {
                line = stream.readLine().trimmed();
            }
        }
        else if (line.startsWith("$version")) {
            // Skip version section
            while (!stream.atEnd() && !line.contains("$end")) {
                line = stream.readLine().trimmed();
            }
        }
        else if (line.startsWith("$comment")) {
            // Skip comment section
            while (!stream.atEnd() && !line.contains("$end")) {
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
            // Move up one scope level
            int lastDot = currentScope.lastIndexOf('.');
            if (lastDot != -1) {
                currentScope = currentScope.left(lastDot);
            } else {
                currentScope.clear();
            }
        }
        else if (line.startsWith("$enddefinitions")) {
            // End of header
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
        qDebug() << "Timescale:" << timescale;
    }
}

void VCDParser::parseScopeLine(const QString &line)
{
    QRegularExpression regex("^\\$scope\\s+(\\w+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    if (match.hasMatch()) {
        QString scopeType = match.captured(1);
        QString scopeName = match.captured(2);
        
        if (!currentScope.isEmpty()) {
            currentScope += "." + scopeName;
        } else {
            currentScope = scopeName;
        }
        // qDebug() << "Entering scope:" << currentScope;
    }
}

void VCDParser::parseVarLine(const QString &line)
{
    // Improved regex to handle signal names with spaces and special characters
    QRegularExpression regex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(.+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    
    if (match.hasMatch()) {
        VCDSignal signal;
        signal.type = match.captured(1);
        signal.width = match.captured(2).toInt();
        signal.identifier = match.captured(3);
        
        // The signal name might have trailing spaces before $end, so trim it
        QString signalName = match.captured(4).trimmed();
        signal.name = signalName;
        signal.scope = currentScope;

        vcdSignals.append(signal);
        identifierMap[signal.identifier] = signal;
        
        // qDebug() << "Found signal:" << signal.name << "Identifier:" << signal.identifier 
        //          << "Type:" << signal.type << "Width:" << signal.width << "Scope:" << signal.scope;
    } else {
        qDebug() << "Failed to parse var line:" << line;
    }
}

bool VCDParser::parseValueChanges(QTextStream &stream)
{
    QRegularExpression timestampRegex("^#(\\d+)$");
    // Improved regex to handle all value change formats
    QRegularExpression valueChangeRegex("^([01xXzZrb])(\\S+)$");
    QRegularExpression vectorValueRegex("^[bB]([01xXzZ]+)\\s+(\\S+)$");

    int currentTime = 0;
    bool inDumpvars = false;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty()) continue;

        // Check for timestamp
        QRegularExpressionMatch timestampMatch = timestampRegex.match(line);
        if (timestampMatch.hasMatch()) {
            currentTime = timestampMatch.captured(1).toInt();
            endTime = qMax(endTime, currentTime);
            inDumpvars = false; // We're past the initial dumpvars section
            continue;
        }

        // Check for $dumpvars section
        if (line.startsWith("$dumpvars")) {
            inDumpvars = true;
            continue;
        }

        if (line.startsWith("$end") && inDumpvars) {
            inDumpvars = false;
            continue;
        }

        // Handle scalar value changes (0, 1, x, z)
        QRegularExpressionMatch valueMatch = valueChangeRegex.match(line);
        if (valueMatch.hasMatch()) {
            QString value = valueMatch.captured(1).toUpper();
            QString identifier = valueMatch.captured(2);

            // Handle special case where identifier might be just a single character
            if (identifier.length() == 1) {
                // This is a valid single-character identifier like #, %, etc.
                VCDValueChange change;
                change.timestamp = currentTime;
                change.value = value;
                valueChanges[identifier].append(change);
                qDebug() << "Scalar change at time" << currentTime << ":" << value << "->" << identifier;
            }
            continue;
        }

        // Handle vector value changes (binary)
        QRegularExpressionMatch vectorMatch = vectorValueRegex.match(line);
        if (vectorMatch.hasMatch()) {
            QString value = vectorMatch.captured(1);
            QString identifier = vectorMatch.captured(2);

            VCDValueChange change;
            change.timestamp = currentTime;
            change.value = value;
            valueChanges[identifier].append(change);
            // qDebug() << "Vector change at time" << currentTime << ":" << value << "->" << identifier;
            continue;
        }

        // Handle real value changes (if any)
        if (line.startsWith("r")) {
            // Real value change - format: r<value> <identifier>
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString value = parts[0].mid(1); // Remove 'r' prefix
                QString identifier = parts[1];
                
                VCDValueChange change;
                change.timestamp = currentTime;
                change.value = value;
                valueChanges[identifier].append(change);
                qDebug() << "Real change at time" << currentTime << ":" << value << "->" << identifier;
            }
        }
    }

    // Debug: print all signals found
    qDebug() << "=== All parsed signals ===";
    for (const auto& signal : vcdSignals) {
        // qDebug() << "Signal:" << signal.name << "ID:" << signal.identifier 
        //          << "Values:" << valueChanges[signal.identifier].size();
    }

    return true;
}