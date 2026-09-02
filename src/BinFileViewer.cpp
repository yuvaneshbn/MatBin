#include "BinFileViewer.h"

#include <QAbstractTableModel>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFontDatabase>
#include <QLabel>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QScreen>
#include <QGuiApplication>
#include <QtEndian>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

static QString getDataTypeTitle(BinToMatConverter::DataType type) {
    switch (type) {
    case BinToMatConverter::DataType::Double64: return "Double64 (8 bytes)";
    case BinToMatConverter::DataType::Single32: return "Single32 (4 bytes)";
    case BinToMatConverter::DataType::Int64:    return "Int64 (8 bytes)";
    case BinToMatConverter::DataType::Uint64:   return "Uint64 (8 bytes)";
    case BinToMatConverter::DataType::Int32:    return "Int32 (4 bytes)";
    case BinToMatConverter::DataType::Uint32:   return "Uint32 (4 bytes)";
    case BinToMatConverter::DataType::Uint16:   return "Uint16 (2 bytes)";
    case BinToMatConverter::DataType::Int16:    return "Int16 (2 bytes)";
    case BinToMatConverter::DataType::Uint8:    return "Uint8 (1 byte)";
    case BinToMatConverter::DataType::Int8:     return "Int8 (1 byte)";
    }
    return "Unknown";
}

static QString getEndianTitle(BinToMatConverter::Endianness endian) {
    return endian == BinToMatConverter::Endianness::LittleEndianMode ? "Little-Endian" : "Big-Endian";
}

namespace {

class BinMatrixModel : public QAbstractTableModel {
public:
    BinMatrixModel(const QByteArray &bytes,
                   BinToMatConverter::DataType type,
                   BinToMatConverter::Endianness endian,
                   quint64 rows,
                   quint64 cols,
                   qint64 payloadOffset,
                   QObject *parent = nullptr)
        : QAbstractTableModel(parent),
          m_bytes(bytes),
          m_type(type),
          m_endian(endian),
          m_rows(rows),
          m_cols(cols),
          m_payloadOffset(payloadOffset) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return static_cast<int>(qMin<quint64>(m_rows, std::numeric_limits<int>::max()));
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid()) return 0;
        return static_cast<int>(qMin<quint64>(m_cols, std::numeric_limits<int>::max()));
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        if (role != Qt::DisplayRole) return {};
        if (orientation == Qt::Horizontal) {
            return QString("C%1").arg(section + 1);
        } else {
            return QString("R%1").arg(section + 1);
        }
    }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) return {};

        const quint64 row = static_cast<quint64>(index.row());
        const quint64 col = static_cast<quint64>(index.column());
        if (row >= m_rows || col >= m_cols) return {};

        const size_t elemSize = elementByteSize();
        const quint64 byteOffset = static_cast<quint64>(m_payloadOffset) + (row * m_cols + col) * elemSize;
        if (byteOffset + elemSize > static_cast<quint64>(m_bytes.size())) return {};

        const char *p = m_bytes.constData() + static_cast<qsizetype>(byteOffset);
        return valueString(p);
    }

private:
    size_t elementByteSize() const {
        switch (m_type) {
        case BinToMatConverter::DataType::Double64: return sizeof(double);
        case BinToMatConverter::DataType::Single32: return sizeof(float);
        case BinToMatConverter::DataType::Int64:    return sizeof(qint64);
        case BinToMatConverter::DataType::Uint64:   return sizeof(quint64);
        case BinToMatConverter::DataType::Int32:    return sizeof(qint32);
        case BinToMatConverter::DataType::Uint32:   return sizeof(quint32);
        case BinToMatConverter::DataType::Uint16:   return sizeof(quint16);
        case BinToMatConverter::DataType::Int16:    return sizeof(qint16);
        case BinToMatConverter::DataType::Uint8:    return sizeof(quint8);
        case BinToMatConverter::DataType::Int8:     return sizeof(qint8);
        }
        return 1;
    }

    QString valueString(const char *p) const {
        switch (m_type) {
        case BinToMatConverter::DataType::Uint8:
            return QString::number(static_cast<quint8>(p[0]));
        case BinToMatConverter::DataType::Int8:
            return QString::number(static_cast<qint8>(p[0]));
        case BinToMatConverter::DataType::Uint16: {
            quint16 v; std::memcpy(&v, p, sizeof(v));
            return QString::number(m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v));
        }
        case BinToMatConverter::DataType::Int16: {
            quint16 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(static_cast<qint16>(raw));
        }
        case BinToMatConverter::DataType::Uint32: {
            quint32 v; std::memcpy(&v, p, sizeof(v));
            return QString::number(m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v));
        }
        case BinToMatConverter::DataType::Int32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(static_cast<qint32>(raw));
        }
        case BinToMatConverter::DataType::Uint64: {
            quint64 v; std::memcpy(&v, p, sizeof(v));
            return QString::number(m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v));
        }
        case BinToMatConverter::DataType::Int64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(static_cast<qint64>(raw));
        }
        case BinToMatConverter::DataType::Single32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            float val; std::memcpy(&val, &raw, sizeof(val));
            return QString::number(val, 'g', 9);
        }
        case BinToMatConverter::DataType::Double64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            double val; std::memcpy(&val, &raw, sizeof(val));
            return QString::number(val, 'g', 17);
        }
        }
        return {};
    }

    const QByteArray &m_bytes;
    BinToMatConverter::DataType m_type;
    BinToMatConverter::Endianness m_endian;
    quint64 m_rows = 0;
    quint64 m_cols = 0;
    qint64 m_payloadOffset = 0;
};

class BinHexWidget : public QWidget {
public:
    explicit BinHexWidget(const QByteArray &bytes, QWidget *parent = nullptr)
        : QWidget(parent), m_bytes(bytes) {
        auto *layout = new QVBoxLayout(this);
        auto *controls = new QHBoxLayout();

        m_offsetLabel = new QLabel(this);
        m_prev = new QPushButton(tr("Previous"), this);
        m_next = new QPushButton(tr("Next"), this);
        m_pageSpin = new QSpinBox(this);
        m_pageSpin->setMinimum(0);
        m_pageSpin->setMaximum(pageCount() > 0 ? pageCount() - 1 : 0);

        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        m_text->setLineWrapMode(QPlainTextEdit::NoWrap);
        m_text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        m_text->setStyleSheet("QPlainTextEdit { font-family: 'Consolas', 'Courier New', monospace; font-size: 11px; background-color: #0f172a; color: #38bdf8; padding: 8px; }");

        controls->addWidget(m_prev);
        controls->addWidget(m_next);
        controls->addWidget(new QLabel(tr("Page:"), this));
        controls->addWidget(m_pageSpin);
        controls->addStretch();
        controls->addWidget(m_offsetLabel);

        layout->addLayout(controls);
        layout->addWidget(m_text, 1);

        connect(m_prev, &QPushButton::clicked, this, [this]() {
            m_pageSpin->setValue(m_pageSpin->value() - 1);
        });
        connect(m_next, &QPushButton::clicked, this, [this]() {
            m_pageSpin->setValue(m_pageSpin->value() + 1);
        });
        connect(m_pageSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { refresh(); });

        refresh();
    }

private:
    int pageCount() const {
        if (m_bytes.isEmpty()) return 0;
        return static_cast<int>((m_bytes.size() + m_pageBytes - 1) / m_pageBytes);
    }

    void refresh() {
        if (m_bytes.isEmpty()) {
            m_text->clear();
            m_offsetLabel->setText(tr("No binary payload bytes"));
            return;
        }

        const size_t totalBytes = static_cast<size_t>(m_bytes.size());
        const size_t start = static_cast<size_t>(m_pageSpin->value()) * m_pageBytes;
        const size_t count = std::min(m_pageBytes, totalBytes - start);
        const auto *bytes = reinterpret_cast<const unsigned char*>(m_bytes.constData()) + start;

        QString out;
        out.reserve(static_cast<int>(count * 4 + 200));

        out += QString("Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
        out += QString("--------------------------------------------------------------------------------\n");

        for (size_t line = 0; line < count; line += 16) {
            const size_t lineCount = std::min<size_t>(16, count - line);
            out += QString("%1  ").arg(static_cast<qulonglong>(start + line), 8, 16, QLatin1Char('0')).toUpper();

            for (size_t i = 0; i < 16; ++i) {
                if (i < lineCount) {
                    out += QString("%1 ").arg(static_cast<unsigned int>(bytes[line + i]), 2, 16, QLatin1Char('0')).toUpper();
                } else {
                    out += "   ";
                }
                if (i == 7) out += ' ';
            }
            out += " |";
            for (size_t i = 0; i < lineCount; ++i) {
                const unsigned char c = bytes[line + i];
                out += (c >= 32 && c <= 126) ? QChar(static_cast<ushort>(c)) : QChar('.');
            }
            out += "|\n";
        }

        m_text->setPlainText(out);
        m_offsetLabel->setText(tr("Bytes %1 – %2 of %3  (Page %4 of %5)")
            .arg(static_cast<qulonglong>(start))
            .arg(static_cast<qulonglong>(start + count - 1))
            .arg(static_cast<qulonglong>(totalBytes))
            .arg(m_pageSpin->value() + 1)
            .arg(pageCount()));

        m_prev->setEnabled(m_pageSpin->value() > 0);
        m_next->setEnabled(m_pageSpin->value() + 1 < pageCount());
    }

    static constexpr size_t m_pageBytes = 4096;
    const QByteArray &m_bytes;
    QPlainTextEdit *m_text = nullptr;
    QSpinBox *m_pageSpin = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QLabel *m_offsetLabel = nullptr;
};

class BinHeatmapWidget : public QWidget {
public:
    BinHeatmapWidget(const QByteArray &bytes,
                     BinToMatConverter::DataType type,
                     BinToMatConverter::Endianness endian,
                     quint64 rows,
                     quint64 cols,
                     qint64 payloadOffset,
                     QWidget *parent = nullptr)
        : QWidget(parent),
          m_bytes(bytes),
          m_type(type),
          m_endian(endian),
          m_rows(rows),
          m_cols(cols),
          m_payloadOffset(payloadOffset) {
        setMinimumSize(720, 500);
        calculateRange();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        if (m_rows == 0 || m_cols == 0 || !std::isfinite(m_min) || !std::isfinite(m_max)) return;

        const int legendWidth = 270;
        const QRect plotRect(55, 25, qMax(50, width() - legendWidth - 90), qMax(50, height() - 80));
        QImage image(plotRect.size(), QImage::Format_RGB32);

        const double range = (m_max > m_min) ? (m_max - m_min) : 1.0;
        for (int y = 0; y < image.height(); ++y) {
            const quint64 row = qMin<quint64>(m_rows - 1, static_cast<quint64>(y) * m_rows / image.height());
            for (int x = 0; x < image.width(); ++x) {
                const quint64 col = qMin<quint64>(m_cols - 1, static_cast<quint64>(x) * m_cols / image.width());
                double v = valueAt(row, col);
                if (!std::isfinite(v)) v = m_min;
                const double n = qBound(0.0, (v - m_min) / range, 1.0);
                image.setPixel(x, y, colorFor(n));
            }
        }

        p.setPen(QColor(180, 190, 200));
        p.drawRect(plotRect);
        p.drawImage(plotRect.topLeft(), image);

        const int lx = plotRect.right() + 20;
        const int ly = plotRect.top();
        const int legendBoxHeight = 310;

        // Draw Legend Container Box
        p.setBrush(QColor(248, 250, 252));
        p.setPen(QColor(203, 213, 225));
        p.drawRoundedRect(QRect(lx, ly, legendWidth + 50, legendBoxHeight), 6, 6);

        p.setFont(QFont(p.font().family(), 10, QFont::Bold));
        p.setPen(QColor(30, 41, 59));
        p.drawText(lx + 12, ly + 22, tr("Heatmap Color Legend"));
        p.setFont(QFont(p.font().family(), 8));

        const struct Entry { 
            QColor c; 
            QString label; 
            QString meaning; 
        } entries[] = {
            {QColor(colorFor(0.10)), tr("Deep Blue / Indigo"), tr("Minimum values — lowest range")},
            {QColor(colorFor(0.30)), tr("Cyan / Teal"), tr("Lower mid-range values")},
            {QColor(colorFor(0.50)), tr("Green / Emerald"), tr("Mid-range / baseline values")},
            {QColor(colorFor(0.70)), tr("Yellow / Amber"), tr("Upper mid-range values")},
            {QColor(colorFor(0.90)), tr("Bright Red / Crimson"), tr("Maximum values — highest range")}
        };

        for (int i = 0; i < 5; ++i) {
            const double lo = m_min + (m_max - m_min) * (static_cast<double>(i) / 5.0);
            const double hi = (i == 4) ? m_max : m_min + (m_max - m_min) * (static_cast<double>(i + 1) / 5.0);
            const int itemY = ly + 36 + i * 52;

            // Color swatch
            p.setBrush(entries[i].c);
            p.setPen(QColor(100, 116, 139));
            p.drawRoundedRect(QRect(lx + 12, itemY, 20, 20), 3, 3);

            // Name & meaning
            p.setFont(QFont(p.font().family(), 8, QFont::Bold));
            p.setPen(QColor(15, 23, 42));
            p.drawText(lx + 38, itemY + 11, entries[i].label);

            p.setFont(QFont(p.font().family(), 7));
            p.setPen(QColor(100, 116, 139));
            p.drawText(lx + 38, itemY + 23, entries[i].meaning);

            // Dynamic Numeric Range
            p.setFont(QFont(p.font().family(), 8, QFont::Bold));
            p.setPen(QColor(13, 71, 161));
            p.drawText(lx + 38, itemY + 36, QString("Range: %1 – %2").arg(lo, 0, 'g', 6).arg(hi, 0, 'g', 6));
        }

        p.setFont(QFont(p.font().family(), 9));
        p.setPen(QColor(51, 65, 85));
        p.drawText(55, height() - 25,
                   QString("Matrix Size: %1 × %2    |    Min Value: %3    |    Max Value: %4")
                   .arg(m_rows).arg(m_cols)
                   .arg(m_min, 0, 'g', 10).arg(m_max, 0, 'g', 10));
    }

private:
    static inline QRgb colorFor(double n) {
        const double clamped = qBound(0.0, n, 1.0);
        const int r = static_cast<int>(255.0 * clamped);
        const int g = static_cast<int>(255.0 * std::sin(clamped * 3.14159265358979323846));
        const int b = static_cast<int>(255.0 * (1.0 - clamped));
        return qRgb(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255));
    }

    size_t elementByteSize() const {
        switch (m_type) {
        case BinToMatConverter::DataType::Double64: return sizeof(double);
        case BinToMatConverter::DataType::Single32: return sizeof(float);
        case BinToMatConverter::DataType::Int64:    return sizeof(qint64);
        case BinToMatConverter::DataType::Uint64:   return sizeof(quint64);
        case BinToMatConverter::DataType::Int32:    return sizeof(qint32);
        case BinToMatConverter::DataType::Uint32:   return sizeof(quint32);
        case BinToMatConverter::DataType::Uint16:   return sizeof(quint16);
        case BinToMatConverter::DataType::Int16:    return sizeof(qint16);
        case BinToMatConverter::DataType::Uint8:    return sizeof(quint8);
        case BinToMatConverter::DataType::Int8:     return sizeof(qint8);
        }
        return 1;
    }

    double valueAt(quint64 row, quint64 col) const {
        const size_t elemSize = elementByteSize();
        const quint64 byteOffset = static_cast<quint64>(m_payloadOffset) + (row * m_cols + col) * elemSize;
        if (byteOffset + elemSize > static_cast<quint64>(m_bytes.size())) return std::numeric_limits<double>::quiet_NaN();

        const char *p = m_bytes.constData() + static_cast<qsizetype>(byteOffset);
        switch (m_type) {
        case BinToMatConverter::DataType::Uint8:  return static_cast<quint8>(p[0]);
        case BinToMatConverter::DataType::Int8:   return static_cast<qint8>(p[0]);
        case BinToMatConverter::DataType::Uint16: {
            quint16 v; std::memcpy(&v, p, sizeof(v));
            return m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v);
        }
        case BinToMatConverter::DataType::Int16: {
            quint16 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return static_cast<qint16>(raw);
        }
        case BinToMatConverter::DataType::Uint32: {
            quint32 v; std::memcpy(&v, p, sizeof(v));
            return m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v);
        }
        case BinToMatConverter::DataType::Int32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return static_cast<qint32>(raw);
        }
        case BinToMatConverter::DataType::Uint64: {
            quint64 v; std::memcpy(&v, p, sizeof(v));
            return static_cast<double>(m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v));
        }
        case BinToMatConverter::DataType::Int64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return static_cast<double>(static_cast<qint64>(raw));
        }
        case BinToMatConverter::DataType::Single32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            float val; std::memcpy(&val, &raw, sizeof(val));
            return static_cast<double>(val);
        }
        case BinToMatConverter::DataType::Double64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            double val; std::memcpy(&val, &raw, sizeof(val));
            return val;
        }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    void calculateRange() {
        if (m_rows == 0 || m_cols == 0) return;
        const quint64 total = m_rows * m_cols;
        m_min = std::numeric_limits<double>::infinity();
        m_max = -std::numeric_limits<double>::infinity();

        // Sample up to 1,000,000 values evenly for large datasets
        const quint64 step = qMax<quint64>(1, total / 1000000ULL);
        for (quint64 i = 0; i < total; i += step) {
            const double v = valueAt(i / m_cols, i % m_cols);
            if (std::isfinite(v)) {
                m_min = qMin(m_min, v);
                m_max = qMax(m_max, v);
            }
        }
    }

    const QByteArray &m_bytes;
    BinToMatConverter::DataType m_type;
    BinToMatConverter::Endianness m_endian;
    quint64 m_rows = 0;
    quint64 m_cols = 0;
    qint64 m_payloadOffset = 0;
    double m_min = std::numeric_limits<double>::quiet_NaN();
    double m_max = std::numeric_limits<double>::quiet_NaN();
};

} // namespace

BinFileViewerDialog::BinFileViewerDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle(tr("BIN Content Viewer - %1").arg(QFileInfo(filePath).fileName()));
    setWindowIcon(QIcon(":/icons/resources/icons/app_logo.svg"));
    setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
    setMinimumSize(750, 520);

    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect avail = screen->availableGeometry();
        resize(qMin(1100, static_cast<int>(avail.width() * 0.85)),
               qMin(750, static_cast<int>(avail.height() * 0.85)));
    } else {
        resize(1000, 700);
    }

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        auto *root = new QVBoxLayout(this);
        auto *errLabel = new QLabel(tr("Unable to read binary file: %1").arg(m_filePath), this);
        root->addWidget(errLabel);
        return;
    }

    m_bytes = file.readAll();
    file.close();

    m_detected = BinToMatConverter::autoDetectFormat(m_filePath);

    setupUi();
}

void BinFileViewerDialog::setupUi() {
    auto *root = new QVBoxLayout(this);

    m_infoLabel = new QLabel(this);
    m_infoLabel->setStyleSheet("QLabel { background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 4px; padding: 6px; font-size: 11px; }");
    m_infoLabel->setText(
        tr("<b>File:</b> %1 &nbsp;|&nbsp; <b>Size:</b> %2 bytes &nbsp;|&nbsp; <b>Detected Type:</b> %3 &nbsp;|&nbsp; <b>Endian:</b> %4<br>"
           "<b>Matrix Layout:</b> %5 × %6 (%7 elements) &nbsp;|&nbsp; <b>Offset:</b> %8 bytes &nbsp;|&nbsp; <b>Detection Reason:</b> %9")
        .arg(QFileInfo(m_filePath).fileName())
        .arg(m_bytes.size())
        .arg(getDataTypeTitle(m_detected.dataType))
        .arg(getEndianTitle(m_detected.endianness))
        .arg(m_detected.matrixRows)
        .arg(m_detected.matrixCols)
        .arg(m_detected.matrixRows * m_detected.matrixCols)
        .arg(m_detected.payloadOffset)
        .arg(m_detected.reason)
    );
    m_infoLabel->setWordWrap(true);
    root->addWidget(m_infoLabel);

    m_tabs = new QTabWidget(this);

    // Tab 1: Data
    auto *table = new QTableView(m_tabs);
    table->setModel(new BinMatrixModel(m_bytes, m_detected.dataType, m_detected.endianness,
                                       m_detected.matrixRows, m_detected.matrixCols,
                                       m_detected.payloadOffset, table));
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->horizontalHeader()->setDefaultSectionSize(88);
    table->verticalHeader()->setDefaultSectionSize(24);
    m_tabs->addTab(table, tr("Data"));

    // Tab 2: Hex (Virtual Paged - Fast & Instant)
    auto *hex = new BinHexWidget(m_bytes, m_tabs);
    m_tabs->addTab(hex, tr("Hex"));

    // Tab 3: Visual Plot (Continuous colormap & dynamic numeric range legend)
    auto *plot = new BinHeatmapWidget(m_bytes, m_detected.dataType, m_detected.endianness,
                                      m_detected.matrixRows, m_detected.matrixCols,
                                      m_detected.payloadOffset, m_tabs);
    m_plotWidget = plot;
    m_tabs->addTab(plot, tr("Visual Plot"));

    root->addWidget(m_tabs, 1);

    // Bottom Action Ribbon: Export Graph, Export ASC, Close
    auto *bottomLayout = new QHBoxLayout();
    m_saveGraphBtn = new QPushButton(tr("Save Graph Image..."), this);
    m_saveGraphBtn->setIcon(QIcon(":/icons/resources/icons/convert.svg"));

    m_saveAscBtn = new QPushButton(tr("Save as .asc..."), this);
    m_saveAscBtn->setIcon(QIcon(":/icons/resources/icons/browse.svg"));

    auto *closeBtn = new QPushButton(tr("Close"), this);

    bottomLayout->addWidget(m_saveGraphBtn);
    bottomLayout->addWidget(m_saveAscBtn);
    bottomLayout->addStretch();
    bottomLayout->addWidget(closeBtn);

    root->addLayout(bottomLayout);

    connect(m_saveGraphBtn, &QPushButton::clicked, this, &BinFileViewerDialog::onSaveGraphImage);
    connect(m_saveAscBtn, &QPushButton::clicked, this, &BinFileViewerDialog::onSaveAscFile);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void BinFileViewerDialog::onSaveGraphImage() {
    if (!m_plotWidget) {
        QMessageBox::warning(this, tr("Export Warning"), tr("Visual plot is not available to export."));
        return;
    }
    QString baseName = QFileInfo(m_filePath).completeBaseName();
    QString defaultPath = QDir::homePath() + "/Downloads/MatBin/output/" + baseName + "_heatmap.png";
    QString filter = tr("PNG Image (*.png);;JPEG Image (*.jpg *.jpeg);;Bitmap Image (*.bmp)");
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save Graph Image"), defaultPath, filter);
    if (savePath.isEmpty()) return;

    QPixmap pixmap(m_plotWidget->size());
    m_plotWidget->render(&pixmap);
    if (pixmap.save(savePath)) {
        QMessageBox::information(this, tr("Export Successful"), tr("Graph image saved successfully to:\n%1").arg(savePath));
    } else {
        QMessageBox::warning(this, tr("Export Failed"), tr("Failed to save graph image to:\n%1").arg(savePath));
    }
}

void BinFileViewerDialog::onSaveAscFile() {
    if (m_detected.matrixRows == 0 || m_detected.matrixCols == 0) {
        QMessageBox::warning(this, tr("Export Warning"), tr("No matrix layout available to export as .asc."));
        return;
    }

    const quint64 rows = m_detected.matrixRows;
    const quint64 cols = m_detected.matrixCols;

    QString baseName = QFileInfo(m_filePath).completeBaseName();
    QString defaultPath = QDir::homePath() + "/Downloads/MatBin/output/" + baseName + ".asc";
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save as .asc"), defaultPath, tr("ASCII Grid Matrix (*.asc);;All Files (*)"));
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export Error"), tr("Unable to create .asc export file:\n%1").arg(savePath));
        return;
    }

    size_t elemSize = 1;
    switch (m_detected.dataType) {
    case BinToMatConverter::DataType::Double64: elemSize = sizeof(double); break;
    case BinToMatConverter::DataType::Single32: elemSize = sizeof(float); break;
    case BinToMatConverter::DataType::Int64:    elemSize = sizeof(qint64); break;
    case BinToMatConverter::DataType::Uint64:   elemSize = sizeof(quint64); break;
    case BinToMatConverter::DataType::Int32:    elemSize = sizeof(qint32); break;
    case BinToMatConverter::DataType::Uint32:   elemSize = sizeof(quint32); break;
    case BinToMatConverter::DataType::Uint16:   elemSize = sizeof(quint16); break;
    case BinToMatConverter::DataType::Int16:    elemSize = sizeof(qint16); break;
    case BinToMatConverter::DataType::Uint8:    elemSize = sizeof(quint8); break;
    case BinToMatConverter::DataType::Int8:     elemSize = sizeof(qint8); break;
    }

    QTextStream out(&file);
    out << "NCOLS " << cols << "\n";
    out << "NROWS " << rows << "\n";
    out << "XLLCORNER 0.0\n";
    out << "YLLCORNER 0.0\n";
    out << "CELLSIZE 1.0\n";
    out << "NODATA_VALUE -9999\n";

    for (quint64 r = 0; r < rows; ++r) {
        QStringList rowTokens;
        rowTokens.reserve(static_cast<qsizetype>(qMin<quint64>(cols, 10000)));
        for (quint64 c = 0; c < cols; ++c) {
            const quint64 byteOffset = static_cast<quint64>(m_detected.payloadOffset) + (r * cols + c) * elemSize;
            if (byteOffset + elemSize > static_cast<quint64>(m_bytes.size())) {
                rowTokens.append("0");
                continue;
            }

            const char *p = m_bytes.constData() + static_cast<qsizetype>(byteOffset);
            switch (m_detected.dataType) {
            case BinToMatConverter::DataType::Uint8:
                rowTokens.append(QString::number(static_cast<quint8>(p[0])));
                break;
            case BinToMatConverter::DataType::Int8:
                rowTokens.append(QString::number(static_cast<qint8>(p[0])));
                break;
            case BinToMatConverter::DataType::Uint16: {
                quint16 v; std::memcpy(&v, p, sizeof(v));
                rowTokens.append(QString::number(m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v)));
                break;
            }
            case BinToMatConverter::DataType::Int16: {
                quint16 raw; std::memcpy(&raw, p, sizeof(raw));
                raw = m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
                rowTokens.append(QString::number(static_cast<qint16>(raw)));
                break;
            }
            case BinToMatConverter::DataType::Uint32: {
                quint32 v; std::memcpy(&v, p, sizeof(v));
                rowTokens.append(QString::number(m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v)));
                break;
            }
            case BinToMatConverter::DataType::Int32: {
                quint32 raw; std::memcpy(&raw, p, sizeof(raw));
                raw = m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
                rowTokens.append(QString::number(static_cast<qint32>(raw)));
                break;
            }
            case BinToMatConverter::DataType::Uint64: {
                quint64 v; std::memcpy(&v, p, sizeof(v));
                rowTokens.append(QString::number(m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v)));
                break;
            }
            case BinToMatConverter::DataType::Int64: {
                quint64 raw; std::memcpy(&raw, p, sizeof(raw));
                raw = m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
                rowTokens.append(QString::number(static_cast<qint64>(raw)));
                break;
            }
            case BinToMatConverter::DataType::Single32: {
                quint32 raw; std::memcpy(&raw, p, sizeof(raw));
                raw = m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
                float val; std::memcpy(&val, &raw, sizeof(val));
                rowTokens.append(QString::number(val, 'g', 9));
                break;
            }
            case BinToMatConverter::DataType::Double64: {
                quint64 raw; std::memcpy(&raw, p, sizeof(raw));
                raw = m_detected.endianness == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
                double val; std::memcpy(&val, &raw, sizeof(val));
                rowTokens.append(QString::number(val, 'g', 17));
                break;
            }
            }
        }
        out << rowTokens.join(" ") << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Export Successful"), tr("Matrix data exported successfully to:\n%1").arg(savePath));
}
