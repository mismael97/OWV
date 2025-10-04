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



QString VCDParser::generateFullName(const QString &scope, const QString &name)
{
    if (scope.isEmpty()) {
        return name;
    }
    return scope + "." + name;
}

bool VCDParser::parseFile(const QString &filename)
{
    // For now, use header-only parsing for performance
    return parseHeaderOnly(filename);
}

bool VCDParser::parseHeaderOnly(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorString = "Cannot open file: " + filename;
        return false;
    }

    vcdFilename = filename;
    QTextStream stream(&file);
    
    vcdSignals.clear();
    identifierMap.clear();
    fullNameMap.clear();  // ADD THIS
    currentScope.clear();
    valueChanges.clear();
    loadedSignals.clear();
    endTime = 0;

    if (!parseHeader(stream)) {
        file.close();
        return false;
    }

    file.close();

    qDebug() << "VCD header parsing completed";
    qDebug() << "Signals found:" << vcdSignals.size();
    qDebug() << "Unique identifiers:" << identifierMap.size();
    qDebug() << "Unique full names:" << fullNameMap.size();

    return true;
}

bool VCDParser::parseHeader(QTextStream &stream)
{
    QRegularExpression scopeRegex("^\\$scope\\s+(\\w+)\\s+(\\S+)\\s*\\$end$");
    QRegularExpression varRegex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(.+)\\s*\\$end$");
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
        else if (line.startsWith("#")) {
            // We reached the value change section, stop header parsing
            break;
        }
    }

    return true;
}


// Update the value change functions to use fullName
bool VCDParser::loadSignalsData(const QList<QString> &fullNames)
{
    if (fullNames.isEmpty()) {
        return true;
    }

    QFile file(vcdFilename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorString = "Cannot open file for signal loading: " + vcdFilename;
        return false;
    }

    QTextStream stream(&file);
    
    // Convert to set for fast lookup - but we need to map fullNames back to identifiers
    QSet<QString> signalsToLoad;
    for (const QString &fullName : fullNames) {
        if (fullNameMap.contains(fullName) && !loadedSignals.contains(fullName)) {
            QString identifier = fullNameMap[fullName].identifier;
            signalsToLoad.insert(identifier);
            // Initialize empty value changes using fullName as key
            valueChanges[fullName] = QVector<VCDValueChange>();
        }
    }

    if (signalsToLoad.isEmpty()) {
        file.close();
        return true; // All signals already loaded
    }

    qDebug() << "Loading data for" << signalsToLoad.size() << "signals";

    if (!parseValueChangesForSignals(stream, signalsToLoad)) {
        file.close();
        return false;
    }

    file.close();

    // Mark signals as loaded using fullName
    for (const QString &fullName : fullNames) {
        if (fullNameMap.contains(fullName)) {
            loadedSignals.insert(fullName);
        }
    }

    qDebug() << "Successfully loaded data for" << signalsToLoad.size() << "signals";
    return true;
}


bool VCDParser::parseValueChangesForSignals(QTextStream &stream, const QSet<QString> &signalsToLoad)
{
    QRegularExpression timestampRegex("^#(\\d+)$");
    QRegularExpression valueChangeRegex("^([01xXzZrb])(\\S+)$");
    QRegularExpression vectorValueRegex("^[bB]([01xXzZ]+)\\s+(\\S+)$");

    int currentTime = 0;
    int changesFound = 0;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty()) continue;

        // Check for timestamp
        QRegularExpressionMatch timestampMatch = timestampRegex.match(line);
        if (timestampMatch.hasMatch()) {
            currentTime = timestampMatch.captured(1).toInt();
            endTime = qMax(endTime, currentTime);
            continue;
        }

        // Handle scalar value changes (0, 1, x, z)
        QRegularExpressionMatch valueMatch = valueChangeRegex.match(line);
        if (valueMatch.hasMatch()) {
            QString value = valueMatch.captured(1).toUpper();
            QString identifier = valueMatch.captured(2);

            if (signalsToLoad.contains(identifier)) {
                // Find the fullName for this identifier
                QString fullName;
                for (const auto &signal : vcdSignals) {
                    if (signal.identifier == identifier) {
                        fullName = signal.fullName;
                        break;
                    }
                }
                
                if (!fullName.isEmpty()) {
                    VCDValueChange change;
                    change.timestamp = currentTime;
                    change.value = value;
                    valueChanges[fullName].append(change);
                    changesFound++;
                }
            }
            continue;
        }

        // Handle vector value changes (binary)
        QRegularExpressionMatch vectorMatch = vectorValueRegex.match(line);
        if (vectorMatch.hasMatch()) {
            QString value = vectorMatch.captured(1);
            QString identifier = vectorMatch.captured(2);

            if (signalsToLoad.contains(identifier)) {
                // Find the fullName for this identifier
                QString fullName;
                for (const auto &signal : vcdSignals) {
                    if (signal.identifier == identifier) {
                        fullName = signal.fullName;
                        break;
                    }
                }
                
                if (!fullName.isEmpty()) {
                    VCDValueChange change;
                    change.timestamp = currentTime;
                    change.value = value;
                    valueChanges[fullName].append(change);
                    changesFound++;
                }
            }
            continue;
        }

        // Handle real value changes
        if (line.startsWith("r")) {
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QString value = parts[0].mid(1); // Remove 'r' prefix
                QString identifier = parts[1];
                
                if (signalsToLoad.contains(identifier)) {
                    // Find the fullName for this identifier
                    QString fullName;
                    for (const auto &signal : vcdSignals) {
                        if (signal.identifier == identifier) {
                            fullName = signal.fullName;
                            break;
                        }
                    }
                    
                    if (!fullName.isEmpty()) {
                        VCDValueChange change;
                        change.timestamp = currentTime;
                        change.value = value;
                        valueChanges[fullName].append(change);
                        changesFound++;
                    }
                }
            }
        }
    }

    qDebug() << "Found" << changesFound << "value changes for requested signals";
    return true;
}

QVector<VCDValueChange> VCDParser::getValueChangesForSignal(const QString &fullName)
{
    // If signal data is not loaded yet, load it now
    if (!loadedSignals.contains(fullName)) {
        QList<QString> signalsToLoad = {fullName};
        loadSignalsData(signalsToLoad);
    }
    
    return valueChanges.value(fullName);
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
    }
}

void VCDParser::parseVarLine(const QString &line)
{
    QRegularExpression regex("^\\$var\\s+(\\w+)\\s+(\\d+)\\s+(\\S+)\\s+(.+)\\s*\\$end$");
    QRegularExpressionMatch match = regex.match(line);
    
    if (match.hasMatch()) {
        VCDSignal signal;
        signal.type = match.captured(1);
        signal.width = match.captured(2).toInt();
        signal.identifier = match.captured(3);
        
        QString signalName = match.captured(4).trimmed();
        signal.name = signalName;
        signal.scope = currentScope;
        signal.fullName = generateFullName(currentScope, signalName);

        vcdSignals.append(signal);
        
        // Store in both maps
        identifierMap[signal.identifier] = signal;
        fullNameMap[signal.fullName] = signal;
        
        // qDebug() << "Parsed signal - Full:" << signal.fullName 
        //          << "ID:" << signal.identifier 
        //          << "Scope:" << signal.scope 
        //          << "Name:" << signal.name;
    }
}