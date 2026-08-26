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

BinToMatConverter::BinToMatConverter(QObject *parent)
    : QObject(parent) {
}

BinToMatConverter::DetectionResult BinToMatConverter::autoDetectFormat(const QString &filePath) {
    DetectionResult result;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.reason = "Unable to open file";
        return result;
    }

    const qint64 fileSize = file.size();
    if (fileSize == 0) {
        result.reason = "File is empty";
        return result;
    }

    // 1. Filename Heuristic Check (Explicit name patterns: float/32/single vs double/64)
    QString fileName = QFileInfo(filePath).fileName().toLower();
    if (fileName.contains("float") || fileName.contains("32") || fileName.contains("single")) {
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
    } else if (fileName.contains("int32")) {
        result.dataType = DataType::Int32;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 32-bit Signed Integer payload (Int32)";
        file.close();
        return result;
    } else if (fileName.contains("int64")) {
        result.dataType = DataType::Int64;
        result.endianness = Endianness::LittleEndianMode;
        result.reason = "Detected 64-bit Signed Integer payload (Int64)";
        file.close();
        return result;
    }

    // 2. Data Payload Scoring for un-named binary files
    const qint64 sampleSize = qMin(fileSize, static_cast<qint64>(4096));
    QByteArray sampleData = file.read(sampleSize);
    file.close();

    auto scoreFloat32 = [](const char *data, qint64 size, bool swapBytes) -> double {
        qint64 count = size / 4;
        if (count == 0) return -1.0;
        int validCount = 0;
        for (qint64 i = 0; i < count; ++i) {
            float val;
            char p[4];
            if (swapBytes) {
                p[0] = data[i * 4 + 3]; p[1] = data[i * 4 + 2];
                p[2] = data[i * 4 + 1]; p[3] = data[i * 4 + 0];
            } else {
                std::memcpy(p, data + i * 4, 4);
            }
            std::memcpy(&val, p, 4);

            uint32_t bits;
            std::memcpy(&bits, &val, 4);
            uint32_t exp = (bits >> 23) & 0xFF;

            if (std::isnan(val) || std::isinf(val)) {
                validCount += 1;
            } else if (exp >= 115 && exp <= 145) {
                validCount += 3;
            } else if (exp > 0 && exp < 255) {
                validCount += 2;
            }
        }
        return static_cast<double>(validCount) / count;
    };

    auto scoreFloat64 = [](const char *data, qint64 size, bool swapBytes) -> double {
        qint64 count = size / 8;
        if (count == 0) return -1.0;
        int validCount = 0;
        for (qint64 i = 0; i < count; ++i) {
            double val;
            char p[8];
            if (swapBytes) {
                for (int j = 0; j < 8; ++j) p[j] = data[i * 8 + 7 - j];
            } else {
                std::memcpy(p, data + i * 8, 8);
            }
            std::memcpy(&val, p, 8);

            uint64_t bits;
            std::memcpy(&bits, &val, 8);
            uint64_t exp = (bits >> 52) & 0x7FF;

            if (std::isnan(val) || std::isinf(val)) {
                validCount += 1;
            } else if (exp >= 980 && exp <= 1045) {
                validCount += 3;
            } else if (exp > 0 && exp < 2047) {
                validCount += 2;
            }
        }
        return static_cast<double>(validCount) / count;
    };

    bool sysIsLittle = (QSysInfo::ByteOrder == QSysInfo::LittleEndian);

    double f32NativeScore = scoreFloat32(sampleData.constData(), sampleData.size(), false);
    double f32SwappedScore = scoreFloat32(sampleData.constData(), sampleData.size(), true);
    double f64NativeScore = scoreFloat64(sampleData.constData(), sampleData.size(), false);
    double f64SwappedScore = scoreFloat64(sampleData.constData(), sampleData.size(), true);

    bool preferLittle = sysIsLittle;
    if (f32SwappedScore > f32NativeScore + 0.5 || f64SwappedScore > f64NativeScore + 0.5) {
        preferLittle = !sysIsLittle;
    }

    result.endianness = preferLittle ? Endianness::LittleEndianMode : Endianness::BigEndianMode;

    if (f32NativeScore >= f64NativeScore) {
        result.dataType = DataType::Single32;
        result.reason = "Detected 32-bit Single Precision payload (Float32)";
    } else {
        result.dataType = DataType::Double64;
        result.reason = "Detected 64-bit Double Precision payload (Double64)";
    }

    return result;
}

bool BinToMatConverter::processFile(const QString &binFilePath, 
                                    const QString &matFilePath, 
                                    const Settings &settings) {
    emit statusLogged(tr("Parsing file: %1").arg(QFileInfo(binFilePath).fileName()), false);
    emit progressUpdated(0);

    try {
        if (!readAndWriteData(binFilePath, matFilePath, settings)) {
            emit fileConversionCompleted(binFilePath, false);
            return false;
        }
    } catch (const std::exception &e) {
        emit statusLogged(tr("Exception during conversion: %1").arg(e.what()), true);
        emit fileConversionCompleted(binFilePath, false);
        return false;
    } catch (...) {
        emit statusLogged(tr("Unknown critical failure during file processing."), true);
        emit fileConversionCompleted(binFilePath, false);
        return false;
    }

    emit progressUpdated(100);
    emit statusLogged(tr("Conversion finished: %1 -> %2")
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

    const qint64 totalBytes = inFile.size();
    size_t elementSize = sizeof(double);
    switch (settings.dataType) {
        case DataType::Double64: elementSize = sizeof(double); break;
        case DataType::Single32: elementSize = sizeof(float); break;
        case DataType::Int32:    elementSize = sizeof(int32_t); break;
        case DataType::Int64:    elementSize = sizeof(int64_t); break;
    }

    if (totalBytes == 0 || (totalBytes % elementSize != 0)) {
        emit statusLogged(tr("File size (%1 bytes) is unaligned with target type size (%2 bytes).")
                              .arg(totalBytes).arg(elementSize), true);
        inFile.close();
        return false;
    }

    const size_t totalElements = totalBytes / elementSize;
    const size_t channels = static_cast<size_t>(qMax(1, settings.channelsCount));

    if (totalElements % channels != 0) {
        emit statusLogged(tr("Total elements (%1) do not divide evenly across %2 channels.")
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
        emit statusLogged(tr("MATIO container creation failed for path: %1").arg(nativeMatPath), true);
        inFile.close();
        return false;
    }

    size_t dims[2] = { channels, totalRecords };
    bool success = false;
    QString targetVarName = settings.varName.trimmed().isEmpty() ? "data" : settings.varName.trimmed();

    bool systemIsLittleEndian = (QSysInfo::ByteOrder == QSysInfo::LittleEndian);
    bool fileIsLittleEndian = (settings.endianness == Endianness::LittleEndianMode);
    bool needByteSwap = (systemIsLittleEndian != fileIsLittleEndian);

    if (settings.dataType == DataType::Double64) {
        std::vector<double> buffer;
        try {
            buffer.resize(totalElements);
        } catch (const std::bad_alloc &) {
            emit statusLogged(tr("Memory allocation failed for %1 elements.").arg(totalElements), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (inFile.read(reinterpret_cast<char*>(buffer.data()), totalBytes) != totalBytes) {
            emit statusLogged(tr("Failed to read file payload."), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (needByteSwap) {
            swapBufferBytes(buffer.data(), totalElements);
        }

        matvar_t *var = Mat_VarCreate(targetVarName.toUtf8().constData(), MAT_C_DOUBLE, MAT_T_DOUBLE, 2, dims, buffer.data(), 0);
        if (var) {
            success = (Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE) == 0);
            Mat_VarFree(var);
        }

    } else if (settings.dataType == DataType::Single32) {
        std::vector<float> buffer;
        try {
            buffer.resize(totalElements);
        } catch (const std::bad_alloc &) {
            emit statusLogged(tr("Memory allocation failed for %1 elements.").arg(totalElements), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (inFile.read(reinterpret_cast<char*>(buffer.data()), totalBytes) != totalBytes) {
            emit statusLogged(tr("Failed to read file payload."), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (needByteSwap) {
            swapBufferBytes(buffer.data(), totalElements);
        }

        matvar_t *var = Mat_VarCreate(targetVarName.toUtf8().constData(), MAT_C_SINGLE, MAT_T_SINGLE, 2, dims, buffer.data(), 0);
        if (var) {
            success = (Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE) == 0);
            Mat_VarFree(var);
        }

    } else if (settings.dataType == DataType::Int32) {
        std::vector<int32_t> buffer;
        try {
            buffer.resize(totalElements);
        } catch (const std::bad_alloc &) {
            emit statusLogged(tr("Memory allocation failed for %1 elements.").arg(totalElements), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (inFile.read(reinterpret_cast<char*>(buffer.data()), totalBytes) != totalBytes) {
            emit statusLogged(tr("Failed to read file payload."), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (needByteSwap) {
            swapBufferBytes(buffer.data(), totalElements);
        }

        matvar_t *var = Mat_VarCreate(targetVarName.toUtf8().constData(), MAT_C_INT32, MAT_T_INT32, 2, dims, buffer.data(), 0);
        if (var) {
            success = (Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE) == 0);
            Mat_VarFree(var);
        }

    } else if (settings.dataType == DataType::Int64) {
        std::vector<int64_t> buffer;
        try {
            buffer.resize(totalElements);
        } catch (const std::bad_alloc &) {
            emit statusLogged(tr("Memory allocation failed for %1 elements.").arg(totalElements), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (inFile.read(reinterpret_cast<char*>(buffer.data()), totalBytes) != totalBytes) {
            emit statusLogged(tr("Failed to read file payload."), true);
            inFile.close(); Mat_Close(matfp); return false;
        }

        if (needByteSwap) {
            swapBufferBytes(buffer.data(), totalElements);
        }

        matvar_t *var = Mat_VarCreate(targetVarName.toUtf8().constData(), MAT_C_INT64, MAT_T_INT64, 2, dims, buffer.data(), 0);
        if (var) {
            success = (Mat_VarWrite(matfp, var, MAT_COMPRESSION_NONE) == 0);
            Mat_VarFree(var);
        }
    }

    inFile.close();
    Mat_Close(matfp);
    return success;
}