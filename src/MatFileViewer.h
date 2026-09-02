#ifndef MAT_FILE_VIEWER_H
#define MAT_FILE_VIEWER_H

#include <QDialog>
#include <QString>
#include <QVector>
#include <QtGlobal>
#include <matio.h>

class QComboBox;
class QLabel;
class QTabWidget;
class QPushButton;
class QWidget;

class MatFileViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit MatFileViewerDialog(const QString &filePath, QWidget *parent = nullptr);
    ~MatFileViewerDialog() override;

private slots:
    void onVariableChanged(int index);
    void onSaveGraphImage();
    void onSaveAscFile();

private:
    struct VariableInfo {
        QString name;
        int rank = 0;
        QVector<quint64> dims;
    };

    bool inspectFile(QString &errorMessage);
    bool loadVariable(const QString &name, QString &errorMessage);
    bool isNumericVariable(const matvar_t *var) const;
    void rebuildTabs();

    QString m_filePath;
    QVector<VariableInfo> m_variables;
    QComboBox *m_variableCombo = nullptr;
    QLabel *m_infoLabel = nullptr;
    QTabWidget *m_tabs = nullptr;
    QWidget *m_plotWidget = nullptr;
    QPushButton *m_saveGraphBtn = nullptr;
    QPushButton *m_saveAscBtn = nullptr;
    mat_t *m_matFile = nullptr;
    matvar_t *m_variable = nullptr;
};

#endif // MAT_FILE_VIEWER_H
