#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDialogButtonBox>
#include "labelSystem.h"

class ConfigEditorDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConfigEditorDialog(const labelConfig &cfg, QWidget *parent = nullptr);
    labelConfig getConfig() const;

private:
    QSpinBox *spinTL, *spinTS, *spinPS, *spinTX, *spinTY, *spinPX, *spinPY, *spinSTX, *spinSTY, *spinXO;
};
