#include "MainWindow.h"
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include <QFileInfo>
#include <QDir>
#include <QIcon>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDialog>
#include <QScreen>
#include <QGuiApplication>
#include <QAbstractTableModel>
#include <QTableView>
#include <QTabWidget>
#include <QHeaderView>
#include <QSplitter>
#include <QPainter>
#include <QElapsedTimer>
#include <QScrollBar>
#include <QStatusBar>
#include <QtEndian>
#include <limits>
#include <cmath>
#include <cstring>


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

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || role != Qt::DisplayRole) return {};

        const quint64 row = static_cast<quint64>(index.row());
        const quint64 col = static_cast<quint64>(index.column());
        const size_t elementSize = elementByteSize();
        const quint64 linearIndex = row * m_cols + col;
        const quint64 byteOffset = static_cast<quint64>(m_payloadOffset) + linearIndex * elementSize;

        if (byteOffset + elementSize > static_cast<quint64>(m_bytes.size())) return {};
        const char *p = m_bytes.constData() + static_cast<qsizetype>(byteOffset);

        switch (m_type) {
        case BinToMatConverter::DataType::Uint8:
            return QString::number(static_cast<unsigned int>(static_cast<unsigned char>(p[0])));
        case BinToMatConverter::DataType::Int8:
            return QString::number(static_cast<int>(static_cast<qint8>(p[0])));
        case BinToMatConverter::DataType::Uint16: {
            quint16 v; std::memcpy(&v, p, sizeof(v));
            if (m_endian == BinToMatConverter::Endianness::LittleEndianMode) v = qFromLittleEndian(v); else v = qFromBigEndian(v);
            return QString::number(v);
        }
        case BinToMatConverter::DataType::Int16: {
            quint16 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = (m_endian == BinToMatConverter::Endianness::LittleEndianMode) ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(static_cast<qint16>(raw));
        }
        case BinToMatConverter::DataType::Int32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = (m_endian == BinToMatConverter::Endianness::LittleEndianMode) ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(static_cast<qint32>(raw));
        }
        case BinToMatConverter::DataType::Uint32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = (m_endian == BinToMatConverter::Endianness::LittleEndianMode) ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return QString::number(raw);
        }
        case BinToMatConverter::DataType::Int64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            if (m_endian == BinToMatConverter::Endianness::LittleEndianMode) raw = qFromLittleEndian(raw); else raw = qFromBigEndian(raw);
            return QString::number(static_cast<qlonglong>(raw));
        }
        case BinToMatConverter::DataType::Uint64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            if (m_endian == BinToMatConverter::Endianness::LittleEndianMode) raw = qFromLittleEndian(raw); else raw = qFromBigEndian(raw);
            return QString::number(raw);
        }
        case BinToMatConverter::DataType::Single32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            if (m_endian == BinToMatConverter::Endianness::LittleEndianMode) raw = qFromLittleEndian(raw); else raw = qFromBigEndian(raw);
            float value; std::memcpy(&value, &raw, sizeof(value));
            return QString::number(value, 'g', 9);
        }
        case BinToMatConverter::DataType::Double64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            if (m_endian == BinToMatConverter::Endianness::LittleEndianMode) raw = qFromLittleEndian(raw); else raw = qFromBigEndian(raw);
            double value; std::memcpy(&value, &raw, sizeof(value));
            return QString::number(value, 'g', 17);
        }
        }

        return {};
    }

private:
    size_t elementByteSize() const {
        switch (m_type) {
        case BinToMatConverter::DataType::Double64: return sizeof(double);
        case BinToMatConverter::DataType::Single32: return sizeof(float);
        case BinToMatConverter::DataType::Int64: return sizeof(qint64);
        case BinToMatConverter::DataType::Int32: return sizeof(qint32);
        case BinToMatConverter::DataType::Uint16: return sizeof(quint16);
        case BinToMatConverter::DataType::Int16: return sizeof(qint16);
        case BinToMatConverter::DataType::Uint8: return sizeof(quint8);
        case BinToMatConverter::DataType::Int8: return sizeof(qint8);
        }
        return 1;
    }

    QByteArray m_bytes;
    BinToMatConverter::DataType m_type;
    BinToMatConverter::Endianness m_endian;
    quint64 m_rows = 0;
    quint64 m_cols = 0;
    qint64 m_payloadOffset = 0;
};

class BinMatrixPlotWidget : public QWidget {
public:
    BinMatrixPlotWidget(const QByteArray &bytes,
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
        setMinimumSize(250, 150);
        setAutoFillBackground(true);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);

        if (m_rows == 0 || m_cols == 0) return;

        const int imageW = qMax(1, width() - 80);
        const int imageH = qMax(1, height() - 60);
        const int offsetX = 40;
        const int offsetY = 20;

        // Downsample to screen resolution. The source matrix remains untouched.
        QImage image(imageW, imageH, QImage::Format_RGB32);

        double minValue = std::numeric_limits<double>::infinity();
        double maxValue = -std::numeric_limits<double>::infinity();

        // First pass: sample source values to determine range.
        const quint64 samples = static_cast<quint64>(imageW) * static_cast<quint64>(imageH);
        const quint64 strideR = qMax<quint64>(1, m_rows / static_cast<quint64>(imageH));
        const quint64 strideC = qMax<quint64>(1, m_cols / static_cast<quint64>(imageW));
        Q_UNUSED(samples);

        for (int y = 0; y < imageH; ++y) {
            const quint64 row = qMin<quint64>(m_rows - 1, static_cast<quint64>(y) * strideR);
            for (int x = 0; x < imageW; ++x) {
                const quint64 col = qMin<quint64>(m_cols - 1, static_cast<quint64>(x) * strideC);
                const double value = valueAt(row, col);
                if (std::isfinite(value)) {
                    minValue = qMin(minValue, value);
                    maxValue = qMax(maxValue, value);
                }
            }
        }

        if (!std::isfinite(minValue) || !std::isfinite(maxValue)) return;
        const double range = (maxValue > minValue) ? (maxValue - minValue) : 1.0;

        for (int y = 0; y < imageH; ++y) {
            const quint64 row = qMin<quint64>(m_rows - 1, static_cast<quint64>(y) * strideR);
            for (int x = 0; x < imageW; ++x) {
                const quint64 col = qMin<quint64>(m_cols - 1, static_cast<quint64>(x) * strideC);
                double value = valueAt(row, col);
                if (!std::isfinite(value)) value = minValue;
                const double n = qBound(0.0, (value - minValue) / range, 1.0);

                // Blue -> cyan -> yellow -> red style heat map without external dependencies.
                const int r = static_cast<int>(255.0 * n);
                const int g = static_cast<int>(255.0 * std::sin(n * 3.14159265358979323846));
                const int b = static_cast<int>(255.0 * (1.0 - n));
                image.setPixel(x, y, qRgb(r, g, b));
            }
        }

        painter.drawImage(offsetX, offsetY, image);

        painter.setPen(Qt::white);
        painter.drawText(10, height() - 25,
                         QString("Min: %1    Max: %2    Matrix: %3 x %4")
                         .arg(minValue, 0, 'g', 10)
                         .arg(maxValue, 0, 'g', 10)
                         .arg(m_rows)
                         .arg(m_cols));
    }

private:
    size_t elementByteSize() const {
        switch (m_type) {
        case BinToMatConverter::DataType::Double64: return sizeof(double);
        case BinToMatConverter::DataType::Single32: return sizeof(float);
        case BinToMatConverter::DataType::Int64: return sizeof(qint64);
        case BinToMatConverter::DataType::Int32: return sizeof(qint32);
        case BinToMatConverter::DataType::Uint16: return sizeof(quint16);
        case BinToMatConverter::DataType::Int16: return sizeof(qint16);
        case BinToMatConverter::DataType::Uint8: return sizeof(quint8);
        case BinToMatConverter::DataType::Int8: return sizeof(qint8);
        }
        return 1;
    }

    double valueAt(quint64 row, quint64 col) const {
        const size_t elementSize = elementByteSize();
        const quint64 byteOffset = static_cast<quint64>(m_payloadOffset) + (row * m_cols + col) * elementSize;
        if (byteOffset + elementSize > static_cast<quint64>(m_bytes.size())) return std::numeric_limits<double>::quiet_NaN();

        const char *p = m_bytes.constData() + static_cast<qsizetype>(byteOffset);

        switch (m_type) {
        case BinToMatConverter::DataType::Uint8: return static_cast<unsigned char>(p[0]);
        case BinToMatConverter::DataType::Int8: return static_cast<qint8>(p[0]);
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
        case BinToMatConverter::DataType::Int64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            return static_cast<qint64>(raw);
        }
        case BinToMatConverter::DataType::Uint64: {
            quint64 v; std::memcpy(&v, p, sizeof(v));
            return m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(v) : qFromBigEndian(v);
        }
        case BinToMatConverter::DataType::Single32: {
            quint32 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            float value; std::memcpy(&value, &raw, sizeof(value));
            return value;
        }
        case BinToMatConverter::DataType::Double64: {
            quint64 raw; std::memcpy(&raw, p, sizeof(raw));
            raw = m_endian == BinToMatConverter::Endianness::LittleEndianMode ? qFromLittleEndian(raw) : qFromBigEndian(raw);
            double value; std::memcpy(&value, &raw, sizeof(value));
            return value;
        }
        }
        return std::numeric_limits<double>::quiet_NaN();
    }

    QByteArray m_bytes;
    BinToMatConverter::DataType m_type;
    BinToMatConverter::Endianness m_endian;
    quint64 m_rows = 0;
    quint64 m_cols = 0;
    qint64 m_payloadOffset = 0;
};

QString dataTypeName(BinToMatConverter::DataType type) {
    switch (type) {
    case BinToMatConverter::DataType::Double64: return "Double64";
    case BinToMatConverter::DataType::Single32: return "Single32";
    case BinToMatConverter::DataType::Int64: return "Int64";
    case BinToMatConverter::DataType::Int32: return "Int32";
    case BinToMatConverter::DataType::Uint16: return "Uint16";
    case BinToMatConverter::DataType::Int16: return "Int16";
    case BinToMatConverter::DataType::Uint8: return "Uint8";
    case BinToMatConverter::DataType::Int8: return "Int8";
    }
    return "Unknown";
}

QString endianName(BinToMatConverter::Endianness endian) {
    return endian == BinToMatConverter::Endianness::LittleEndianMode
        ? "Little-Endian" : "Big-Endian";
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent) {
    setupUi();
    connect(&futureWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onWorkerFinished);
}

void MainWindow::setupUi() {
    setWindowTitle(tr("MatBin - Binary to MATLAB MAT File Converter"));
    setWindowIcon(QIcon(":/icons/resources/icons/app_logo.svg"));
    resize(820, 620);

    // Menu Bar Action Ribbon
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About Application..."));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAboutApplication);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QGroupBox *fileGroup = new QGroupBox(tr("Batch Queue"), this);
    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);

    fileListWidget = new QListWidget();
    fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QHBoxLayout *fileBtnLayout = new QHBoxLayout();
    browseButton = new QPushButton(tr("Add Files..."));
    browseButton->setIcon(QIcon(":/icons/resources/icons/browse.svg"));

    removeButton = new QPushButton(tr("Remove Selected"));
    removeButton->setIcon(QIcon(":/icons/resources/icons/clear.svg"));

    clearButton = new QPushButton(tr("Clear Queue"));
    clearButton->setIcon(QIcon(":/icons/resources/icons/clear.svg"));

    viewContentsButton = new QPushButton(tr("View Contents..."));

    QSize standardIconSize(20, 20);
    browseButton->setIconSize(standardIconSize);
    removeButton->setIconSize(standardIconSize);
    clearButton->setIconSize(standardIconSize);

    fileBtnLayout->addWidget(browseButton);
    fileBtnLayout->addWidget(removeButton);
    fileBtnLayout->addWidget(clearButton);
    fileBtnLayout->addWidget(viewContentsButton);
    fileBtnLayout->addStretch();

    fileLayout->addWidget(fileListWidget);
    fileLayout->addLayout(fileBtnLayout);
    mainLayout->addWidget(fileGroup);

    QGroupBox *configGroup = new QGroupBox(tr("Structural & Data Configurations"), this);
    QFormLayout *configLayout = new QFormLayout(configGroup);
    configLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    dataTypeComboBox = new QComboBox();
    dataTypeComboBox->addItem(tr("Auto-Detect (Smart Payload Inspector)"), -1);
    dataTypeComboBox->addItem(tr("64-bit Double Precision Float (double) - 8 bytes"), static_cast<int>(BinToMatConverter::DataType::Double64));
    dataTypeComboBox->addItem(tr("32-bit Single Precision Float (float) - 4 bytes"), static_cast<int>(BinToMatConverter::DataType::Single32));
    dataTypeComboBox->addItem(tr("64-bit Signed Integer (int64_t) - 8 bytes"), static_cast<int>(BinToMatConverter::DataType::Int64));
    dataTypeComboBox->addItem(tr("64-bit Unsigned Integer (uint64_t) - 8 bytes"), static_cast<int>(BinToMatConverter::DataType::Uint64));
    dataTypeComboBox->addItem(tr("32-bit Signed Integer (int32_t) - 4 bytes"), static_cast<int>(BinToMatConverter::DataType::Int32));
    dataTypeComboBox->addItem(tr("32-bit Unsigned Integer (uint32_t) - 4 bytes"), static_cast<int>(BinToMatConverter::DataType::Uint32));
    dataTypeComboBox->addItem(tr("16-bit Unsigned Integer (uint16_t) - 2 bytes"), static_cast<int>(BinToMatConverter::DataType::Uint16));
    dataTypeComboBox->addItem(tr("16-bit Signed Integer (int16_t) - 2 bytes"), static_cast<int>(BinToMatConverter::DataType::Int16));
    dataTypeComboBox->addItem(tr("8-bit Unsigned Integer (uint8_t) - 1 byte"), static_cast<int>(BinToMatConverter::DataType::Uint8));
    dataTypeComboBox->addItem(tr("8-bit Signed Integer (int8_t) - 1 byte"), static_cast<int>(BinToMatConverter::DataType::Int8));

    endiannessComboBox = new QComboBox();
    endiannessComboBox->addItem(tr("Auto-Detect (Smart Payload Inspector)"), -1);
    endiannessComboBox->addItem(tr("Little-Endian (x86-64 / ARM64 Native - Least significant byte stored first)"), static_cast<int>(BinToMatConverter::Endianness::LittleEndianMode));
    endiannessComboBox->addItem(tr("Big-Endian (Network Order) - Most significant byte stored first"), static_cast<int>(BinToMatConverter::Endianness::BigEndianMode));

    dimensionComboBox = new QComboBox();
    dimensionComboBox->addItem(tr("Auto Detect (Smart Square / Natural Factor)"), 0ULL);

    varNameLineEdit = new QLineEdit("data");
    varNameLineEdit->setPlaceholderText(tr("MATLAB matrix variable name"));

    QString defaultDownloadsOutput = QDir::toNativeSeparators(QDir::homePath() + "/Downloads/MatBin/output");
    outputDirLineEdit = new QLineEdit();
    outputDirLineEdit->setPlaceholderText(defaultDownloadsOutput);
    selectDirBtn = new QPushButton(tr("Browse..."));
    selectDirBtn->setIcon(QIcon(":/icons/resources/icons/folder.svg"));
    selectDirBtn->setIconSize(standardIconSize);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->setContentsMargins(0, 0, 0, 0);
    dirLayout->addWidget(outputDirLineEdit);
    dirLayout->addWidget(selectDirBtn);

    configLayout->addRow(tr("Numerical Precision(Memory Byte Width):"), dataTypeComboBox);
    configLayout->addRow(tr("Serialization order:"), endiannessComboBox);
    configLayout->addRow(tr("Matrix Dimension (First MAT Dimension):"), dimensionComboBox);
    configLayout->addRow(tr("Workspace Variable Name:"), varNameLineEdit);
    configLayout->addRow(tr("Export Destination:"), dirLayout);

    mainLayout->addWidget(configGroup);

    QGroupBox *execGroup = new QGroupBox(tr("Execution Log"), this);
    QVBoxLayout *execLayout = new QVBoxLayout(execGroup);

    logTextEdit = new QTextEdit();
    logTextEdit->setReadOnly(true);

    progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    convertButton = new QPushButton(tr("Convert All Files"));
    convertButton->setIcon(QIcon(":/icons/resources/icons/convert.svg"));
    convertButton->setIconSize(QSize(24, 24));
    convertButton->setStyleSheet("font-weight: bold; min-height: 32px;");

    execLayout->addWidget(logTextEdit);
    execLayout->addWidget(progressBar);
    execLayout->addWidget(convertButton);

    mainLayout->addWidget(execGroup);

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseFiles);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedFiles);
    connect(selectDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearQueue);
    connect(viewContentsButton, &QPushButton::clicked, this, &MainWindow::onViewContents);
    connect(fileListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::updateViewContentsButton);
    connect(convertButton, &QPushButton::clicked, this, &MainWindow::onStartBatchConversion);

    viewContentsButton->setEnabled(false);
}

void MainWindow::onAboutApplication() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About MatBin Application"));
    dialog.setWindowIcon(QIcon(":/icons/resources/icons/app_logo.svg"));
    dialog.resize(680, 520);

    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);

    QLabel *headerLabel = new QLabel(tr(
        "<h2>MatBin - Binary to MATLAB MAT File Converter v1.3.0</h2>"
        "<p><b>Code by:</b> Yuvanesh (MC1)</p>"
        "<p><b>Primary Use:</b> To convert Tercom .bin files to .mat files</p>"
    ), &dialog);
    headerLabel->setWordWrap(true);
    dialogLayout->addWidget(headerLabel);

    // 1. Architecture & Operational Mechanics Note Group Box
    QGroupBox *noteGroup = new QGroupBox(tr("Architecture & Operational Mechanics Note"), &dialog);
    QVBoxLayout *noteLayout = new QVBoxLayout(noteGroup);

    QTextEdit *noteTextEdit = new QTextEdit(&dialog);
    noteTextEdit->setReadOnly(true);
    noteTextEdit->setStyleSheet(
        "QTextEdit { "
        "  background-color: #ffffff; "
        "  color: #1a252f; "
        "  border: 1px solid #cfd8dc; "
        "  border-radius: 4px; "
        "  padding: 8px; "
        "  font-size: 12px; "
        "}"
    );
    noteTextEdit->setHtml(tr(
        "<style>"
        "  body { font-size: 12px; font-family: sans-serif; line-height: 1.5; color: #1a252f; margin: 0; padding: 4px; background-color: #ffffff; }"
        "  b { color: #0d47a1; font-size: 13px; }"
        "  code { background-color: #eceff1; padding: 2px 5px; border-radius: 3px; color: #b71c1c; font-family: monospace; font-size: 11px; }"
        "  .warning { color: #b71c1c; font-weight: bold; margin-top: 10px; padding: 8px; background-color: #ffebee; border-radius: 4px; border: 1px solid #ef9a9a; }"
        "</style>"
        
        "<b>1. Batch Queue &amp; File Routing Mechanics</b><br/>"
        "Managing bulk data extraction requires automated batch processing and user overrides. The Batch Queue container holds active export tasks. Default output destination is <code>C:\\Users\\&lt;User&gt;\\Downloads\\MatBin\\output</code>.<br/><br/>"
        
        "<b>2. Smart Auto-Detection &amp; Precision</b><br/>"
        "Data serialization automatically identifies numerical precision (Double64, Single32, Int64, Uint64, Int32, Uint32, Uint16, Int16, Uint8, Int8) and stream byte order (Little-Endian / Big-Endian) upon file attachment.<br/><br/>"
        
        "<b>3. Dynamic Matrix Dimension Selection</b><br/>"
        "Calculates all valid divisor factor shapes (R &times; C) for the binary payload. Auto-Detect defaults to optimal square/natural matrix dimensions (e.g. 3600 &times; 3600 for 12,960,000 elements), with manual dropdown overrides available.<br/><br/>"
        
        "<b>4. Interactive Content Viewer &amp; Color Shades Legend</b><br/>"
        "Features a 3-tab inspector window (Data matrix table, Hex raw byte dump, and 2D Visual Plot heatmap with a color shades legend ranging from Deep Blue for minimums to Crimson Red for maximums). Window includes native Minimize (_), Maximize (&square;), and Close (&times;) controls.<br/><br/>"

        "<b>5. MATLAB Workspace Integration</b><br/>"
        "Structures numerical data into MATLAB v5 MAT containers under the designated workspace variable (default: <code>'data'</code>) for direct MATLAB import.<br/><br/>"
        
        "<div class=\"warning\"><b>Dependency Warning:</b> MATIO C-Library is required for MAT container serialization. If MATIO is not present or fails to initialize, the program will not run and will display an error message.</div>"
    ));

    noteLayout->addWidget(noteTextEdit);
    dialogLayout->addWidget(noteGroup);

    dialog.exec();
}

void MainWindow::onBrowseFiles() {
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Select Binary Files"), QString(), tr("Binary Files (*.bin *.dat);;All Files (*)")
    );

    if (!files.isEmpty()) {
        for (const QString &filePath : files) {
            if (!queuedFiles.contains(filePath)) {
                queuedFiles.append(filePath);
                fileListWidget->addItem(filePath);

                BinToMatConverter::DetectionResult detected = BinToMatConverter::autoDetectFormat(filePath);
                logTextEdit->append(QString("<font color=\"#0288D1\">[AUTO-DETECT] %1: %2</font>")
                                       .arg(QFileInfo(filePath).fileName(), detected.reason));
            }
        }
    }
}

void MainWindow::updateDimensionOptions() {
    if (!dimensionComboBox) return;

    dimensionComboBox->clear();
    dimensionComboBox->addItem(tr("Auto Detect (Smart Square / Natural Factor)"), 0ULL);

    if (!fileListWidget || fileListWidget->selectedItems().isEmpty()) {
        if (fileListWidget && fileListWidget->count() > 0) {
            fileListWidget->setCurrentRow(0);
        } else {
            return;
        }
    }

    if (fileListWidget->selectedItems().isEmpty()) return;

    const QString filePath = fileListWidget->selectedItems().first()->text();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    qint64 fileSize = file.size();
    file.close();

    if (fileSize == 0) return;

    BinToMatConverter::DetectionResult detected = BinToMatConverter::autoDetectFormat(filePath);
    size_t elementSize = sizeof(uint16_t);
    switch (detected.dataType) {
        case BinToMatConverter::DataType::Double64: elementSize = sizeof(double); break;
        case BinToMatConverter::DataType::Single32: elementSize = sizeof(float); break;
        case BinToMatConverter::DataType::Int64:    elementSize = sizeof(int64_t); break;
        case BinToMatConverter::DataType::Uint64:   elementSize = sizeof(uint64_t); break;
        case BinToMatConverter::DataType::Int32:    elementSize = sizeof(int32_t); break;
        case BinToMatConverter::DataType::Uint32:   elementSize = sizeof(uint32_t); break;
        case BinToMatConverter::DataType::Uint16:   elementSize = sizeof(uint16_t); break;
        case BinToMatConverter::DataType::Int16:    elementSize = sizeof(int16_t); break;
        case BinToMatConverter::DataType::Uint8:    elementSize = sizeof(uint8_t); break;
        case BinToMatConverter::DataType::Int8:     elementSize = sizeof(int8_t); break;
    }

    quint64 totalElements = static_cast<quint64>(fileSize / static_cast<qint64>(elementSize));
    if (totalElements == 0) return;

    quint64 root = static_cast<quint64>(std::sqrt(static_cast<long double>(totalElements)));
    
    QVector<quint64> factors;
    for (quint64 r = 1; r <= root; ++r) {
        if (totalElements % r == 0) {
            factors.append(r);
            quint64 c = totalElements / r;
            if (c != r) {
                factors.append(c);
            }
        }
    }
    std::sort(factors.begin(), factors.end());

    quint64 optR = detected.matrixRows > 0 ? detected.matrixRows : root;
    quint64 optC = detected.matrixCols > 0 ? detected.matrixCols : (totalElements / optR);
    dimensionComboBox->setItemText(0, tr("Auto Detect (%1 × %2)").arg(optR).arg(optC));

    for (quint64 r : factors) {
        quint64 c = totalElements / r;
        QString label = QString("%1 × %2").arg(r).arg(c);
        if (r == 1) label += tr(" (Row Vector)");
        else if (c == 1) label += tr(" (Column Vector)");
        else if (r == optR && c == optC) label += tr(" (Square Matrix)");

        dimensionComboBox->addItem(label, static_cast<qulonglong>(r));
    }
}

void MainWindow::updateViewContentsButton() {
    if (!viewContentsButton) return;
    viewContentsButton->setEnabled(!isProcessing && fileListWidget && fileListWidget->selectedItems().size() == 1);
    updateDimensionOptions();
}

void MainWindow::onViewContents() {
    if (isProcessing || !fileListWidget) return;

    const QList<QListWidgetItem*> selected = fileListWidget->selectedItems();
    if (selected.size() != 1) {
        QMessageBox::information(this, tr("Select One File"),
                                 tr("Select exactly one binary file from the Batch Queue."));
        return;
    }

    const QString filePath = selected.first()->text();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Unable to Open File"),
                              tr("Unable to open the selected binary file:\n%1").arg(filePath));
        return;
    }

    const qint64 fileSize = file.size();
    QByteArray bytes = file.readAll();
    file.close();

    if (bytes.size() != fileSize) {
        QMessageBox::critical(this, tr("Read Error"),
                              tr("The complete binary file could not be read."));
        return;
    }

    const BinToMatConverter::DetectionResult detected =
        BinToMatConverter::autoDetectFormat(filePath);

    if (detected.matrixRows == 0 || detected.matrixCols == 0) {
        QMessageBox::warning(this, tr("Matrix Layout Not Determined"),
                             tr("The binary format was detected, but a matrix layout could not be determined automatically.\n\nDetection: %1")
                             .arg(detected.reason));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("BIN Contents - %1").arg(QFileInfo(filePath).fileName()));
    dialog.setWindowIcon(QIcon(":/icons/resources/icons/app_logo.svg"));
    dialog.setWindowFlags(Qt::Window | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    dialog.setMinimumSize(550, 380);

    QScreen *primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen) {
        QRect avail = primaryScreen->availableGeometry();
        int width = qMin(1000, static_cast<int>(avail.width() * 0.80));
        int height = qMin(650, static_cast<int>(avail.height() * 0.80));
        dialog.resize(width, height);
    } else {
        dialog.resize(850, 600);
    }

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *info = new QLabel(&dialog);
    info->setStyleSheet("QLabel { background-color: #f1f5f9; color: #0f172a; border: 1px solid #cbd5e1; border-radius: 4px; padding: 6px; font-size: 11px; }");
    info->setText(
        tr("<b>File:</b> %1 &nbsp;|&nbsp; <b>Size:</b> %2 bytes &nbsp;|&nbsp; <b>Detected Type:</b> %3 (%4)<br>"
           "<b>Matrix Layout:</b> %5 × %6 (%7 elements) &nbsp;|&nbsp; <b>Offset:</b> %8 bytes &nbsp;|&nbsp; <b>Detection:</b> %9")
        .arg(QFileInfo(filePath).fileName())
        .arg(fileSize)
        .arg(dataTypeName(detected.dataType))
        .arg(endianName(detected.endianness))
        .arg(detected.matrixRows)
        .arg(detected.matrixCols)
        .arg(detected.matrixRows * detected.matrixCols)
        .arg(detected.payloadOffset)
        .arg(detected.reason));
    info->setWordWrap(true);
    layout->addWidget(info);

    QTabWidget *tabs = new QTabWidget(&dialog);

    // Tab 1: Data (Matrix values table)
    QTableView *table = new QTableView(tabs);
    table->setModel(new BinMatrixModel(bytes, detected.dataType, detected.endianness,
                                       detected.matrixRows, detected.matrixCols,
                                       detected.payloadOffset, table));
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->horizontalHeader()->setDefaultSectionSize(75);
    table->verticalHeader()->setDefaultSectionSize(24);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    tabs->addTab(table, tr("Data"));

    // Tab 2: Hex (Raw Bytes Hex Dump)
    QTextEdit *hexTextEdit = new QTextEdit(tabs);
    hexTextEdit->setReadOnly(true);
    hexTextEdit->setStyleSheet("QTextEdit { font-family: 'Consolas', 'Courier New', monospace; font-size: 11px; background-color: #0f172a; color: #38bdf8; padding: 8px; }");

    QString hexDump;
    const qint64 maxHexBytes = qMin<qint64>(bytes.size(), 65536);
    hexDump.reserve(static_cast<size_t>(maxHexBytes * 4));

    hexDump += QString("Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\n");
    hexDump += QString("--------------------------------------------------------------------------------\n");

    for (qint64 i = 0; i < maxHexBytes; i += 16) {
        hexDump += QString("%1  ").arg(i, 8, 16, QChar('0')).toUpper();
        
        QString asciiPart;
        for (int j = 0; j < 16; ++j) {
            if (i + j < maxHexBytes) {
                unsigned char b = static_cast<unsigned char>(bytes.at(static_cast<qsizetype>(i + j)));
                hexDump += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
                asciiPart += (b >= 32 && b <= 126) ? static_cast<char>(b) : '.';
            } else {
                hexDump += "   ";
            }
            if (j == 7) hexDump += " ";
        }
        hexDump += QString(" |%1|\n").arg(asciiPart);
    }

    if (bytes.size() > maxHexBytes) {
        hexDump += QString("\n... [Showing first %1 bytes of %2 total raw bytes] ...\n").arg(maxHexBytes).arg(bytes.size());
    }

    hexTextEdit->setPlainText(hexDump);
    tabs->addTab(hexTextEdit, tr("Hex"));

    // Tab 3: Visual Plot with Color Shades Legend Box
    QWidget *plotTabContainer = new QWidget(tabs);
    QVBoxLayout *plotTabLayout = new QVBoxLayout(plotTabContainer);

    BinMatrixPlotWidget *plot = new BinMatrixPlotWidget(bytes, detected.dataType,
                                                         detected.endianness,
                                                         detected.matrixRows,
                                                         detected.matrixCols,
                                                         detected.payloadOffset,
                                                         plotTabContainer);
    plotTabLayout->addWidget(plot, 1);

    QLabel *colorLegendLabel = new QLabel(plotTabContainer);
    colorLegendLabel->setText(tr(
        "<div style=\"background-color: #f8fafc; border: 1px solid #cbd5e1; border-radius: 6px; padding: 10px; font-size: 12px; color: #1e293b;\">"
        "<b>Color Shades Legend (2D Matrix Heatmap Representation):</b><br/>"
        "• <span style=\"color: #0000ff; font-weight: bold;\">■ Deep Blue / Indigo:</span> Represents <b>Minimum Payload Values (Low Amplitude/Intensity)</b>.<br/>"
        "• <span style=\"color: #008080; font-weight: bold;\">■ Cyan / Teal:</span> Represents <b>Lower Mid-Range Values</b>.<br/>"
        "• <span style=\"color: #008000; font-weight: bold;\">■ Green / Emerald:</span> Represents <b>Mid-Range Baseline Values</b>.<br/>"
        "• <span style=\"color: #ff8c00; font-weight: bold;\">■ Yellow / Amber:</span> Represents <b>Upper Mid-Range Values</b>.<br/>"
        "• <span style=\"color: #ff0000; font-weight: bold;\">■ Bright Red / Crimson:</span> Represents <b>Maximum Payload Values (High Amplitude/Intensity)</b>."
        "</div>"
    ));
    colorLegendLabel->setWordWrap(true);
    plotTabLayout->addWidget(colorLegendLabel);

    tabs->addTab(plotTabContainer, tr("Visual Plot"));
    tabs->setCurrentWidget(plotTabContainer);

    layout->addWidget(tabs, 1);

    dialog.exec();
}

void MainWindow::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Select Export Directory"), QString(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (!dir.isEmpty()) {
        outputDirLineEdit->setText(dir);
    }
}

void MainWindow::onRemoveSelectedFiles() {
    if (isProcessing) return;
    QList<QListWidgetItem*> selectedItems = fileListWidget->selectedItems();
    if (selectedItems.isEmpty()) return;

    for (QListWidgetItem *item : selectedItems) {
        QString filePath = item->text();
        queuedFiles.removeAll(filePath);
        delete fileListWidget->takeItem(fileListWidget->row(item));
    }
    updateViewContentsButton();
}

void MainWindow::onClearQueue() {
    if (isProcessing) return;
    queuedFiles.clear();
    fileListWidget->clear();
    progressBar->setValue(0);
    updateViewContentsButton();
}

void MainWindow::setControlsEnabled(bool enabled) {
    browseButton->setEnabled(enabled);
    removeButton->setEnabled(enabled);
    selectDirBtn->setEnabled(enabled);
    clearButton->setEnabled(enabled);
    convertButton->setEnabled(enabled);
    updateViewContentsButton();
    if (!enabled && viewContentsButton) viewContentsButton->setEnabled(false);
    outputDirLineEdit->setEnabled(enabled);
    varNameLineEdit->setEnabled(enabled);
    dataTypeComboBox->setEnabled(enabled);
    endiannessComboBox->setEnabled(enabled);
    if (dimensionComboBox) dimensionComboBox->setEnabled(enabled);
    fileListWidget->setEnabled(enabled);
}

void MainWindow::onStartBatchConversion() {
    if (queuedFiles.isEmpty()) {
        QMessageBox::information(this, tr("Empty Queue"), tr("Add binary files before executing batch conversion."));
        return;
    }

    isProcessing = true;
    setControlsEnabled(false);
    logTextEdit->clear();
    logTextEdit->append(tr("Initiating conversion execution..."));

    BinToMatConverter::Settings baseSettings;
    baseSettings.firstDimension = dimensionComboBox ? dimensionComboBox->currentData().toULongLong() : 0ULL;
    baseSettings.varName = varNameLineEdit->text().trimmed();

    int selectedDataTypeVal = dataTypeComboBox->currentData().toInt();
    int selectedEndianVal = endiannessComboBox->currentData().toInt();

    QString customDir = outputDirLineEdit->text().trimmed();
    if (customDir.isEmpty()) {
        customDir = QDir::homePath() + "/Downloads/MatBin/output";
    }

    // Ensure default or custom export folder exists
    QDir().mkpath(customDir);

    QStringList fileListCopy = queuedFiles;

    QFuture<void> future = QtConcurrent::run([this, fileListCopy, baseSettings, selectedDataTypeVal, selectedEndianVal, customDir]() {
        try {
            const int totalFiles = fileListCopy.size();
            for (int i = 0; i < totalFiles; ++i) {
                const QString &binPath = fileListCopy.at(i);
                QFileInfo fileInfo(binPath);
                
                QString matFileName = fileInfo.completeBaseName() + ".mat";
                QString matPath = QDir(customDir).filePath(matFileName);

                BinToMatConverter::Settings fileSettings = baseSettings;

                // 1. Resolve DataType (Auto-Detect if -1, otherwise use user's explicit manual selection)
                if (selectedDataTypeVal == -1) {
                    BinToMatConverter::DetectionResult perFileDetect = BinToMatConverter::autoDetectFormat(binPath);
                    fileSettings.dataType = perFileDetect.dataType;
                } else {
                    fileSettings.dataType = static_cast<BinToMatConverter::DataType>(selectedDataTypeVal);
                }

                // 2. Resolve Endianness (Auto-Detect if -1, otherwise use user's explicit manual selection)
                if (selectedEndianVal == -1) {
                    BinToMatConverter::DetectionResult perFileDetect = BinToMatConverter::autoDetectFormat(binPath);
                    fileSettings.endianness = perFileDetect.endianness;
                } else {
                    fileSettings.endianness = static_cast<BinToMatConverter::Endianness>(selectedEndianVal);
                }

                BinToMatConverter converter;
                connect(&converter, &BinToMatConverter::progressUpdated, this, &MainWindow::onConversionProgress, Qt::QueuedConnection);
                connect(&converter, &BinToMatConverter::statusLogged, this, &MainWindow::onLogReceived, Qt::QueuedConnection);

                converter.processFile(binPath, matPath, fileSettings);

                int overallProgress = static_cast<int>((static_cast<double>(i + 1) / totalFiles) * 100.0);
                QMetaObject::invokeMethod(this, [this, overallProgress]() {
                    progressBar->setValue(overallProgress);
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception &e) {
            QString errorMsg = QString("Worker thread exception: %1").arg(e.what());
            QMetaObject::invokeMethod(this, [this, errorMsg]() {
                onLogReceived(errorMsg, true);
            }, Qt::QueuedConnection);
        } catch (...) {
            QMetaObject::invokeMethod(this, [this]() {
                onLogReceived("Critical unknown error in worker thread.", true);
            }, Qt::QueuedConnection);
        }
    });

    futureWatcher.setFuture(future);
}

void MainWindow::onConversionProgress(int percentage) {
    Q_UNUSED(percentage);
}

void MainWindow::onLogReceived(const QString &message, bool isError) {
    if (isError) {
        logTextEdit->append(QString("<font color=\"#D32F2F\">[ERROR] %1</font>").arg(message));
    } else {
        logTextEdit->append(QString("<font color=\"#388E3C\">[INFO] %1</font>").arg(message));
    }
}

void MainWindow::onWorkerFinished() {
    isProcessing = false;
    setControlsEnabled(true);
    logTextEdit->append(tr("\nBatch conversion pipeline concluded."));
    QMessageBox::information(this, tr("Execution Finished"), tr("Processing completed successfully."));
}