#include "BinToMatConverter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSysInfo>
#include <QtEndian>
#include <matio.h>
#include <cmath>
#include <vector>
#include <new>
#include <algorithm>
#include <cstring>
#include <type_traits>

BinToMatConverter::BinToMatConverter(QObject *parent)
    : QObject(parent) {
}

BinToMatConverter::DetectionResult BinToMatConverter::autoDetectFormat(const QString &filePath) {
    DetectionResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.reason = "Unable to open target binary file";
        return result;
    }

    const qint64 fileSize = file.size();
    if (fileSize == 0) {
        result.reason = "Target binary file is empty";
        return result;
    }
    result.payloadOffset = 0;
    result.payloadBytes = fileSize;

    QString fileName = QFileInfo(filePath).fileName().toLower();
    if (fileName.contains("uint16") || fileName.contains("u16") || fileName.contains("dem") || fileName.contains("radalt")) {
        result.dataType = DataType::Uint16;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 16-bit Unsigned Integer telemetry payload (Uint16)";
        file.close();
        return result;
    } else if (fileName.contains("int16") || fileName.contains("i16")) {
        result.dataType = DataType::Int16;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 16-bit Signed Integer telemetry payload (Int16)";
        file.close();
        return result;
    } else if (fileName.contains("float") || fileName.contains("32") || fileName.contains("single")) {
        result.dataType = DataType::Single32;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 32-bit Single Precision payload (Float32)";
        file.close();
        return result;
    } else if (fileName.contains("double") || fileName.contains("64")) {
        result.dataType = DataType::Double64;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 64-bit Double Precision payload (Double64)";
        file.close();
        return result;
    }

    const qint64 sampleSize = qMin(fileSize, static_cast<qint64>(4096));
    QByteArray sampleData = file.read(sampleSize);
    file.close();

    auto scoreUint16 = [](const char *data, qint64 size) -> double {
        qint64 count = size / 2;
        if (count == 0) return 0.0;
        
        int validHighByteCount = 0;
        const auto *u8Data = reinterpret_cast<const uint8_t*>(data);
        
        uint8_t baseHighByte = u8Data[1];
        for (qint64 i = 0; i < count; ++i) {
            uint8_t highByte = u8Data[i * 2 + 1];
            if (std::abs(static_cast<int>(highByte) - static_cast<int>(baseHighByte)) <= 2) {
                validHighByteCount++;
            }
        }
        return static_cast<double>(validHighByteCount) / count;
    };

    auto scoreFloat32 = [](const char *data, qint64 size) -> double {
        qint64 count = size / 4;
        if (count == 0) return -1.0;
        int validCount = 0;
        for (qint64 i = 0; i < count; ++i) {
            float val;
            std::memcpy(&val, data + i * 4, 4);
            uint32_t bits;
            std::memcpy(&bits, &val, 4);
            uint32_t exp = (bits >> 23) & 0xFF;

            if (exp >= 115 && exp <= 145) {
                validCount += 3;
            } else if (exp > 0 && exp < 255) {
                validCount += 1;
            }
        }
        return static_cast<double>(validCount) / (count * 3);
    };

    double u16Score = scoreUint16(sampleData.constData(), sampleData.size());
    double f32Score = scoreFloat32(sampleData.constData(), sampleData.size());

    result.endianness = Endianness::LittleEndianMode;

    if (u16Score > 0.70 && u16Score > f32Score) {
        result.dataType = DataType::Uint16;
        result.reason = "Detected 16-bit Unsigned Integer telemetry stream (Uint16)";
    } else if (f32Score > 0.60) {
        result.dataType = DataType::Single32;
        result.reason = "Detected 32-bit Single Precision payload (Float32)";
    } else {
        result.dataType = DataType::Uint16;
        result.reason = "Fallback default: 16-bit Unsigned Integer telemetry (Uint16)";
    }

    return result;
}

bool BinToMatConverter::processFile(const QString &binFilePath, 
                                    const QString &matFilePath, 
                                    const Settings &settings) {
    emit statusLogged(tr("Parsing input file: %1").arg(QFileInfo(binFilePath).fileName()), false);
    emit progressUpdated(0);

    try {
        if (!readAndWriteData(binFilePath, matFilePath, settings)) {
            emit fileConversionCompleted(binFilePath, false);
            return false;
        }
    } catch (const std::exception &e) {
        emit statusLogged(tr("Exception during conversion execution: %1").arg(e.what()), true);
        emit fileConversionCompleted(binFilePath, false);
        return false;
    } catch (...) {
        emit statusLogged(tr("Critical failure encountered during pipeline processing."), true);
        emit fileConversionCompleted(binFilePath, false);
        return false;
    }

    emit progressUpdated(100);
    emit statusLogged(tr("Conversion successfully completed: %1 -> %2")
                          .arg(QFileInfo(binFilePath).fileName(), matFilePath), false);
    emit fileConversionCompleted(binFilePath, true);
    return true;
}

template <typename T>
static inline void swapBufferBytes(T *data, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char *p = reinterpret_cast<char*>(&data[i]);
        std::reverse(p, p + sizeof(T));
    }
}

bool BinToMatConverter::readAndWriteData(const QString &binPath, 
                                         const QString &matPath, 
                                         const Settings &settings) {
    QFile inFile(binPath);
    if (!inFile.open(QIODevice::ReadOnly)) {
        emit statusLogged(tr("Unable to open binary input file: %1").arg(binPath), true);
        return false;
    }

    const qint64 totalFileBytes = inFile.size();
    if (settings.payloadOffset > 0) {
        if (settings.payloadOffset >= totalFileBytes) {
            emit statusLogged(tr("Payload offset (%1 bytes) exceeds total file size (%2 bytes).")
                                  .arg(settings.payloadOffset).arg(totalFileBytes), true);
            inFile.close();
            return false;
        }
        if (!inFile.seek(settings.payloadOffset)) {
            emit statusLogged(tr("Unable to seek to payload offset (%1 bytes).")
                                  .arg(settings.payloadOffset), true);
            inFile.close();
            return false;
        }
    }

    const qint64 totalBytes = totalFileBytes - settings.payloadOffset;
    size_t elementSize = sizeof(uint16_t);
    switch (settings.dataType) {
        case DataType::Double64: elementSize = sizeof(double); break;
        case DataType::Single32: elementSize = sizeof(float); break;
        case DataType::Int64:    elementSize = sizeof(int64_t); break;
        case DataType::Int32:    elementSize = sizeof(int32_t); break;
        case DataType::Uint16:   elementSize = sizeof(uint16_t); break;
        case DataType::Int16:    elementSize = sizeof(int16_t); break;
        case DataType::Uint8:    elementSize = sizeof(uint8_t); break;
        case DataType::Int8:     elementSize = sizeof(int8_t); break;
    }

    if (totalBytes < static_cast<qint64>(elementSize)) {
        emit statusLogged(tr("Payload size (%1 bytes) is smaller than target type width (%2 bytes).")
                              .arg(totalBytes).arg(elementSize), true);
        inFile.close();
        return false;
    }

    qint64 unalignedRemainder = totalBytes % elementSize;
    qint64 usableBytes = totalBytes - unalignedRemainder;

    if (unalignedRemainder != 0) {
        emit statusLogged(tr("Warning: %1 trailing byte(s) are not part of a complete element and will be truncated.")
                              .arg(unalignedRemainder), false);
    }

    const size_t totalElements = usableBytes / elementSize;
    const size_t channels = static_cast<size_t>(qMax(1, settings.channelsCount));

    if (totalElements % channels != 0) {
        emit statusLogged(tr("Total usable elements (%1) do not divide evenly across %2 channels.")
                              .arg(totalElements).arg(channels), true);
        inFile.close();
        return false;
    }

    const size_t totalRecords = totalElements / channels;

    QFileInfo matInfo(matPath);
    QDir matDir = matInfo.dir();
    if (!matDir.exists()) {
        matDir.mkpath(".");
    }

    QString nativeMatPath = QDir::toNativeSeparators(matPath);
    mat_t *matfp = Mat_CreateVer(nativeMatPath.toUtf8().constData(), nullptr, MAT_FT_MAT5);
    if (!matfp) {
        emit statusLogged(tr("MATIO container creation failed at path: %1").arg(nativeMatPath), true);
        inFile.close();
        return false;
    }

    size_t dims[2] = { channels, totalRecords };
    bool success = false;
    QString targetVarName = settings.varName.trimmed().isEmpty() ? "data" : settings.varName.trimmed();

    bool systemIsLittleEndian = (QSysInfo::ByteOrder == QSysInfo::LittleEndian);
    bool fileIsLittleEndian = (settings.endianness == Endianness::LittleEndianMode);
    bool needByteSwap = (systemIsLittleEndian != fileIsLittleEndian);

    auto readAndWriteBuffer = [&](auto typePtr, enum matio_classes classType, enum matio_types dataTypeEnum) -> bool {
        using T = std::remove_pointer_t<decltype(typePtr)>;
        std::vector<T> buffer;
        try {
            buffer.resize(totalElements);
        } catch (const std::bad_alloc &) {
            emit statusLogged(tr("Memory allocation failed for %1 elements.").arg(totalElements), true);
            return false;
        }

        if (inFile.read(reinterpret_cast<char*>(buffer.data()), usableBytes) != usableBytes) {
            emit statusLogged(tr("Failed to read required binary payload bytes.").arg(usableBytes), true);
            return false;
        }

        if (needByteSwap && sizeof(T) > 1) {
            swapBufferBytes(buffer.data(), totalElements);
        }

        matvar_t *var = Mat_VarCreate(targetVarName.toUtf8().constData(), classType, dataTypeEnum, 2, dims, buffer.data(), 0);
        if (var) {
            bool writeOk = (Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE) == 0);
            Mat_VarFree(var);
            return writeOk;
        }
        return false;
    };

    switch (settings.dataType) {
        case DataType::Uint16:
            success = readAndWriteBuffer(static_cast<uint16_t*>(nullptr), MAT_C_UINT16, MAT_T_UINT16);
            break;
        case DataType::Int16:
            success = readAndWriteBuffer(static_cast<int16_t*>(nullptr), MAT_C_INT16, MAT_T_INT16);
            break;
        case DataType::Double64:
            success = readAndWriteBuffer(static_cast<double*>(nullptr), MAT_C_DOUBLE, MAT_T_DOUBLE);
            break;
        case DataType::Single32:
            success = readAndWriteBuffer(static_cast<float*>(nullptr), MAT_C_SINGLE, MAT_T_SINGLE);
            break;
        case DataType::Int32:
            success = readAndWriteBuffer(static_cast<int32_t*>(nullptr), MAT_C_INT32, MAT_T_INT32);
            break;
        case DataType::Int64:
            success = readAndWriteBuffer(static_cast<int64_t*>(nullptr), MAT_C_INT64, MAT_T_INT64);
            break;
        case DataType::Uint8:
            success = readAndWriteBuffer(static_cast<uint8_t*>(nullptr), MAT_C_UINT8, MAT_T_UINT8);
            break;
        case DataType::Int8:
            success = readAndWriteBuffer(static_cast<int8_t*>(nullptr), MAT_C_INT8, MAT_T_INT8);
            break;
    }

    inFile.close();
    Mat_Close(matfp);
    return success;
}