#include "../include/VisualSettingsDialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QLabel>

VisualSettingsDialog::VisualSettingsDialog(const QString &currentLogo, const QString &currentIcon, bool simpleMode, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Visual Customization");
    resize(500, 350);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Group: Label Customization
    QGroupBox *grpLabel = new QGroupBox("Label Preview Settings", this);
    QVBoxLayout *grpLayout1 = new QVBoxLayout(grpLabel);
    
    QLabel *info1 = new QLabel("Select a custom image file (PNG, JPG) to on the label.\n"
                               "Clear the field to use a blank white label.", grpLabel);
    info1->setStyleSheet("color: gray;");
    info1->setWordWrap(true);
    grpLayout1->addWidget(info1);

    QHBoxLayout *h1 = new QHBoxLayout();
    logoPathEdit = new QLineEdit(grpLabel);
    logoPathEdit->setText(currentLogo);
    logoPathEdit->setPlaceholderText("Default (Blank)");
    QPushButton *btnBrowse1 = new QPushButton("Browse...", grpLabel);
    connect(btnBrowse1, &QPushButton::clicked, this, &VisualSettingsDialog::browseLogo);
    h1->addWidget(logoPathEdit);
    h1->addWidget(btnBrowse1);
    
    QFormLayout *f1 = new QFormLayout();
    f1->addRow("Label Logo:", h1);
    grpLayout1->addLayout(f1);

    mainLayout->addWidget(grpLabel);

    // Group: App Appearance
    QGroupBox *grpApp = new QGroupBox("Application Appearance", this);
    QVBoxLayout *grpLayout2 = new QVBoxLayout(grpApp);
    
    QLabel *info2 = new QLabel("Select a custom icon (.ico, .png) for the application window.\n"
                               "Restart required for full effect.", grpApp);
    info2->setStyleSheet("color: gray;");
    
    QHBoxLayout *h2 = new QHBoxLayout();
    iconPathEdit = new QLineEdit(grpApp);
    iconPathEdit->setText(currentIcon);
    iconPathEdit->setPlaceholderText("Default");
    QPushButton *btnBrowse2 = new QPushButton("Browse...", grpApp);
    connect(btnBrowse2, &QPushButton::clicked, this, &VisualSettingsDialog::browseIcon);
    h2->addWidget(iconPathEdit);
    h2->addWidget(btnBrowse2);

    QFormLayout *f2 = new QFormLayout();
    f2->addRow("App Icon:", h2);
    grpLayout2->addWidget(info2);
    grpLayout2->addLayout(f2);
    
    mainLayout->addWidget(grpApp);

    // Group: Interface Options
    QGroupBox *grpUI = new QGroupBox("Interface Options", this);
    QVBoxLayout *grpLayout3 = new QVBoxLayout(grpUI);

    chkSimpleQueue = new QCheckBox("Use simple checkbox for Print Queue", grpUI);
    chkSimpleQueue->setChecked(simpleMode);
    chkSimpleQueue->setToolTip("If checked, the 'Print Qty' column becomes a simple checkbox (Print 1 / Don't Print).");
    grpLayout3->addWidget(chkSimpleQueue);

    mainLayout->addWidget(grpUI);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void VisualSettingsDialog::browseLogo() {
    QString file = QFileDialog::getOpenFileName(this, "Select Logo Image", logoPathEdit->text(), "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!file.isEmpty()) logoPathEdit->setText(file);
}

void VisualSettingsDialog::browseIcon() {
    QString file = QFileDialog::getOpenFileName(this, "Select App Icon", iconPathEdit->text(), "Icons (*.ico *.png *.icns *.jpg)");
    if (!file.isEmpty()) iconPathEdit->setText(file);
}

QString VisualSettingsDialog::getLogoPath() const {
    return logoPathEdit->text();
}

QString VisualSettingsDialog::getIconPath() const {
    return iconPathEdit->text();
}

bool VisualSettingsDialog::getSimpleQueueMode() const {
    return chkSimpleQueue->isChecked();
}
