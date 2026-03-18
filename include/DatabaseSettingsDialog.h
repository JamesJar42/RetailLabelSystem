#pragma once
#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include "shop.h" 

class DatabaseSettingsDialog : public QDialog {
    Q_OBJECT
public:
    enum class Mode { Default = 0, CSV = 1, Clover = 2 };

    explicit DatabaseSettingsDialog(const QString &currentPath, const CSVMapping &currentMap, 
                                    Mode currentMode, const QString &merchantId, const QString &apiToken, const QString &clientId, bool isSandbox,
                                    shop *shopPtr, QWidget *parent = nullptr);
    
    QString getDatabasePath() const;
    CSVMapping getMapping() const;
    
    Mode getMode() const;
    QString getCloverMerchantId() const;
    QString getCloverToken() const;
    QString getCloverClientId() const;
    bool getIsSandbox() const;

private slots:
    void onSyncClover();
    void onOAuthConnection();

private:
    shop *dbPtr;
    QTcpServer *oauthServer;
    QString pkceVerifier;
    QString currentOAuthState;

    // UI Elements
    QTabWidget *tabWidget;
    
    // CSV Tab
    QLineEdit *pathEdit;
    QPushButton *browseBtn;
    QSpinBox *spinBarcode;
    QSpinBox *spinName;
    QSpinBox *spinPrice;
    QSpinBox *spinOrigPrice;
    QSpinBox *spinQty;
    QCheckBox *chkHeader;

    // Clover Tab
    QLineEdit *txtMerchantId;
    QLineEdit *txtApiToken;
    QLineEdit *txtClientId;
    QCheckBox *chkSandbox;
    
    void browseFile();
};
