#ifndef BIN_TO_MAT_CONVERTER_H
#define BIN_TO_MAT_CONVERTER_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QDataStream>
#include <cstdint>

class BinToMatConverter : public QObject {
    Q_OBJECT

public:
    enum class DataType {
        Double64,
        Single32,
        Int64,
        Uint64,
        Int32,
        Uint32,
        Uint16,
        Int16,
        Uint8,
        Int8
    };

    enum class Endianness {
        LittleEndianMode,
        BigEndianMode
    };

    struct Settings {
        DataType dataType = DataType::Uint16;
        Endianness endianness = Endianness::LittleEndianMode;
        int channelsCount = 1;
        QString varName = "data";
        qint64 payloadOffset = 0;
    };

    struct DetectionResult {
        DataType dataType = DataType::Uint16;
        Endianness endianness = Endianness::LittleEndianMode;
        qint64 payloadOffset = 0;
        qint64 payloadBytes = 0;
        quint64 matrixRows = 0;
        quint64 matrixCols = 0;
        double confidenceScore = 0.0;
        double matrixConfidence = 0.0;
        QString reason;
    };

    // Alias for backward compatibility
    using ConversionSettings = Settings;

    explicit BinToMatConverter(QObject *parent = nullptr);
    ~BinToMatConverter() override = default;

    bool processFile(const QString &binFilePath, 
                     const QString &matFilePath, 
                     const Settings &settings = Settings());

    static DetectionResult autoDetectFormat(const QString &filePath);

signals:
    void progressUpdated(int percentage);
    void statusLogged(const QString &message, bool isError);
    void fileConversionCompleted(const QString &binFilePath, bool success);

private:
    bool readAndWriteData(const QString &binPath, 
                          const QString &matPath, 
                          const Settings &settings);
};

#endif // BIN_TO_MAT_CONVERTER_H