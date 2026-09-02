#include "MainWindow.h"
#include "MatFileViewer.h"
#include "BinFileViewer.h"
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

    contentViewerButton = new QPushButton(tr("Content Viewer..."), this);
    contentViewerButton->setIcon(QIcon(":/icons/resources/icons/browse.svg"));
    contentViewerButton->setIconSize(QSize(20, 20));
    contentViewerButton->setStyleSheet("font-weight: bold; min-height: 28px;");
    mainLayout->addWidget(contentViewerButton);

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::onBrowseFiles);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::onRemoveSelectedFiles);
    connect(selectDirBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::onClearQueue);
    connect(viewContentsButton, &QPushButton::clicked, this, &MainWindow::onViewContents);
    connect(contentViewerButton, &QPushButton::clicked, this, &MainWindow::onOpenMatContentViewer);
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
        "<h2>MatBin - Binary to MATLAB MAT File Converter v1.4.0</h2>"
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
        
        "<b>3. Dynamic Matrix Dimension Selection &amp; Storage Transposition</b><br/>"
        "Calculates all valid divisor factor shapes (R &times; C) for the binary payload. Auto-Detect defaults to optimal square/natural matrix dimensions (e.g. 3600 &times; 3600 for 12,960,000 elements), with manual dropdown overrides available. Matrix storage is automatically transposed to match MATLAB's column-major layout.<br/><br/>"
        
        "<b>4. Interactive Content Viewers &amp; Export Tools</b><br/>"
        "Features dedicated BIN and MAT Content Viewers with a 3-tab inspector window: Data numerical matrix table, virtual-paged dark Hex console, and 2D Visual Plot thermal heatmap with continuous color gradient and calculated numeric ranges. Includes one-click <b>Save Graph Image</b> (.png/.jpg/.bmp) and <b>Save as .asc</b> (ESRI ASCII Grid) matrix export.<br/><br/>"

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
    QList<QListWidgetItem*> selected = fileListWidget->selectedItems();
    if (selected.size() != 1) {
        QMessageBox::information(this, tr("Select One File"),
                                 tr("Select exactly one binary file from the Batch Queue."));
        return;
    }

    const QString filePath = selected.first()->text();
    if (filePath.isEmpty() || !QFile::exists(filePath)) {
        QMessageBox::critical(this, tr("File Error"),
                              tr("Selected binary file is invalid or does not exist."));
        return;
    }

    BinFileViewerDialog dialog(filePath, this);
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
    if (contentViewerButton) contentViewerButton->setEnabled(enabled);
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
    int selectedChannels = dimensionComboBox ? dimensionComboBox->currentData().toInt() : 0;
    baseSettings.channelsCount = selectedChannels;
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

    QFuture<void> future = QtConcurrent::run([this, fileListCopy, baseSettings, selectedDataTypeVal, selectedEndianVal, selectedChannels, customDir]() {
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

                // 3. Resolve Channels / Matrix Dimension
                if (selectedChannels <= 0) {
                    BinToMatConverter::DetectionResult perFileDetect = BinToMatConverter::autoDetectFormat(binPath);
                    fileSettings.channelsCount = perFileDetect.matrixRows > 0 ? static_cast<int>(perFileDetect.matrixRows) : 1;
                } else {
                    fileSettings.channelsCount = selectedChannels;
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

void MainWindow::onOpenMatContentViewer() {
    QString initialDir = outputDirLineEdit ? outputDirLineEdit->text().trimmed() : QString();
    if (initialDir.isEmpty() || !QDir(initialDir).exists()) {
        initialDir = QDir::homePath() + "/Downloads/MatBin/output";
    }
    QString matPath = QFileDialog::getOpenFileName(
        this,
        tr("Open MATLAB MAT File for Inspection"),
        initialDir,
        tr("MATLAB Files (*.mat);;All Files (*)")
    );
    if (matPath.isEmpty()) return;

    MatFileViewerDialog dialog(matPath, this);
    dialog.exec();
}