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
        Int32,
        Int64
    };

    enum class Endianness {
        LittleEndianMode,
        BigEndianMode
    };

    struct Settings {
        DataType dataType = DataType::Double64;
        Endianness endianness = Endianness::LittleEndianMode;
        int channelsCount = 1;
        QString varName = "data";
    };

    // Alias for backward compatibility
    using ConversionSettings = Settings;

    explicit BinToMatConverter(QObject *parent = nullptr);
    ~BinToMatConverter() override = default;

    bool processFile(const QString &binFilePath, 
                     const QString &matFilePath, 
                     const Settings &settings = Settings());

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