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

    QSize standardIconSize(20, 20);
    browseButton->setIconSize(standardIconSize);
    removeButton->setIconSize(standardIconSize);
    clearButton->setIconSize(standardIconSize);

    fileBtnLayout->addWidget(browseButton);
    fileBtnLayout->addWidget(removeButton);
    fileBtnLayout->addWidget(clearButton);
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
    dataTypeComboBox->addItem(tr("32-bit Signed Integer (int32_t) - 4 bytes"), static_cast<int>(BinToMatConverter::DataType::Int32));
    dataTypeComboBox->addItem(tr("64-bit Signed Integer (int64_t) - 8 bytes"), static_cast<int>(BinToMatConverter::DataType::Int64));

    endiannessComboBox = new QComboBox();
    endiannessComboBox->addItem(tr("Auto-Detect (Smart Payload Inspector)"), -1);
    endiannessComboBox->addItem(tr("Little-Endian (x86-64 / ARM64 Native - Least significant byte stored first)"), static_cast<int>(BinToMatConverter::Endianness::LittleEndianMode));
    endiannessComboBox->addItem(tr("Big-Endian (Network Order) - Most significant byte stored first"), static_cast<int>(BinToMatConverter::Endianness::BigEndianMode));

    channelsSpinBox = new QSpinBox();
    channelsSpinBox->setRange(1, 1024);
    channelsSpinBox->setValue(1);

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
    configLayout->addRow(tr("Channels Per Frame(Dimensions):"), channelsSpinBox);
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
    connect(convertButton, &QPushButton::clicked, this, &MainWindow::onStartBatchConversion);
}

void MainWindow::onAboutApplication() {
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About MatBin Application"));
    dialog.setWindowIcon(QIcon(":/icons/resources/icons/app_logo.svg"));
    dialog.resize(680, 560);

    QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);

    QLabel *headerLabel = new QLabel(tr(
        "<h2>MatBin - Binary to MATLAB MAT File Converter v1.2.0</h2>"
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
    noteTextEdit->setHtml(tr(
        "<style>"
        "  body { font-size: 11px; font-family: sans-serif; line-height: 1.4; color: #2c3e50; margin: 0; padding: 4px; }"
        "  h3 { margin: 8px 0 4px 0; color: #1a252f; font-size: 12px; }"
        "  ul { margin: 4px 0 8px 20px; padding: 0; }"
        "  li { margin-bottom: 4px; }"
        "  p { margin: 4px 0; }"
        "  .note { font-style: italic; color: #7f8c8d; margin-top: 6px; }"
        "</style>"
        
        "<b>1. Batch Queue &amp; File Routing Mechanics</b><br/>"
        "Managing bulk data extraction requires a blend of automated batch processing and granular user overrides:<br/>"
        "• <b>Batch Queue (QListWidget):</b> Acts as the central container holding active export tasks or datasets. Users can reorder, add, or remove entries dynamically.<br/>"
        "• <b>Destination Routing (QFileDialog::getExistingDirectory):</b> Establishes the root directory target for automated batch outputs.<br/>"
        "• <b>Single-File Overrides (QFileDialog::getSaveFileName):</b> Intercepts the queue workflow for bespoke paths or custom filenames.<br/><br/>"
        
        "<b>2. Numerical Precision &amp; Stream Endianness</b><br/>"
        "Data serialization relies on strict type definitions and byte-ordering configurations:<br/>"
        "• <b>Memory Byte Widths (S<sub>p</sub> &isin; {4, 8}):</b> Controls allocation footprints: <code>Double64</code> / <code>Int64</code> ($S_p = 8$), <code>Single32</code> / <code>Int32</code> ($S_p = 4$).<br/>"
        "• <b>Stream Endianness:</b> Determines multi-byte serialization order (Little-Endian native vs. Big-Endian network order).<br/><br/>"
        
        "<b>3. Channel Layout &amp; Dimensionality (M)</b><br/>"
        "Data shaping dictates array interpretation:<br/>"
        "• <b>1D Vector Mode (1 &times; E):</b> Treats the dataset as a continuous stream of $E$ elements for single-sensor logs.<br/>"
        "• <b>2D Matrix Mode (M &times; N):</b> Structures data into $M$ parallel channels across $N$ samples per channel.<br/><br/>"
        
        "<b>4. MATLAB Workspace Variable Integration</b><br/>"
        "Bridges Qt applications with MATLAB environments via dynamic variable injection:<br/>"
        "• <b>Workspace Symbol (\"data\"):</b> Defines the exact variable name string used when injecting memory buffers.<br/>"
        "<p class=\"note\"><b>Note:</b> Ensure that matrix mode dimensions (M &times; N) match the expected array shape of the workspace symbol to prevent MATLAB dimensional mismatch errors.</p>"
    ));

    noteLayout->addWidget(noteTextEdit);
    dialogLayout->addWidget(noteGroup);

    QPushButton *closeBtn = new QPushButton(tr("Close"), &dialog);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialogLayout->addWidget(closeBtn);

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
}

void MainWindow::onClearQueue() {
    if (isProcessing) return;
    queuedFiles.clear();
    fileListWidget->clear();
    progressBar->setValue(0);
}

void MainWindow::setControlsEnabled(bool enabled) {
    browseButton->setEnabled(enabled);
    removeButton->setEnabled(enabled);
    selectDirBtn->setEnabled(enabled);
    clearButton->setEnabled(enabled);
    convertButton->setEnabled(enabled);
    outputDirLineEdit->setEnabled(enabled);
    varNameLineEdit->setEnabled(enabled);
    dataTypeComboBox->setEnabled(enabled);
    endiannessComboBox->setEnabled(enabled);
    channelsSpinBox->setEnabled(enabled);
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
    baseSettings.channelsCount = channelsSpinBox->value();
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