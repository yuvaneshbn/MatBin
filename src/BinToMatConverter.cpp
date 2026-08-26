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

BinToMatConverter::BinToMatConverter(QObject *parent)
    : QObject(parent) {
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

        for (size_t i = 0; i < totalElements; ++i) {
            if (std::isnan(buffer[i])) {
                emit statusLogged(tr("Corrupt floating-point element (NaN) at index %1").arg(i), true);
                inFile.close(); Mat_Close(matfp); return false;
            }
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

        for (size_t i = 0; i < totalElements; ++i) {
            if (std::isnan(buffer[i])) {
                emit statusLogged(tr("Corrupt floating-point element (NaN) at index %1").arg(i), true);
                inFile.close(); Mat_Close(matfp); return false;
            }
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