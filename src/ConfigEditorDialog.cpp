#include "../include/ConfigEditorDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

ConfigEditorDialog::ConfigEditorDialog(const labelConfig &cfg, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Edit Label Config");
    QVBoxLayout *main = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout();

    spinTL = new QSpinBox(this); spinTL->setRange(1, 10000); spinTL->setValue(cfg.TL);
    spinTS = new QSpinBox(this); spinTS->setRange(1, 1000); spinTS->setValue(cfg.TS);
    spinPS = new QSpinBox(this); spinPS->setRange(1, 1000); spinPS->setValue(cfg.PS);
    spinTX = new QSpinBox(this); spinTX->setRange(0, 5000); spinTX->setValue(cfg.TX);
    spinTY = new QSpinBox(this); spinTY->setRange(0, 5000); spinTY->setValue(cfg.TY);
    spinPX = new QSpinBox(this); spinPX->setRange(0, 5000); spinPX->setValue(cfg.PX);
    spinPY = new QSpinBox(this); spinPY->setRange(0, 5000); spinPY->setValue(cfg.PY);
    spinSTX = new QSpinBox(this); spinSTX->setRange(0, 5000); spinSTX->setValue(cfg.STX);
    spinSTY = new QSpinBox(this); spinSTY->setRange(0, 5000); spinSTY->setValue(cfg.STY);
    spinXO = new QSpinBox(this); spinXO->setRange(0, 5000); spinXO->setValue(cfg.XO);

    form->addRow(new QLabel("Text Length (TL):"), spinTL);
    form->addRow(new QLabel("Text Size (TS):"), spinTS);
    form->addRow(new QLabel("Price Size (PS):"), spinPS);
    form->addRow(new QLabel("Text X (TX):"), spinTX);
    form->addRow(new QLabel("Text Y (TY):"), spinTY);
    form->addRow(new QLabel("Price X (PX):"), spinPX);
    form->addRow(new QLabel("Price Y (PY):"), spinPY);
    form->addRow(new QLabel("Substring X (STX):"), spinSTX);
    form->addRow(new QLabel("Substring Y (STY):"), spinSTY);
    form->addRow(new QLabel("X Offset (XO):"), spinXO);

    main->addLayout(form);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);
}

labelConfig ConfigEditorDialog::getConfig() const {
    labelConfig cfg;
    cfg.TL = spinTL->value();
    cfg.TS = spinTS->value();
    cfg.PS = spinPS->value();
    cfg.TX = spinTX->value();
    cfg.TY = spinTY->value();
    cfg.PX = spinPX->value();
    cfg.PY = spinPY->value();
    cfg.STX = spinSTX->value();
    cfg.STY = spinSTY->value();
    cfg.XO = spinXO->value();
    return cfg;
}
