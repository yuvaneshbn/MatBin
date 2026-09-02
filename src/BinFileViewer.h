#ifndef BIN_FILE_VIEWER_H
#define BIN_FILE_VIEWER_H

#include <QDialog>
#include <QString>
#include <QByteArray>
#include "BinToMatConverter.h"

class QTabWidget;
class QLabel;
class QPushButton;
class QWidget;

class BinFileViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit BinFileViewerDialog(const QString &filePath, QWidget *parent = nullptr);
    ~BinFileViewerDialog() override = default;

private slots:
    void onSaveGraphImage();
    void onSaveAscFile();

private:
    void setupUi();

    QString m_filePath;
    QByteArray m_bytes;
    BinToMatConverter::DetectionResult m_detected;
    QTabWidget *m_tabs = nullptr;
    QLabel *m_infoLabel = nullptr;
    QWidget *m_plotWidget = nullptr;
    QPushButton *m_saveGraphBtn = nullptr;
    QPushButton *m_saveAscBtn = nullptr;
};

#endif // BIN_FILE_VIEWER_H
