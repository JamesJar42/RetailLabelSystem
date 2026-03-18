#include <QDesktopServices>
#include <QInputDialog>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include "../include/DatabaseSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QApplication>

DatabaseSettingsDialog::DatabaseSettingsDialog(const QString &currentPath, const CSVMapping &currentMap, 
                                               Mode currentMode, const QString &merchantId, const QString &apiToken, const QString &clientId, bool isSandbox,
                                               shop *shopPtr, QWidget *parent)
    : QDialog(parent), dbPtr(shopPtr), oauthServer(nullptr)
{
    setWindowTitle("Database Connection Settings");
    resize(550, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    tabWidget = new QTabWidget(this);
    
    // ================== TAB 0: DEFAULT / INTERNAL ==================
    QWidget *tabDefault = new QWidget();
    QVBoxLayout *defaultLayout = new QVBoxLayout(tabDefault);
    QLabel *lblDefault = new QLabel("<b>Default Mode</b><br>The application uses its own internal database stored in 'resources/Database.csv'.<br>"
                                    "You can add products manually using the 'Add Product' button.<br>"
                                    "You can Import/Export CSV files via the File menu.", tabDefault);
    lblDefault->setWordWrap(true);
    defaultLayout->addWidget(lblDefault);
    defaultLayout->addStretch();
    tabWidget->addTab(tabDefault, "Default (Internal)");

    // ================== TAB 1: EXTERNAL CSV ==================
    QWidget *tabCSV = new QWidget();
    QVBoxLayout *csvLayout = new QVBoxLayout(tabCSV);
    
    // File Selection
    QGroupBox *grpFile = new QGroupBox("External Database Source (POS Export)", tabCSV);
    QVBoxLayout *fileLayout = new QVBoxLayout(grpFile);
    QLabel *lblInfo = new QLabel("Select the CSV file that your POS system exports to.\nThe application will watch this file for changes.", grpFile);
    lblInfo->setStyleSheet("color: gray;");
    lblInfo->setWordWrap(true);
    
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathEdit = new QLineEdit(grpFile);
    pathEdit->setText(currentPath);
    pathEdit->setPlaceholderText("C:/Path/To/POS/Inventory.csv");
    
    browseBtn = new QPushButton("Browse...", grpFile);
    connect(browseBtn, &QPushButton::clicked, this, &DatabaseSettingsDialog::browseFile);
    
    pathLayout->addWidget(pathEdit);
    pathLayout->addWidget(browseBtn);
    
    fileLayout->addWidget(lblInfo);
    fileLayout->addLayout(pathLayout);
    csvLayout->addWidget(grpFile);

    // Column Mapping
    QGroupBox *grpMap = new QGroupBox("CSV Column Mapping (0-based Index)", tabCSV);
    QFormLayout *form = new QFormLayout(grpMap);
    
    auto makeSpin = [&](int val) {
        QSpinBox *sb = new QSpinBox(grpMap);
        sb->setRange(0, 50); 
        sb->setValue(val);
        return sb;
    };

    spinBarcode = makeSpin(currentMap.barcodeCol);
    spinName = makeSpin(currentMap.nameCol);
    spinPrice = makeSpin(currentMap.priceCol);
    spinOrigPrice = makeSpin(currentMap.originalPriceCol);
    spinQty = makeSpin(currentMap.labelQuantityCol); 
    
    chkHeader = new QCheckBox("First row is a header (Skip it)", grpMap);
    chkHeader->setChecked(currentMap.hasHeader);

    // Tooltips
    spinBarcode->setToolTip("The column number containing the Barcode/SKU");
    spinName->setToolTip("The column number containing the Product Name");
    spinQty->setToolTip("If the POS exports 'Label Quantity' or 'Stock', map it here. Otherwise, set to a column that is always 0 or -1 if unsupported.");

    form->addRow(new QLabel("Barcode Column:"), spinBarcode);
    form->addRow(new QLabel("Name/Description Column:"), spinName);
    form->addRow(new QLabel("Price Column:"), spinPrice);
    form->addRow(new QLabel("Original/Was Price Column:"), spinOrigPrice);
    form->addRow(new QLabel("Label Quantity Column:"), spinQty);
    form->addRow(chkHeader);
    
    csvLayout->addWidget(grpMap);
    tabWidget->addTab(tabCSV, "External CSV (POS Sync)");

    // ================== TAB 2: CLOVER API ==================
    QWidget *tabClover = new QWidget();
    QVBoxLayout *cloverLayout = new QVBoxLayout(tabClover);
    
    QLabel *cloverInfo = new QLabel("Connect directly to your Clover POS via API.\nYou need to generate an API Token from your Clover Dashboard (Setup -> API Tokens).", tabClover);
    cloverInfo->setWordWrap(true);
    cloverLayout->addWidget(cloverInfo);
    
    QFormLayout *cloverForm = new QFormLayout();
    
    txtMerchantId = new QLineEdit(tabClover);
    txtMerchantId->setPlaceholderText("e.g. T96XXXXXXXXXX");
    txtMerchantId->setText(merchantId);

    txtClientId = new QLineEdit(tabClover);
    txtClientId->setPlaceholderText("App ID (Required for Production)");
    txtClientId->setText(clientId);
    
    txtApiToken = new QLineEdit(tabClover);
    txtApiToken->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    txtApiToken->setPlaceholderText("Paste your Clover API Token here");
    txtApiToken->setText(apiToken);
    
    cloverForm->addRow("Merchant ID:", txtMerchantId);
    cloverForm->addRow("App ID (Client ID):", txtClientId);
    cloverForm->addRow("API Token:", txtApiToken);

    // OAuth Generator (PKCE Flow + Local Server)
    QPushButton *btnGetToken = new QPushButton("Generate Token", tabClover);
    cloverForm->addRow("", btnGetToken);
    connect(btnGetToken, &QPushButton::clicked, [this](){
        QString appId = txtClientId->text().trimmed();
        bool isSandbox = chkSandbox->isChecked();
        if (appId.isEmpty()) {
            QMessageBox::warning(this, "Missing App ID", "Please enter the App ID (Client ID) from your Clover Developer Dashboard.");
            return;
        }

        // 1. Start Local Server
        if (oauthServer) { oauthServer->close(); delete oauthServer; }
        oauthServer = new QTcpServer(this);
        connect(oauthServer, &QTcpServer::newConnection, this, &DatabaseSettingsDialog::onOAuthConnection);
        
        // Try to listen on fixed port 3000 (usually allowed for dev)
        // If 3000 is taken, this will fail. For production we ideally need a fixed port registered in Clover.
        // Let's assume port 3000.
        if (!oauthServer->listen(QHostAddress::LocalHost, 3000)) {
             QMessageBox::warning(this, "Port Error", "Could not start local server on port 3000.\nMake sure no other app is using it.");
             return;
        }

        // 2. Generate PKCE Verifier and Challenge
        const QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~";
        this->pkceVerifier.clear();
        for(int i=0; i<128; i++) {
            this->pkceVerifier.append(chars.at(QRandomGenerator::global()->bounded(chars.length())));
        }
        
        QByteArray hash = QCryptographicHash::hash(this->pkceVerifier.toUtf8(), QCryptographicHash::Sha256);
        QString codeChallenge = QString(hash.toBase64()).replace("+", "-").replace("/", "_").replace("=", "");

        // 3. Launch Browser
        // redirect_uri must be http://localhost:3000/
        QString baseUrl = isSandbox ? "https://sandbox.dev.clover.com" : "https://www.clover.com";
        QString urlStr = QString("%1/oauth/v2/authorize?client_id=%2&response_type=code&code_challenge=%3&code_challenge_method=S256&redirect_uri=http://localhost:3000/")
                            .arg(baseUrl).arg(appId).arg(codeChallenge);
        
        QMessageBox::information(this, "Instructions", 
             "1. PREPARATION (Important for Sandbox/Test Merchants):\n"
             "   If your test merchant has no password (Global Developer Dashboard),\n"
             "   open your browser and 'Launch Dashboard' for that merchant FIRST.\n"
             "2. Click OK below to open the Authorization page.\n"
             "3. Since you are already logged in, it should skip the password screen.\n"
             "4. Authorize the app and valid credentials will be captured.");
             
        QDesktopServices::openUrl(QUrl(urlStr));
    });

    chkSandbox = new QCheckBox("Use Sandbox Environment (For Development)", tabClover);
    chkSandbox->setChecked(isSandbox);
    cloverForm->addRow("", chkSandbox);
    
    cloverLayout->addLayout(cloverForm);

    QPushButton *btnSync = new QPushButton("Sync with Clover", tabClover);
    connect(btnSync, &QPushButton::clicked, this, &DatabaseSettingsDialog::onSyncClover);
    cloverLayout->addWidget(btnSync);
    
    cloverLayout->addStretch();
    
    tabWidget->addTab(tabClover, "Clover POS Integration");

    // Set Initial Tab
    if (currentMode == Mode::Clover) {
        tabWidget->setCurrentIndex(2);
    } else if (currentMode == Mode::CSV) {
        tabWidget->setCurrentIndex(1);
    } else {
        tabWidget->setCurrentIndex(0);
    }

    mainLayout->addWidget(tabWidget);

    // --- Buttons ---
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void DatabaseSettingsDialog::browseFile() {
    QString fileName = QFileDialog::getOpenFileName(this, "Select POS Export File", pathEdit->text(), "CSV Files (*.csv);;Text Files (*.txt);;All Files (*.*)");
    if (!fileName.isEmpty()) {
        pathEdit->setText(fileName);
    }
}

QString DatabaseSettingsDialog::getDatabasePath() const {
    return pathEdit->text();
}

CSVMapping DatabaseSettingsDialog::getMapping() const {
    CSVMapping map;
    map.barcodeCol = spinBarcode->value();
    map.nameCol = spinName->value();
    map.priceCol = spinPrice->value();
    map.originalPriceCol = spinOrigPrice->value();
    map.labelQuantityCol = spinQty->value();
    map.hasHeader = chkHeader->isChecked();
    return map;
}

DatabaseSettingsDialog::Mode DatabaseSettingsDialog::getMode() const {
    if (tabWidget->currentIndex() == 2) return Mode::Clover;
    if (tabWidget->currentIndex() == 1) return Mode::CSV;
    return Mode::Default;
}

QString DatabaseSettingsDialog::getCloverMerchantId() const {
    return txtMerchantId->text().trimmed();
}

void DatabaseSettingsDialog::onOAuthConnection() {
    QTcpSocket *socket = oauthServer->nextPendingConnection();
    connect(socket, &QTcpSocket::readyRead, [this, socket](){
        QByteArray data = socket->readAll();
        QString request = QString::fromUtf8(data);
        
        // Parse GET /?code=...
        QString code;
        int idx = request.indexOf("code=");
        if (idx != -1) {
            int end = request.indexOf(" ", idx);
            if (end == -1) end = request.length();
            
            // Also check for '&' if there are multiple params
            int amp = request.indexOf("&", idx);
            if (amp != -1 && amp < end) end = amp;
            
            code = request.mid(idx + 5, end - (idx + 5));
        }

        // Parse merchant_id=...
        QString authMerchantId;
        int mIdx = request.indexOf("merchant_id=");
        if (mIdx != -1) {
            int end = request.indexOf(" ", mIdx);
            if (end == -1) end = request.length();
            int amp = request.indexOf("&", mIdx);
            if (amp != -1 && amp < end) end = amp;
            
            authMerchantId = request.mid(mIdx + 12, end - (mIdx + 12));
            if (!authMerchantId.isEmpty()) {
                txtMerchantId->setText(authMerchantId);
            }
        }

        // Send Response
        QString response = QString("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n"
                                   "<html><body style='font-family:sans-serif; text-align:center; padding:50px;'>"
                                   "<h1>Authorization Successful!</h1>"
                                   "<p>You simply authenticated the app.</p>"
                                   "<p>Merchant ID: <b>%1</b></p>"
                                   "<p>You can close this window and return to the application.</p>"
                                   "<script>setTimeout(function(){window.close();}, 3000);</script>"
                                   "</body></html>").arg(authMerchantId);

        socket->write(response.toUtf8());
        socket->flush();
        socket->disconnectFromHost(); // The server will close the connection after flush and client ack

        // Close server
        oauthServer->close();

        if (code.isEmpty()) {
             QMessageBox::warning(this, "Auth Error", "Could not find authorization code in callback.");
             return;
        }

        // Exchange Token
        QString appId = txtClientId->text().trimmed();
        bool isSandbox = chkSandbox->isChecked();
        
        QString baseUrl = isSandbox ? "https://sandbox.dev.clover.com" : "https://www.clover.com";

        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QNetworkRequest req(QUrl(baseUrl + "/oauth/v2/token"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        QJsonObject json;
        json["client_id"] = appId;
        json["code"] = code;
        json["code_verifier"] = this->pkceVerifier;
        
        QByteArray postData = QJsonDocument(json).toJson();

        QNetworkReply *reply = manager->post(req, postData);
        connect(reply, &QNetworkReply::finished, [this, reply, manager](){
             if (reply->error() == QNetworkReply::NoError) {
                QByteArray respData = reply->readAll();
                QJsonDocument respDoc = QJsonDocument::fromJson(respData);
                QString accessToken = respDoc.object().value("access_token").toString();
                if (!accessToken.isEmpty()) {
                    txtApiToken->setText(accessToken);
                    QMessageBox::information(this, "Success", "Authentication successful! Token saved.");
                } else {
                    QMessageBox::warning(this, "Error", "Failed to parse access token.");
                }
             } else {
                 QMessageBox::warning(this, "Exchange Error", reply->errorString());
             }
             reply->deleteLater();
             manager->deleteLater();
        });
    });
}

QString DatabaseSettingsDialog::getCloverToken() const {
    return txtApiToken->text().trimmed();
}

QString DatabaseSettingsDialog::getCloverClientId() const {
    return txtClientId->text().trimmed();
}


bool DatabaseSettingsDialog::getIsSandbox() const {
    return chkSandbox->isChecked();
}

void DatabaseSettingsDialog::onSyncClover() {
    if (!dbPtr) return;

    QString mId = txtMerchantId->text().trimmed();
    QString token = txtApiToken->text().trimmed();
    bool sandbox = chkSandbox->isChecked();

    if (mId.isEmpty() || token.isEmpty()) {
        QMessageBox::warning(this, "Missing Credentials", "Please enter a Merchant ID and API Token.");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool success = dbPtr->loadFromClover(mId.toStdString(), token.toStdString(), sandbox);
    QApplication::restoreOverrideCursor();

    if (success) {
        QMessageBox::information(this, "Success", "Successfully connected and synced inventory from Clover!");
    } else {
        QMessageBox::warning(this, "Sync Failed", "Failed to sync with Clover.\nCheck your credentials and internet connection.");
    }
}
