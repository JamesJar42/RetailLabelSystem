#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

class VisualSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit VisualSettingsDialog(const QString &currentLogo, const QString &currentIcon, bool simpleQueueMode, QWidget *parent = nullptr);
    QString getLogoPath() const;
    QString getIconPath() const;
    bool getSimpleQueueMode() const;

private:
    QLineEdit *logoPathEdit;
    QLineEdit *iconPathEdit;
    QCheckBox *chkSimpleQueue;

    void browseLogo();
    void browseIcon();
};
