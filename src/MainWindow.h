#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStringList>
#include <QFutureWatcher>
#include "BinToMatConverter.h"

#include <atomic>

class QListWidget;
class QPushButton;
class QTextEdit;
class QProgressBar;
class QComboBox;
class QSpinBox;
class QLineEdit;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void onBrowseFiles();
    void onRemoveSelectedFiles();
    void onBrowseOutputDir();
    void onClearQueue();
    void onStartBatchConversion();
    void onConversionProgress(int percentage);
    void onLogReceived(const QString &message, bool isError);
    void onWorkerFinished();
    void onAboutApplication();

private:
    void setupUi();
    void setControlsEnabled(bool enabled);

    QListWidget *fileListWidget = nullptr;
    QPushButton *browseButton = nullptr;
    QPushButton *removeButton = nullptr;
    QPushButton *selectDirBtn = nullptr;
    QPushButton *clearButton = nullptr;
    QPushButton *convertButton = nullptr;
    QLineEdit *varNameLineEdit = nullptr;
    QLineEdit *outputDirLineEdit = nullptr;
    QTextEdit *logTextEdit = nullptr;
    QProgressBar *progressBar = nullptr;
    QComboBox *dataTypeComboBox = nullptr;
    QComboBox *endiannessComboBox = nullptr;
    QSpinBox *channelsSpinBox = nullptr;
    QSpinBox *payloadOffsetSpinBox = nullptr;

    QStringList queuedFiles;
    QFutureWatcher<void> futureWatcher;
    bool isProcessing = false;
    std::atomic<int> batchSuccessCount{0};
    std::atomic<int> batchFailureCount{0};
};

#endif // MAINWINDOW_H