#include "MatFileViewer.h"

#include <matio.h>

#include <QAbstractTableModel>
#include <QComboBox>
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
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

static QString getMatTypeName(const matvar_t *var) {
    if (!var) return "unknown";
    if (var->class_type == MAT_C_UINT16 || var->data_type == MAT_T_UINT16) return "uint16";
    if (var->class_type == MAT_C_INT16 || var->data_type == MAT_T_INT16) return "int16";
    if (var->class_type == MAT_C_DOUBLE || var->data_type == MAT_T_DOUBLE) return "double";
    if (var->class_type == MAT_C_SINGLE || var->data_type == MAT_T_SINGLE) return "single";
    if (var->class_type == MAT_C_UINT32 || var->data_type == MAT_T_UINT32) return "uint32";
    if (var->class_type == MAT_C_INT32 || var->data_type == MAT_T_INT32) return "int32";
    if (var->class_type == MAT_C_UINT64 || var->data_type == MAT_T_UINT64) return "uint64";
    if (var->class_type == MAT_C_INT64 || var->data_type == MAT_T_INT64) return "int64";
    if (var->class_type == MAT_C_UINT8 || var->data_type == MAT_T_UINT8) return "uint8";
    if (var->class_type == MAT_C_INT8 || var->data_type == MAT_T_INT8) return "int8";
    return "numeric";
}

namespace {

class MatMatrixModel : public QAbstractTableModel {
public:
    explicit MatMatrixModel(const matvar_t *var, QObject *parent = nullptr)
        : QAbstractTableModel(parent), m_var(var) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid() || !m_var || m_var->rank <= 0) return 0;
        return static_cast<int>(qMin<quint64>(m_var->dims[0], std::numeric_limits<int>::max()));
    }

    int columnCount(const QModelIndex &parent = QModelIndex()) const override {
        if (parent.isValid() || !m_var || m_var->rank <= 0) return 0;
        quint64 cols = 1;
        for (int i = 1; i < m_var->rank; ++i) {
            if (m_var->dims[i] != 0 && cols > std::numeric_limits<quint64>::max() / m_var->dims[i]) return 0;
            cols *= m_var->dims[i];
        }
        return static_cast<int>(qMin<quint64>(cols, std::numeric_limits<int>::max()));
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
        if (!index.isValid() || role != Qt::DisplayRole || !m_var || !m_var->data) return {};
        const quint64 rows = m_var->dims[0];
        const quint64 linearIndex = static_cast<quint64>(index.row()) + rows * static_cast<quint64>(index.column());
        return valueString(linearIndex);
    }

private:
    QString valueString(quint64 i) const {
        if (!m_var || !m_var->data) return {};
        if (m_var->isComplex) return "complex";

        if (m_var->class_type == MAT_C_UINT16 || m_var->data_type == MAT_T_UINT16) {
            return QString::number(static_cast<const quint16*>(m_var->data)[i]);
        }
        if (m_var->class_type == MAT_C_INT16 || m_var->data_type == MAT_T_INT16) {
            return QString::number(static_cast<const qint16*>(m_var->data)[i]);
        }
        if (m_var->class_type == MAT_C_DOUBLE || m_var->data_type == MAT_T_DOUBLE) {
            return QString::number(static_cast<const double*>(m_var->data)[i], 'g', 17);
        }
        if (m_var->class_type == MAT_C_SINGLE || m_var->data_type == MAT_T_SINGLE) {
            return QString::number(static_cast<const float*>(m_var->data)[i], 'g', 9);
        }
        if (m_var->class_type == MAT_C_UINT32 || m_var->data_type == MAT_T_UINT32) {
            return QString::number(static_cast<quint32>(static_cast<const quint32*>(m_var->data)[i]));
        }
        if (m_var->class_type == MAT_C_INT32 || m_var->data_type == MAT_T_INT32) {
            return QString::number(static_cast<qint32>(static_cast<const qint32*>(m_var->data)[i]));
        }
        if (m_var->class_type == MAT_C_UINT64 || m_var->data_type == MAT_T_UINT64) {
            return QString::number(static_cast<qulonglong>(static_cast<const quint64*>(m_var->data)[i]));
        }
        if (m_var->class_type == MAT_C_INT64 || m_var->data_type == MAT_T_INT64) {
            return QString::number(static_cast<qlonglong>(static_cast<const qint64*>(m_var->data)[i]));
        }
        if (m_var->class_type == MAT_C_UINT8 || m_var->data_type == MAT_T_UINT8) {
            return QString::number(static_cast<const quint8*>(m_var->data)[i]);
        }
        if (m_var->class_type == MAT_C_INT8 || m_var->data_type == MAT_T_INT8) {
            return QString::number(static_cast<const qint8*>(m_var->data)[i]);
        }
        return {};
    }

    const matvar_t *m_var = nullptr;
};

class MatHexWidget : public QWidget {
public:
    explicit MatHexWidget(const matvar_t *var, QWidget *parent = nullptr)
        : QWidget(parent), m_var(var) {
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
        if (!m_var) return 0;
        return static_cast<int>((m_var->nbytes + m_pageBytes - 1) / m_pageBytes);
    }

    void refresh() {
        if (!m_var || !m_var->data || m_var->nbytes == 0) {
            m_text->clear();
            m_offsetLabel->setText(tr("No payload bytes"));
            return;
        }

        const size_t start = static_cast<size_t>(m_pageSpin->value()) * m_pageBytes;
        const size_t count = std::min(m_pageBytes, m_var->nbytes - start);
        const auto *bytes = static_cast<const unsigned char*>(m_var->data) + start;

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
            .arg(static_cast<qulonglong>(m_var->nbytes))
            .arg(m_pageSpin->value() + 1)
            .arg(pageCount()));
        m_prev->setEnabled(m_pageSpin->value() > 0);
        m_next->setEnabled(m_pageSpin->value() + 1 < pageCount());
    }

    static constexpr size_t m_pageBytes = 4096;
    const matvar_t *m_var = nullptr;
    QPlainTextEdit *m_text = nullptr;
    QSpinBox *m_pageSpin = nullptr;
    QPushButton *m_prev = nullptr;
    QPushButton *m_next = nullptr;
    QLabel *m_offsetLabel = nullptr;
};

class MatHeatmapWidget : public QWidget {
public:
    explicit MatHeatmapWidget(const matvar_t *var, QWidget *parent = nullptr)
        : QWidget(parent), m_var(var) {
        setMinimumSize(720, 500);
        calculateRange();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), Qt::white);
        if (!m_var || !m_var->data || m_rows == 0 || m_cols == 0 || !std::isfinite(m_min) || !std::isfinite(m_max)) return;

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
            
            // Color swatch matching actual continuous color palette
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

    double valueAt(quint64 row, quint64 col) const {
        if (!m_var || !m_var->data) return std::numeric_limits<double>::quiet_NaN();
        const quint64 index = row + m_rows * col;

        if (m_var->class_type == MAT_C_UINT16 || m_var->data_type == MAT_T_UINT16) {
            return static_cast<const quint16*>(m_var->data)[index];
        }
        if (m_var->class_type == MAT_C_INT16 || m_var->data_type == MAT_T_INT16) {
            return static_cast<const qint16*>(m_var->data)[index];
        }
        if (m_var->class_type == MAT_C_DOUBLE || m_var->data_type == MAT_T_DOUBLE) {
            return static_cast<const double*>(m_var->data)[index];
        }
        if (m_var->class_type == MAT_C_SINGLE || m_var->data_type == MAT_T_SINGLE) {
            return static_cast<const float*>(m_var->data)[index];
        }
        if (m_var->class_type == MAT_C_UINT32 || m_var->data_type == MAT_T_UINT32) {
            return static_cast<double>(static_cast<const quint32*>(m_var->data)[index]);
        }
        if (m_var->class_type == MAT_C_INT32 || m_var->data_type == MAT_T_INT32) {
            return static_cast<double>(static_cast<const qint32*>(m_var->data)[index]);
        }
        if (m_var->class_type == MAT_C_UINT64 || m_var->data_type == MAT_T_UINT64) {
            return static_cast<double>(static_cast<const quint64*>(m_var->data)[index]);
        }
        if (m_var->class_type == MAT_C_INT64 || m_var->data_type == MAT_T_INT64) {
            return static_cast<double>(static_cast<const qint64*>(m_var->data)[index]);
        }
        if (m_var->class_type == MAT_C_UINT8 || m_var->data_type == MAT_T_UINT8) {
            return static_cast<const quint8*>(m_var->data)[index];
        }
        if (m_var->class_type == MAT_C_INT8 || m_var->data_type == MAT_T_INT8) {
            return static_cast<const qint8*>(m_var->data)[index];
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    void calculateRange() {
        if (!m_var || !m_var->data || m_var->isComplex || m_var->rank <= 0) return;
        m_rows = m_var->dims[0];
        m_cols = 1;
        for (int i = 1; i < m_var->rank; ++i) m_cols *= m_var->dims[i];
        const quint64 total = m_rows * m_cols;
        m_min = std::numeric_limits<double>::infinity();
        m_max = -std::numeric_limits<double>::infinity();
        for (quint64 i = 0; i < total; ++i) {
            const double v = valueAt(i % m_rows, i / m_rows);
            if (std::isfinite(v)) {
                m_min = qMin(m_min, v);
                m_max = qMax(m_max, v);
            }
        }
    }

    const matvar_t *m_var = nullptr;
    quint64 m_rows = 0;
    quint64 m_cols = 0;
    double m_min = std::numeric_limits<double>::quiet_NaN();
    double m_max = std::numeric_limits<double>::quiet_NaN();
};

} // namespace

MatFileViewerDialog::MatFileViewerDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent), m_filePath(filePath) {
    setWindowTitle(tr("MAT Content Viewer - %1").arg(QFileInfo(filePath).fileName()));
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

    auto *root = new QVBoxLayout(this);
    auto *top = new QGridLayout();

    top->addWidget(new QLabel(tr("MAT Variable:"), this), 0, 0);
    m_variableCombo = new QComboBox(this);
    top->addWidget(m_variableCombo, 0, 1);
    m_infoLabel = new QLabel(this);
    m_infoLabel->setWordWrap(true);
    top->addWidget(m_infoLabel, 1, 0, 1, 2);
    root->addLayout(top);

    m_tabs = new QTabWidget(this);
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

    connect(m_saveGraphBtn, &QPushButton::clicked, this, &MatFileViewerDialog::onSaveGraphImage);
    connect(m_saveAscBtn, &QPushButton::clicked, this, &MatFileViewerDialog::onSaveAscFile);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_variableCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MatFileViewerDialog::onVariableChanged);

    QString error;
    if (!inspectFile(error) || m_variables.isEmpty()) {
        m_infoLabel->setText(error.isEmpty() ? tr("No numeric MATLAB variables were found.") : error);
        m_variableCombo->setEnabled(false);
        m_saveGraphBtn->setEnabled(false);
        m_saveAscBtn->setEnabled(false);
        return;
    }

    for (const auto &v : m_variables) {
        m_variableCombo->addItem(v.name);
    }
    m_variableCombo->setCurrentIndex(0);
}

MatFileViewerDialog::~MatFileViewerDialog() {
    if (m_variable) Mat_VarFree(m_variable);
    if (m_matFile) Mat_Close(m_matFile);
}

bool MatFileViewerDialog::isNumericVariable(const matvar_t *var) const {
    if (!var) return false;

    // Check class_type
    switch (var->class_type) {
    case MAT_C_UINT8:
    case MAT_C_INT8:
    case MAT_C_UINT16:
    case MAT_C_INT16:
    case MAT_C_UINT32:
    case MAT_C_INT32:
    case MAT_C_UINT64:
    case MAT_C_INT64:
    case MAT_C_SINGLE:
    case MAT_C_DOUBLE:
        return true;
    default:
        break;
    }

    // Check data_type fallback
    switch (var->data_type) {
    case MAT_T_UINT8:
    case MAT_T_INT8:
    case MAT_T_UINT16:
    case MAT_T_INT16:
    case MAT_T_UINT32:
    case MAT_T_INT32:
    case MAT_T_UINT64:
    case MAT_T_INT64:
    case MAT_T_SINGLE:
    case MAT_T_DOUBLE:
        return true;
    default:
        break;
    }

    return false;
}

bool MatFileViewerDialog::inspectFile(QString &errorMessage) {
    QString nativePath = QDir::toNativeSeparators(m_filePath);
    mat_t *mat = Mat_Open(nativePath.toUtf8().constData(), MAT_ACC_RDONLY);
    if (!mat) {
        mat = Mat_Open(nativePath.toLocal8Bit().constData(), MAT_ACC_RDONLY);
    }
    if (!mat) {
        errorMessage = tr("Unable to open MAT file: %1").arg(m_filePath);
        return false;
    }

    m_variables.clear();
    while (matvar_t *info = Mat_VarReadNextInfo(mat)) {
        if (isNumericVariable(info)) {
            VariableInfo v;
            v.name = QString::fromUtf8(info->name ? info->name : "unnamed");
            v.rank = info->rank;
            for (int i = 0; i < info->rank; ++i) {
                v.dims.append(static_cast<quint64>(info->dims[i]));
            }
            m_variables.append(v);
        }
        Mat_VarFree(info);
    }
    Mat_Close(mat);

    if (m_variables.isEmpty()) {
        errorMessage = tr("No supported numeric variables were found in this MAT file.");
        return false;
    }
    return true;
}

bool MatFileViewerDialog::loadVariable(const QString &name, QString &errorMessage) {
    if (m_variable) {
        Mat_VarFree(m_variable);
        m_variable = nullptr;
    }
    if (m_matFile) {
        Mat_Close(m_matFile);
        m_matFile = nullptr;
    }

    QString nativePath = QDir::toNativeSeparators(m_filePath);
    m_matFile = Mat_Open(nativePath.toUtf8().constData(), MAT_ACC_RDONLY);
    if (!m_matFile) {
        m_matFile = Mat_Open(nativePath.toLocal8Bit().constData(), MAT_ACC_RDONLY);
    }
    if (!m_matFile) {
        errorMessage = tr("Unable to reopen MAT file.");
        return false;
    }

    m_variable = Mat_VarRead(m_matFile, name.toUtf8().constData());
    if (!m_variable) {
        m_variable = Mat_VarRead(m_matFile, name.toLocal8Bit().constData());
    }

    if (!m_variable || !m_variable->data) {
        errorMessage = tr("Unable to read data for variable '%1'.").arg(name);
        return false;
    }
    return true;
}

void MatFileViewerDialog::onVariableChanged(int index) {
    if (index < 0 || index >= m_variables.size()) return;
    QString error;
    if (!loadVariable(m_variables[index].name, error)) {
        m_infoLabel->setText(error);
        return;
    }
    rebuildTabs();
}

void MatFileViewerDialog::rebuildTabs() {
    while (m_tabs->count() > 0) {
        QWidget *w = m_tabs->widget(0);
        m_tabs->removeTab(0);
        w->deleteLater();
    }
    m_plotWidget = nullptr;
    if (!m_variable) return;

    QString dimsText;
    for (int i = 0; i < m_variable->rank; ++i) {
        if (i) dimsText += " × ";
        dimsText += QString::number(static_cast<qulonglong>(m_variable->dims[i]));
    }
    m_infoLabel->setText(tr("Variable: <b>%1</b> &nbsp;&nbsp;|&nbsp;&nbsp; Type: <b>%2</b> &nbsp;&nbsp;|&nbsp;&nbsp; Dimensions: <b>%3</b> &nbsp;&nbsp;|&nbsp;&nbsp; Bytes: <b>%4</b>")
                         .arg(QString::fromUtf8(m_variable->name ? m_variable->name : "unnamed"))
                         .arg(getMatTypeName(m_variable))
                         .arg(dimsText)
                         .arg(static_cast<qulonglong>(m_variable->nbytes)));

    auto *table = new QTableView(m_tabs);
    table->setModel(new MatMatrixModel(m_variable, table));
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->horizontalHeader()->setDefaultSectionSize(88);
    table->verticalHeader()->setDefaultSectionSize(24);
    m_tabs->addTab(table, tr("Data"));

    auto *hex = new MatHexWidget(m_variable, m_tabs);
    m_tabs->addTab(hex, tr("Hex"));

    auto *plot = new MatHeatmapWidget(m_variable, m_tabs);
    m_plotWidget = plot;
    m_tabs->addTab(plot, tr("Visual Plot"));
}

void MatFileViewerDialog::onSaveGraphImage() {
    if (!m_plotWidget) {
        QMessageBox::warning(this, tr("Export Warning"), tr("Visual plot is not available to export."));
        return;
    }
    QString baseName = QFileInfo(m_filePath).completeBaseName();
    if (m_variable && m_variable->name) {
        baseName += "_" + QString::fromUtf8(m_variable->name);
    }
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

void MatFileViewerDialog::onSaveAscFile() {
    if (!m_variable || !m_variable->data || m_variable->rank < 2) {
        QMessageBox::warning(this, tr("Export Warning"), tr("No 2D numeric matrix data available to export as .asc."));
        return;
    }

    const quint64 rows = m_variable->dims[0];
    quint64 cols = 1;
    for (int i = 1; i < m_variable->rank; ++i) cols *= m_variable->dims[i];

    QString baseName = QFileInfo(m_filePath).completeBaseName();
    if (m_variable && m_variable->name) {
        baseName += "_" + QString::fromUtf8(m_variable->name);
    }
    QString defaultPath = QDir::homePath() + "/Downloads/MatBin/output/" + baseName + ".asc";
    QString savePath = QFileDialog::getSaveFileName(this, tr("Save as .asc"), defaultPath, tr("ASCII Grid Matrix (*.asc);;All Files (*)"));
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export Error"), tr("Unable to create .asc export file:\n%1").arg(savePath));
        return;
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
            const quint64 index = r + rows * c;
            if (m_variable->class_type == MAT_C_UINT16 || m_variable->data_type == MAT_T_UINT16) {
                rowTokens.append(QString::number(static_cast<const quint16*>(m_variable->data)[index]));
            } else if (m_variable->class_type == MAT_C_INT16 || m_variable->data_type == MAT_T_INT16) {
                rowTokens.append(QString::number(static_cast<const qint16*>(m_variable->data)[index]));
            } else if (m_variable->class_type == MAT_C_DOUBLE || m_variable->data_type == MAT_T_DOUBLE) {
                rowTokens.append(QString::number(static_cast<const double*>(m_variable->data)[index], 'g', 17));
            } else if (m_variable->class_type == MAT_C_SINGLE || m_variable->data_type == MAT_T_SINGLE) {
                rowTokens.append(QString::number(static_cast<const float*>(m_variable->data)[index], 'g', 9));
            } else if (m_variable->class_type == MAT_C_UINT32 || m_variable->data_type == MAT_T_UINT32) {
                rowTokens.append(QString::number(static_cast<quint32>(static_cast<const quint32*>(m_variable->data)[index])));
            } else if (m_variable->class_type == MAT_C_INT32 || m_variable->data_type == MAT_T_INT32) {
                rowTokens.append(QString::number(static_cast<qint32>(static_cast<const qint32*>(m_variable->data)[index])));
            } else if (m_variable->class_type == MAT_C_UINT64 || m_variable->data_type == MAT_T_UINT64) {
                rowTokens.append(QString::number(static_cast<qulonglong>(static_cast<const quint64*>(m_variable->data)[index])));
            } else if (m_variable->class_type == MAT_C_INT64 || m_variable->data_type == MAT_T_INT64) {
                rowTokens.append(QString::number(static_cast<qlonglong>(static_cast<const qint64*>(m_variable->data)[index])));
            } else if (m_variable->class_type == MAT_C_UINT8 || m_variable->data_type == MAT_T_UINT8) {
                rowTokens.append(QString::number(static_cast<const quint8*>(m_variable->data)[index]));
            } else if (m_variable->class_type == MAT_C_INT8 || m_variable->data_type == MAT_T_INT8) {
                rowTokens.append(QString::number(static_cast<const qint8*>(m_variable->data)[index]));
            } else {
                rowTokens.append("0");
            }
        }
        out << rowTokens.join(" ") << "\n";
    }

    file.close();
    QMessageBox::information(this, tr("Export Successful"), tr("Matrix data exported successfully to:\n%1").arg(savePath));
}
