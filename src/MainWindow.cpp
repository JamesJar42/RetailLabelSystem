#include "../include/MainWindow.h"
#include <QMenuBar>
#include <QMessageBox>
#include <QDebug>
#include <QHeaderView>
#include <QFileInfo>
#include <QDir>
#include <thread>
#include <QMetaObject>
#include <QInputDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QProcess>
#include <QIcon>
#include <QToolBar>
#include <QFont>
#include <QStyle>
#include <QColor>
#include <QSettings>
#include <QDialog>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QComboBox>
#include <QPushButton>
#include <QStatusBar>
#include <QSpinBox>
#include <QSet>
#include <QDialogButtonBox>
#include <QLabel>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTabWidget>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QDateTime>
#include <QTimer>
#include <QSharedPointer>
#include <algorithm>
#include <cmath>
#include <QShortcut>
#include <QKeySequence>
#include <functional>
#include "../include/AppUpdater.h"
#include "../include/ConfigEditorDialog.h"
#include "../include/ProductDelegate.h"
#include "../forms/ui_MainWindow.h"

// Icons are embedded in resources/icons.qrc and accessed as :/icons/<name>.svg

static CSVMapping getAppCSVMapping() {
    QSettings settings;
    CSVMapping map;
    map.barcodeCol = settings.value("db_col_barcode", 0).toInt();
    map.nameCol = settings.value("db_col_name", 1).toInt();
    map.priceCol = settings.value("db_col_price", 2).toInt();
    map.originalPriceCol = settings.value("db_col_original_price", 4).toInt();
    map.labelQuantityCol = settings.value("db_col_label_quantity", 5).toInt();
    map.hasHeader = settings.value("db_has_header", true).toBool();
    return map;
}

static QString generatePkceCodeVerifier();
static QString generatePkceCodeChallenge(const QString &verifier);

#ifndef RETAIL_LABELER_VERSION
#define RETAIL_LABELER_VERSION "1.0.0"
#endif

static const QString kUpdaterRepoFull = QStringLiteral("JamesJar42/RetailLabelSystem");
static const QString kUpdaterSourceFixed = QStringLiteral("github:JamesJar42/RetailLabelSystem");

bool MainWindow::ensureCloverTokenForUse(QString &tokenOut, QString &errorOut, bool forceRefresh)
{
    QSettings settings;
    tokenOut = settings.value("clover_token", "").toString().trimmed();
    const QString refreshToken = settings.value("clover_refresh_token", "").toString().trimmed();
    const qint64 expiresAt = settings.value("clover_token_expires_at", 0).toLongLong();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const bool nearExpiry = (expiresAt > 0) && (now >= (expiresAt - 90));

    if (tokenOut.isEmpty()) {
        errorOut = "Missing Clover access token. Please reconnect Clover.";
        return false;
    }

    const bool shouldRefresh = forceRefresh || nearExpiry;
    if (!shouldRefresh) {
        errorOut.clear();
        return true;
    }

    if (refreshToken.isEmpty()) {
        errorOut = "Clover token expired and no refresh token is stored. Please reconnect Clover.";
        return false;
    }

    const QString clientId = settings.value("clover_client_id", "").toString().trimmed();
    const QString clientSecret = settings.value("clover_client_secret", "").toString().trimmed();
    const bool isSandbox = settings.value("clover_sandbox", false).toBool();
    if (clientId.isEmpty()) {
        errorOut = "Clover client ID is missing. Please set it in Database Settings.";
        return false;
    }

    auto refreshed = ls->dtb.refreshCloverAccessToken(
        clientId.toStdString(),
        clientSecret.toStdString(),
        refreshToken.toStdString(),
        isSandbox);

    if (refreshed.first.empty()) {
        const std::string detail = ls->dtb.getLastOAuthError();
        errorOut = detail.empty()
            ? "Clover token refresh failed. Please reconnect Clover."
            : QString::fromStdString(detail);
        return false;
    }

    tokenOut = QString::fromStdString(refreshed.first).trimmed();
    settings.setValue("clover_token", tokenOut);

    const QString nextRefresh = QString::fromStdString(refreshed.second).trimmed();
    if (!nextRefresh.isEmpty()) {
        settings.setValue("clover_refresh_token", nextRefresh);
    }

    const int expiresIn = ls->dtb.getLastOAuthExpiresIn();
    if (expiresIn > 0) {
        settings.setValue("clover_token_expires_at", QDateTime::currentSecsSinceEpoch() + expiresIn);
    }

    errorOut.clear();
    return true;
}

MainWindow::MainWindow(labelSystem *labelSys, QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow()),
      ls(labelSys),
      currentQuickFilter("all"),
      searchBarInput(nullptr),
      resultsSummaryLabel(nullptr),
    batchActionBar(nullptr),
    undoActionBar(nullptr),
    undoActionLabel(nullptr),
    undoActionButton(nullptr),
    undoActionExpiryTimer(nullptr),
    undoActionCountdownTimer(nullptr),
    undoActionSecondsRemaining(0),
    openQueueWorkspaceAction(nullptr),
    appUpdater(nullptr),
    focusDebugEnabled(false)
{
    ui->setupUi(this);

    // Apply Custom Icon if set
    QSettings settings;
    QString customIcon = settings.value("custom_app_icon_path").toString();
    if (!customIcon.isEmpty() && QFile::exists(customIcon)) {
        setWindowIcon(QIcon(customIcon));
    } else {
        // Fallback to embedded resource if desired, or let system default take over
        // setWindowIcon(QIcon(":/icons/logo.ico")); 
    }

    // Apply a modern font and basic stylesheet for a cleaner look
    QFont baseFont("Segoe UI", 10);
    QApplication::setFont(baseFont);
    // Do not set a default stylesheet here; run.cpp loads the initial QSS so MainWindow
    // won't duplicate or override that logic. We still set fonts/palettes programmatically.

    appUpdater = new AppUpdater(this);
    {
        QString appVersion = QCoreApplication::applicationVersion().trimmed();
        if (appVersion.isEmpty()) {
            appVersion = QStringLiteral(RETAIL_LABELER_VERSION);
        }
        appUpdater->setCurrentVersion(appVersion);
        settings.setValue("updater/manifestUrl", kUpdaterSourceFixed);
        appUpdater->setUpdateSource(kUpdaterSourceFixed);
        const QString installerArgsText = settings.value("updater/installerArgs", "").toString().trimmed();
        appUpdater->setDefaultInstallerArguments(installerArgsText.split(' ', Qt::SkipEmptyParts));
        appUpdater->setSignaturePolicy(
            settings.value("updater/requireSignature", true).toBool(),
            settings.value("updater/expectedPublisher", "").toString());

        connect(appUpdater, &AppUpdater::updateAvailable, this,
                [this](const QString &latestVersion,
                       const QString &installerUrl,
                       const QString &releaseNotes,
                       const QString &sha256,
                   const QString &publisher,
                   const QString &sourceType,
                       bool userInitiated) {
            QSettings updaterSettings;
            const bool autoInstall = updaterSettings.value("updater/autoInstall", false).toBool();

            if (autoInstall && !userInitiated) {
                if (statusBar()) {
                    statusBar()->showMessage(QString("Update %1 found. Downloading installer...").arg(latestVersion), 5000);
                }
                appUpdater->downloadAndInstall(installerUrl, sha256, publisher, false);
                return;
            }

            QString message = QString("Version %1 is available.\n\nInstall this update now?").arg(latestVersion);
            if (!sourceType.trimmed().isEmpty()) {
                message += QString("\n\nSource: %1").arg(sourceType.trimmed());
            }
            if (!releaseNotes.trimmed().isEmpty()) {
                message += QString("\n\nRelease notes:\n%1").arg(releaseNotes.trimmed());
            }

            const QMessageBox::StandardButton choice = QMessageBox::question(
                this,
                "Update Available",
                message,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes);

            if (choice == QMessageBox::Yes) {
                if (statusBar()) {
                    statusBar()->showMessage("Downloading update...", 4000);
                }
                appUpdater->downloadAndInstall(installerUrl, sha256, publisher, true);
            }
        });

        connect(appUpdater, &AppUpdater::noUpdateAvailable, this,
                [this](const QString &currentVersion, bool userInitiated) {
            if (userInitiated) {
                QMessageBox::information(this,
                                         "No Update Found",
                                         QString("Retail Labeler is up to date (%1).")
                                             .arg(currentVersion.isEmpty() ? QStringLiteral(RETAIL_LABELER_VERSION) : currentVersion));
            } else if (statusBar()) {
                statusBar()->showMessage("Automatic update check complete: already up to date.", 2500);
            }
        });

        connect(appUpdater, &AppUpdater::checkFailed, this,
                [this](const QString &message, bool userInitiated) {
            if (userInitiated) {
                QMessageBox::warning(this, "Update Check Failed", message);
            } else {
                qDebug() << "Automatic update check failed:" << message;
            }
        });

        connect(appUpdater, &AppUpdater::installFailed, this,
                [this](const QString &message, bool) {
            QMessageBox::warning(this, "Update Install Failed", message);
        });

        connect(appUpdater, &AppUpdater::installStarted, this,
                [this](const QString &, bool userInitiated) {
            if (userInitiated) {
                QMessageBox::information(this,
                                         "Installer Started",
                                         "The installer has started. Retail Labeler will now close so the update can continue.");
            }
            qApp->quit();
        });
    }

    // Configure menus
    QMenuBar *menuBar = this->menuBar();
    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *printAction = fileMenu->addAction(QIcon::fromTheme("document-print"), "Print Labels (Preview)", [this]() {
        if (!ls) return;
        ls->viewLabels();

        // After viewLabels returns (it handles printing), ask to clear the queue
        // The console part of viewLabels handles this in non-GUI, but here we add a GUI prompt if viewLabels didn't already
        // clear them or if we want to enforce it.
        // Actually viewLabels in GUI mode (see labelSystem implementation) returns immediately and does NOT prompt, 
        // relying on this caller to do it.
        
        QMessageBox::StandardButton clearBtn = QMessageBox::question(this,
            "Clear Print Queue",
            "Clear the print queue (reset all quantities to 0)?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (clearBtn == QMessageBox::Yes) {
            ls->clearQueue();
            setLabelSystem(ls);
        }
    });
    printAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    printAction->setShortcutContext(Qt::ApplicationShortcut);

    openQueueWorkspaceAction = fileMenu->addAction(QIcon::fromTheme("view-list-details"), "Open Print Queue Workspace...", [this]() {
        if (!ls) return;

        QDialog dlg(this);
        dlg.setWindowTitle("Print Queue Workspace");
        dlg.resize(900, 560);

        QVBoxLayout *layout = new QVBoxLayout(&dlg);

        QLabel *summaryLabel = new QLabel(&dlg);
        summaryLabel->setObjectName("queueSummaryLabel");
        layout->addWidget(summaryLabel);

        QLabel *readinessLabel = new QLabel(&dlg);
        readinessLabel->setObjectName("queueReadinessLabel");
        readinessLabel->setWordWrap(true);
        layout->addWidget(readinessLabel);

        QTableWidget *queueTable = new QTableWidget(&dlg);
        queueTable->setColumnCount(4);
        queueTable->setHorizontalHeaderLabels({"Product Name", "Barcode", "Price", "Print Qty"});
        queueTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        queueTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
        queueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        queueTable->horizontalHeader()->setStretchLastSection(true);
        queueTable->verticalHeader()->setVisible(false);
        layout->addWidget(queueTable, 1);

        QCheckBox *clearAfterPrint = new QCheckBox("Clear queue after print preview closes", &dlg);
        clearAfterPrint->setChecked(true);
        layout->addWidget(clearAfterPrint);

        QLabel *lifecycleLabel = new QLabel("", &dlg);
        lifecycleLabel->setObjectName("queueLifecycleLabel");
        lifecycleLabel->setWordWrap(true);
        layout->addWidget(lifecycleLabel);

        QHBoxLayout *buttons = new QHBoxLayout;
        QPushButton *queueUpBtn = new QPushButton("Queue +1", &dlg);
        QPushButton *queueDownBtn = new QPushButton("Queue -1", &dlg);
        QPushButton *removeBtn = new QPushButton("Remove Selected", &dlg);
        QPushButton *clearBtn = new QPushButton("Clear Queue", &dlg);
        QPushButton *printBtn = new QPushButton("Print Queue", &dlg);
        QPushButton *closeBtn = new QPushButton("Close", &dlg);

        buttons->addWidget(queueUpBtn);
        buttons->addWidget(queueDownBtn);
        buttons->addWidget(removeBtn);
        buttons->addWidget(clearBtn);
        buttons->addStretch();
        buttons->addWidget(printBtn);
        buttons->addWidget(closeBtn);
        layout->addLayout(buttons);

        auto selectedBarcodes = [queueTable]() {
            QSet<QString> barcodes;
            const QList<QTableWidgetItem *> selectedItems = queueTable->selectedItems();
            for (QTableWidgetItem *it : selectedItems) {
                if (it) {
                    const QString bc = it->data(Qt::UserRole).toString();
                    if (!bc.isEmpty()) barcodes.insert(bc);
                }
            }
            return barcodes;
        };

        auto refreshQueueWorkspace = [this, queueTable, summaryLabel, readinessLabel, lifecycleLabel]() {
            if (!ls) return;

            const auto &products = ls->dtb.listProduct();
            const bool wasBlocked = queueTable->blockSignals(true);
            queueTable->setRowCount(0);

            int queuedProducts = 0;
            int totalLabels = 0;
            for (const product &pd : products) {
                if (pd.getLabelQuantity() <= 0) continue;

                const int row = queueTable->rowCount();
                queueTable->insertRow(row);

                QTableWidgetItem *nameItem = new QTableWidgetItem(QString::fromStdString(pd.getName()));
                QTableWidgetItem *barcodeItem = new QTableWidgetItem(QString::fromStdString(pd.getBarcode()));
                QTableWidgetItem *priceItem = new QTableWidgetItem(QString::number(pd.getPrice(), 'f', 2));
                QTableWidgetItem *qtyItem = new QTableWidgetItem(QString::number(pd.getLabelQuantity()));

                nameItem->setData(Qt::UserRole, barcodeItem->text());
                barcodeItem->setData(Qt::UserRole, barcodeItem->text());
                priceItem->setData(Qt::UserRole, barcodeItem->text());
                qtyItem->setData(Qt::UserRole, barcodeItem->text());

                queueTable->setItem(row, 0, nameItem);
                queueTable->setItem(row, 1, barcodeItem);
                queueTable->setItem(row, 2, priceItem);
                queueTable->setItem(row, 3, qtyItem);

                ++queuedProducts;
                totalLabels += pd.getLabelQuantity();
            }

            queueTable->blockSignals(wasBlocked);

            const labelConfig cfg = ls->getLabelConfig();
            const int columns = std::max(1, (2480 - cfg.XO) / 480);
            const int rows = std::max(1, 3508 / 320);
            const int labelsPerPage = std::max(1, columns * rows);
            const int estimatedPages = (totalLabels <= 0) ? 0 : ((totalLabels + labelsPerPage - 1) / labelsPerPage);

            summaryLabel->setText(QString("Queued products: %1 | Labels queued: %2 | Est. pages: %3")
                .arg(queuedProducts)
                .arg(totalLabels)
                .arg(estimatedPages));

            QString readiness = "Readiness: ";
            if (queuedProducts <= 0) {
                readiness += "No queued items. Add at least one product before printing.";
            } else if (totalLabels <= 0) {
                readiness += "Queue quantities are zero.";
            } else {
                readiness += "Ready to print. Verify page estimate and quantities.";
            }
            readinessLabel->setText(readiness);
            lifecycleLabel->setText("Cleanup note: temporary page deletion retries run after print preview closes.");
        };

        connect(queueUpBtn, &QPushButton::clicked, &dlg, [this, selectedBarcodes, refreshQueueWorkspace]() {
            if (!ls) return;
            const QSet<QString> barcodes = selectedBarcodes();
            for (const QString &bc : barcodes) {
                try {
                    product &p = ls->dtb.searchByBarcode(bc.toStdString());
                    p.setLabelQuantity(p.getLabelQuantity() + 1);
                } catch (...) {}
            }
            refreshQueueWorkspace();
            rebuildProductTable();
        });

        connect(queueDownBtn, &QPushButton::clicked, &dlg, [this, selectedBarcodes, refreshQueueWorkspace]() {
            if (!ls) return;
            const QSet<QString> barcodes = selectedBarcodes();
            for (const QString &bc : barcodes) {
                try {
                    product &p = ls->dtb.searchByBarcode(bc.toStdString());
                    p.setLabelQuantity(std::max(0, p.getLabelQuantity() - 1));
                } catch (...) {}
            }
            refreshQueueWorkspace();
            rebuildProductTable();
        });

        connect(removeBtn, &QPushButton::clicked, &dlg, [this, selectedBarcodes, refreshQueueWorkspace]() {
            if (!ls) return;
            const QSet<QString> barcodes = selectedBarcodes();
            if (barcodes.isEmpty()) return;

            const std::vector<product> snapshot = ls->dtb.listProduct();
            for (const QString &bc : barcodes) {
                ls->dtb.removeByBarcode(bc.toStdString());
            }

            queueUndoAction(QString("Queue workspace removed %1 product(s).").arg(barcodes.size()), [this, snapshot]() {
                if (!ls) return;
                ls->dtb.clear();
                for (const product &pd : snapshot) {
                    ls->dtb.add(pd);
                }
                rebuildProductTable();
            });

            refreshQueueWorkspace();
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Queue workspace: products removed.", 2200);
        });

        connect(clearBtn, &QPushButton::clicked, &dlg, [this, refreshQueueWorkspace]() {
            if (!ls) return;
            if (QMessageBox::question(this, "Clear Queue", "Clear all print queue quantities?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
                return;
            }

            const std::vector<product> snapshot = ls->dtb.listProduct();
            ls->clearQueue();
            queueUndoAction("Queue workspace cleared queue quantities.", [this, snapshot]() {
                if (!ls) return;
                ls->dtb.clear();
                for (const product &pd : snapshot) {
                    ls->dtb.add(pd);
                }
                rebuildProductTable();
            });

            refreshQueueWorkspace();
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Queue workspace: queue cleared.", 2200);
        });

        connect(printBtn, &QPushButton::clicked, &dlg, [this, summaryLabel, clearAfterPrint, refreshQueueWorkspace, lifecycleLabel]() {
            if (!ls) return;

            const auto &products = ls->dtb.listProduct();
            int totalLabels = 0;
            for (const product &pd : products) {
                totalLabels += std::max(0, pd.getLabelQuantity());
            }

            if (totalLabels <= 0) {
                QMessageBox::information(this, "Print Queue", "No labels queued. Add quantities before printing.");
                return;
            }

            QMessageBox::StandardButton confirm = QMessageBox::question(
                this,
                "Confirm Print",
                QString("Proceed with print preview?\n%1").arg(summaryLabel->text()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::Yes
            );
            if (confirm != QMessageBox::Yes) return;

            if (statusBar()) statusBar()->showMessage("Opening print preview...", 1800);
            ls->viewLabels();

            if (clearAfterPrint->isChecked()) {
                ls->clearQueue();
            }

            refreshQueueWorkspace();
            rebuildProductTable();
            lifecycleLabel->setText("Print preview closed. Cleanup retries are running in background (see resources/DeletionLog.txt if needed).");
            if (statusBar()) statusBar()->showMessage("Print flow completed. Cleanup retries running in background.", 3500);
        });

        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        refreshQueueWorkspace();
        dlg.exec();
    });
    openQueueWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    openQueueWorkspaceAction->setShortcutContext(Qt::ApplicationShortcut);

    fileMenu->addAction(QIcon::fromTheme("application-exit"), "Exit", this, &QWidget::close);

    QAction *saveAction = fileMenu->addAction(QIcon::fromTheme("document-save"), "Save", [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "Save Product List", "", "CSV Files (*.csv)");
        if (fileName.isEmpty()) return;

        if (!ls->dtb.saveToCSV(fileName.toStdString())) {
            QMessageBox::warning(this, "Save Error", "Unable to save file.");
        }
    });
    saveAction->setShortcut(QKeySequence::Save);
    saveAction->setShortcutContext(Qt::ApplicationShortcut);

    QAction *loadAction = fileMenu->addAction(QIcon::fromTheme("document-open"), "Load", [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Load Product List", "", "CSV Files (*.csv)");
        if (fileName.isEmpty()) return;

        if (!ls->dtb.loadFromCSV(fileName.toStdString(), getAppCSVMapping())) {
            QMessageBox::warning(this, "Load Error", "Unable to open or parse file.");
            return;
        }

        setLabelSystem(ls);
    });
    loadAction->setShortcut(QKeySequence::Open);
    loadAction->setShortcutContext(Qt::ApplicationShortcut);

    // Scan barcodes -> allow pasting/scanning multiple barcodes (one per line) and add to print queue
    QAction *scanAction = fileMenu->addAction(QIcon::fromTheme("system-search"), "Scan Barcodes...", [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Scan Barcodes");
        QVBoxLayout *layout = new QVBoxLayout(&dlg);
        
        QLabel *lbl = new QLabel("Scan barcode (Enter to process):", &dlg);
        layout->addWidget(lbl);
        
        QLineEdit *scanInput = new QLineEdit(&dlg);
        scanInput->setPlaceholderText("Scan...");
        layout->addWidget(scanInput);
        
        QLabel *qtyLabel = new QLabel("Quantity to add:", &dlg);
        layout->addWidget(qtyLabel);
        QSpinBox *qtyBox = new QSpinBox(&dlg);
        qtyBox->setRange(1, 999);
        qtyBox->setValue(1);
        layout->addWidget(qtyBox);
        
        QTextEdit *logObj = new QTextEdit(&dlg);
        logObj->setReadOnly(true);
        layout->addWidget(logObj);
        
        QPushButton *closeBtn = new QPushButton("Close", &dlg);
        layout->addWidget(closeBtn);
        
        connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
        
        auto processScan = [this, scanInput, qtyBox, logObj]() {
            QString input = scanInput->text().trimmed();
            if (input.isEmpty()) return;
            
            // Handle multiple lines if pasted
            QStringList lines = input.split(QRegularExpression("[\\r\\n]"), Qt::SkipEmptyParts);
            std::vector<std::string> barcodes;
            for(const QString &l : lines) {
                if(!l.trimmed().isEmpty()) barcodes.push_back(l.trimmed().toStdString());
            }
            
            int matched = 0;
            if(ls && !barcodes.empty()) {
                matched = ls->queueProducts(barcodes, qtyBox->value());
            }
            
            setLabelSystem(ls);
            
            // Log
            if (matched > 0) {
                 logObj->append(QString("Processed %1: Matched %2 products.").arg(input).arg(matched));
            } else {
                 logObj->append(QString("Processed %1: No match.").arg(input));
            }
            
            scanInput->clear();
            scanInput->setFocus();
        };

        connect(scanInput, &QLineEdit::returnPressed, processScan);
        
        // Also support a "Add" button just in case
        QPushButton *addBtn = new QPushButton("Add", &dlg);
        connect(addBtn, &QPushButton::clicked, processScan);
        layout->insertWidget(2, addBtn); // Insert after input
        
        scanInput->setFocus();
        dlg.exec();
    });
    scanAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    scanAction->setShortcutContext(Qt::ApplicationShortcut);

    QAction *clearQueueAction = fileMenu->addAction(QIcon::fromTheme("edit-clear"), "Clear Queue", [this]() {
        if (!ls) return;

        const std::vector<product> snapshot = ls->dtb.listProduct();
        ls->clearQueue();
        rebuildProductTable();
        if (statusBar()) statusBar()->showMessage("Queue cleared.", 2000);

        queueUndoAction("Queue cleared.", [this, snapshot]() {
            if (!ls) return;
            ls->dtb.clear();
            for (const product &pd : snapshot) {
                ls->dtb.add(pd);
            }
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Undo applied: queue restored.", 2200);
        });
    });
    clearQueueAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K));
    clearQueueAction->setShortcutContext(Qt::ApplicationShortcut);

    QMenu *helpMenu = menuBar->addMenu("Help");
    helpMenu->addAction(QIcon::fromTheme("help-about"), "About", []() {
        QMessageBox::about(nullptr, "About", "Retail Label System v1.0\nStyled Edition");
    });
    helpMenu->addAction("Keyboard Shortcuts", [this]() {
        QMessageBox::information(
            this,
            "Keyboard Shortcuts",
            "Ctrl+F: Focus search\n"
            "Ctrl+N: Add product\n"
            "Ctrl+P: Print labels\n"
            "Ctrl+Shift+P: Open queue workspace\n"
            "Ctrl+S: Save\n"
            "Ctrl+O: Load\n"
            "Ctrl+L: Scan barcodes\n"
            "Ctrl+Z: Undo last destructive action\n"
            "Del: Remove selected products\n"
            "Esc: Clear selection / leave field\n"
            "F6: Cycle focus regions"
        );
    });
    helpMenu->addSeparator();
    helpMenu->addAction("Check for Updates Now", [this]() {
        if (!appUpdater) return;

        QSettings updaterSettings;
        const QString installerArgs = updaterSettings.value("updater/installerArgs", "").toString().trimmed();
        const bool requireSignature = updaterSettings.value("updater/requireSignature", true).toBool();
        const QString expectedPublisher = updaterSettings.value("updater/expectedPublisher", "").toString().trimmed();
        updaterSettings.setValue("updater/manifestUrl", kUpdaterSourceFixed);

        if (statusBar()) {
            statusBar()->showMessage("Checking for updates (GitHub Releases)...", 3000);
        }

        appUpdater->setUpdateSource(kUpdaterSourceFixed);
        appUpdater->setDefaultInstallerArguments(installerArgs.split(' ', Qt::SkipEmptyParts));
        appUpdater->setSignaturePolicy(requireSignature, expectedPublisher);
        appUpdater->checkForUpdates(true);
    });

    QMenu *optionsMenu = menuBar->addMenu("Options");
    QAction *updaterSettingsAction = optionsMenu->addAction("Updater Settings...", [this]() {
        QSettings s;

        QDialog dlg(this);
        dlg.setWindowTitle("Updater Settings");
        dlg.resize(560, 360);

        QVBoxLayout *layout = new QVBoxLayout(&dlg);
        QLabel *intro = new QLabel(
            "Configure where updates are checked and how installer trust is validated.",
            &dlg);
        intro->setWordWrap(true);
        layout->addWidget(intro);

        QFormLayout *form = new QFormLayout;
        s.setValue("updater/manifestUrl", kUpdaterSourceFixed);
        QLabel *sourceValue = new QLabel(QString("github:%1").arg(kUpdaterRepoFull), &dlg);
        sourceValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QLineEdit *argsEdit = new QLineEdit(s.value("updater/installerArgs", "").toString(), &dlg);
        argsEdit->setPlaceholderText("Optional installer args, e.g. /S");
        QCheckBox *requireSigCheck = new QCheckBox("Require signed installer", &dlg);
        requireSigCheck->setChecked(s.value("updater/requireSignature", true).toBool());
        QLineEdit *publisherEdit = new QLineEdit(s.value("updater/expectedPublisher", "").toString(), &dlg);
        publisherEdit->setPlaceholderText("Optional signer match, e.g. CN=Retail Label Co");
        QCheckBox *autoCheck = new QCheckBox("Check for updates on startup", &dlg);
        autoCheck->setChecked(s.value("updater/autoCheck", true).toBool());
        QCheckBox *autoInstall = new QCheckBox("Auto-install update when found", &dlg);
        autoInstall->setChecked(s.value("updater/autoInstall", false).toBool());

        QLabel *hint = new QLabel(
            QString("Updater source is locked to github:%1.").arg(kUpdaterRepoFull),
            &dlg);
        hint->setWordWrap(true);

        form->addRow("Source", sourceValue);
        form->addRow("Installer args", argsEdit);
        form->addRow("Signer match", publisherEdit);
        form->addRow("", requireSigCheck);
        form->addRow("", autoCheck);
        form->addRow("", autoInstall);
        form->addRow("", hint);
        layout->addLayout(form);

        QLabel *status = new QLabel("", &dlg);
        status->setWordWrap(true);
        layout->addWidget(status);

        QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
        QPushButton *checkNowBtn = box->addButton("Check Now", QDialogButtonBox::ActionRole);
        layout->addWidget(box);

        connect(checkNowBtn, &QPushButton::clicked, &dlg, [this, argsEdit, requireSigCheck, publisherEdit, status]() {
            if (!appUpdater) return;

            appUpdater->setUpdateSource(kUpdaterSourceFixed);
            appUpdater->setDefaultInstallerArguments(argsEdit->text().trimmed().split(' ', Qt::SkipEmptyParts));
            appUpdater->setSignaturePolicy(requireSigCheck->isChecked(), publisherEdit->text().trimmed());
            status->setText("Checking for updates (GitHub Releases)...");
            status->setStyleSheet("color: #1565c0;");
            appUpdater->checkForUpdates(true);
        });

        connect(box, &QDialogButtonBox::accepted, &dlg, [&]() {
            s.setValue("updater/manifestUrl", kUpdaterSourceFixed);
            s.setValue("updater/installerArgs", argsEdit->text().trimmed());
            s.setValue("updater/requireSignature", requireSigCheck->isChecked());
            s.setValue("updater/expectedPublisher", publisherEdit->text().trimmed());
            s.setValue("updater/autoCheck", autoCheck->isChecked());
            s.setValue("updater/autoInstall", autoInstall->isChecked());

            if (appUpdater) {
                appUpdater->setUpdateSource(kUpdaterSourceFixed);
                appUpdater->setDefaultInstallerArguments(argsEdit->text().trimmed().split(' ', Qt::SkipEmptyParts));
                appUpdater->setSignaturePolicy(requireSigCheck->isChecked(), publisherEdit->text().trimmed());
            }

            dlg.accept();
        });
        connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        dlg.exec();
    });
    updaterSettingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_U));
    updaterSettingsAction->setShortcutContext(Qt::ApplicationShortcut);

    QAction *settingsWorkspaceAction = optionsMenu->addAction("Settings Workspace...", [this]() {
        QSettings settings;
        if (!ls) return;

        QDialog dlg(this);
        dlg.setWindowTitle("Settings Workspace");
        dlg.resize(860, 620);

        QVBoxLayout *root = new QVBoxLayout(&dlg);
        QLabel *intro = new QLabel("Unified settings for data source, visual preferences, label layout, and integrations.", &dlg);
        intro->setWordWrap(true);
        root->addWidget(intro);

        QTabWidget *tabs = new QTabWidget(&dlg);
        root->addWidget(tabs, 1);

        const QString dataTabTitle = "Data Source";
        const QString visualTabTitle = "Visual Theme";
        const QString layoutTabTitle = "Label Layout";
        const QString integrationsTabTitle = "Integrations";

        QWidget *dataTab = new QWidget(&dlg);
        QFormLayout *dataForm = new QFormLayout(dataTab);

        QComboBox *modeCombo = new QComboBox(dataTab);
        modeCombo->addItem("Default (Internal)", 0);
        modeCombo->addItem("CSV (External)", 1);
        modeCombo->addItem("Clover", 2);
        modeCombo->setCurrentIndex(std::max(0, modeCombo->findData(settings.value("db_mode", 0).toInt())));

        QWidget *dbPathRow = new QWidget(dataTab);
        QHBoxLayout *dbPathLayout = new QHBoxLayout(dbPathRow);
        dbPathLayout->setContentsMargins(0, 0, 0, 0);
        QLineEdit *dbPathEdit = new QLineEdit(settings.value("database_path", "").toString(), dbPathRow);
        QPushButton *dbBrowseBtn = new QPushButton("Browse", dbPathRow);
        dbPathLayout->addWidget(dbPathEdit, 1);
        dbPathLayout->addWidget(dbBrowseBtn);

        QSpinBox *barcodeCol = new QSpinBox(dataTab);
        QSpinBox *nameCol = new QSpinBox(dataTab);
        QSpinBox *priceCol = new QSpinBox(dataTab);
        QSpinBox *wasPriceCol = new QSpinBox(dataTab);
        QSpinBox *qtyCol = new QSpinBox(dataTab);
        for (QSpinBox *sb : { barcodeCol, nameCol, priceCol, wasPriceCol, qtyCol }) {
            sb->setRange(0, 30);
        }
        barcodeCol->setValue(settings.value("db_col_barcode", 0).toInt());
        nameCol->setValue(settings.value("db_col_name", 1).toInt());
        priceCol->setValue(settings.value("db_col_price", 2).toInt());
        wasPriceCol->setValue(settings.value("db_col_original_price", 4).toInt());
        qtyCol->setValue(settings.value("db_col_label_quantity", 5).toInt());

        QCheckBox *hasHeaderCheck = new QCheckBox("CSV has header row", dataTab);
        hasHeaderCheck->setChecked(settings.value("db_has_header", true).toBool());
        QLabel *csvHint = new QLabel("Used only in CSV mode. Provide a valid .csv path.", dataTab);
        csvHint->setWordWrap(true);

        dataForm->addRow("Data mode", modeCombo);
        dataForm->addRow("CSV file", dbPathRow);
        dataForm->addRow("Barcode column", barcodeCol);
        dataForm->addRow("Name column", nameCol);
        dataForm->addRow("Price column", priceCol);
        dataForm->addRow("Was price column", wasPriceCol);
        dataForm->addRow("Queue qty column", qtyCol);
        dataForm->addRow("", hasHeaderCheck);
        dataForm->addRow("", csvHint);
        tabs->addTab(dataTab, dataTabTitle);

        QWidget *visualTab = new QWidget(&dlg);
        QFormLayout *visualForm = new QFormLayout(visualTab);

        QWidget *logoRow = new QWidget(visualTab);
        QHBoxLayout *logoLayout = new QHBoxLayout(logoRow);
        logoLayout->setContentsMargins(0, 0, 0, 0);
        QLineEdit *logoEdit = new QLineEdit(settings.value("custom_logo_path", "").toString(), logoRow);
        QPushButton *logoBrowse = new QPushButton("Browse", logoRow);
        logoLayout->addWidget(logoEdit, 1);
        logoLayout->addWidget(logoBrowse);

        QWidget *iconRow = new QWidget(visualTab);
        QHBoxLayout *iconLayout = new QHBoxLayout(iconRow);
        iconLayout->setContentsMargins(0, 0, 0, 0);
        QLineEdit *iconEdit = new QLineEdit(settings.value("custom_app_icon_path", "").toString(), iconRow);
        QPushButton *iconBrowse = new QPushButton("Browse", iconRow);
        iconLayout->addWidget(iconEdit, 1);
        iconLayout->addWidget(iconBrowse);

        QCheckBox *simpleQueueCheck = new QCheckBox("Simple queue mode", visualTab);
        simpleQueueCheck->setChecked(settings.value("simple_queue_mode", false).toBool());
        QCheckBox *focusDebugCheck = new QCheckBox("Enable focus debug logging (developer)", visualTab);
        focusDebugCheck->setChecked(settings.value("accessibility/focusDebug", false).toBool());
        QLabel *assetHint = new QLabel("Optional assets: when set, files must exist.", visualTab);
        assetHint->setWordWrap(true);

        QComboBox *themeCombo = new QComboBox(visualTab);
        themeCombo->addItem("Light", "light");
        themeCombo->addItem("Dark", "dark");
        themeCombo->addItem("Custom QSS File", "file");
        themeCombo->setCurrentIndex(std::max(0, themeCombo->findData(settings.value("theme", "light").toString())));

        visualForm->addRow("Label logo", logoRow);
        visualForm->addRow("App icon", iconRow);
        visualForm->addRow("Theme", themeCombo);
        visualForm->addRow("", simpleQueueCheck);
        visualForm->addRow("", focusDebugCheck);
        visualForm->addRow("", assetHint);
        tabs->addTab(visualTab, visualTabTitle);

        QWidget *layoutTab = new QWidget(&dlg);
        QVBoxLayout *layoutTabLayout = new QVBoxLayout(layoutTab);
        labelConfig workingCfg = ls->getLabelConfig();
        QLabel *layoutSummary = new QLabel(layoutTab);
        layoutSummary->setWordWrap(true);
        auto refreshLayoutSummary = [layoutSummary, &workingCfg]() {
            layoutSummary->setText(QString("Current layout:\nTL=%1 TS=%2 PS=%3\nTX=%4 TY=%5\nPX=%6 PY=%7\nSTX=%8 STY=%9\nXO=%10")
                .arg(workingCfg.TL).arg(workingCfg.TS).arg(workingCfg.PS)
                .arg(workingCfg.TX).arg(workingCfg.TY)
                .arg(workingCfg.PX).arg(workingCfg.PY)
                .arg(workingCfg.STX).arg(workingCfg.STY)
                .arg(workingCfg.XO));
        };
        refreshLayoutSummary();

        QPushButton *editLayoutBtn = new QPushButton("Edit Layout...", layoutTab);
        connect(editLayoutBtn, &QPushButton::clicked, &dlg, [this, &workingCfg, refreshLayoutSummary]() {
            ConfigEditorDialog cfgDlg(workingCfg, this);
            if (cfgDlg.exec() == QDialog::Accepted) {
                workingCfg = cfgDlg.getConfig();
                refreshLayoutSummary();
            }
        });

        QCheckBox *nativePrintDialogCheck = new QCheckBox("Use native print dialog", layoutTab);
        nativePrintDialogCheck->setChecked(settings.value("print/useNativeDialog", false).toBool());
        QCheckBox *qtTextRenderCheck = new QCheckBox("Use Qt text rendering for labels", layoutTab);
        qtTextRenderCheck->setChecked(settings.value("print/useQtAddText", true).toBool());

        layoutTabLayout->addWidget(layoutSummary);
        layoutTabLayout->addWidget(editLayoutBtn);
        layoutTabLayout->addWidget(nativePrintDialogCheck);
        layoutTabLayout->addWidget(qtTextRenderCheck);
        layoutTabLayout->addStretch();
        tabs->addTab(layoutTab, layoutTabTitle);

        QWidget *integrationTab = new QWidget(&dlg);
        QFormLayout *integrationForm = new QFormLayout(integrationTab);
        QLineEdit *merchantEdit = new QLineEdit(settings.value("clover_merchant_id", "").toString(), integrationTab);
        QLineEdit *tokenEdit = new QLineEdit(settings.value("clover_token", "").toString(), integrationTab);
        tokenEdit->setEchoMode(QLineEdit::Password);
        
        QLineEdit *clientIdEdit = new QLineEdit(settings.value("clover_client_id", "").toString(), integrationTab);
        QLineEdit *clientSecretEdit = new QLineEdit(settings.value("clover_client_secret", "").toString(), integrationTab);
        clientSecretEdit->setEchoMode(QLineEdit::Password);
        
        QCheckBox *sandboxCheck = new QCheckBox("Use Clover sandbox", integrationTab);
        sandboxCheck->setChecked(settings.value("clover_sandbox", false).toBool());
        
        QPushButton *connectBtn = new QPushButton("Connect with Clover...", integrationTab);
        QPushButton *testConnectionBtn = new QPushButton("Test Clover Connection", integrationTab);
        QLabel *statusLabel = new QLabel("", integrationTab);
        statusLabel->setWordWrap(true);
        
        QLabel *cloverHint = new QLabel("Manual entry supported. Use 'Connect' for OAuth flow.", integrationTab);
        cloverHint->setWordWrap(true);

        QLabel *updaterSection = new QLabel("Updater", integrationTab);
        updaterSection->setStyleSheet("font-weight: 600;");
        settings.setValue("updater/manifestUrl", kUpdaterSourceFixed);
        QLabel *updaterSourceValue = new QLabel(QString("github:%1").arg(kUpdaterRepoFull), integrationTab);
        updaterSourceValue->setTextInteractionFlags(Qt::TextSelectableByMouse);
        QLineEdit *updaterArgsEdit = new QLineEdit(settings.value("updater/installerArgs", "").toString(), integrationTab);
        updaterArgsEdit->setPlaceholderText("Optional installer args, e.g. /S");
        QCheckBox *updaterRequireSignature = new QCheckBox("Require signed installer", integrationTab);
        updaterRequireSignature->setChecked(settings.value("updater/requireSignature", true).toBool());
        QLineEdit *updaterPublisherEdit = new QLineEdit(settings.value("updater/expectedPublisher", "").toString(), integrationTab);
        updaterPublisherEdit->setPlaceholderText("Optional signer match, e.g. CN=Retail Label Co");
        QCheckBox *updaterAutoCheck = new QCheckBox("Check for updates on startup", integrationTab);
        updaterAutoCheck->setChecked(settings.value("updater/autoCheck", true).toBool());
        QCheckBox *updaterAutoInstall = new QCheckBox("Auto-install update when found", integrationTab);
        updaterAutoInstall->setChecked(settings.value("updater/autoInstall", false).toBool());
        QPushButton *updaterCheckNowBtn = new QPushButton("Check for Updates Now", integrationTab);
        QLabel *updaterHint = new QLabel(
            QString("Updater source is locked to github:%1.").arg(kUpdaterRepoFull),
            integrationTab);
        updaterHint->setWordWrap(true);
        
        integrationForm->addRow("Merchant ID", merchantEdit);
        integrationForm->addRow("API Token", tokenEdit);
        integrationForm->addRow("Client ID", clientIdEdit);
        integrationForm->addRow("Client Secret", clientSecretEdit);
        integrationForm->addRow("", sandboxCheck);
        integrationForm->addRow("", connectBtn);
        integrationForm->addRow("", testConnectionBtn);
        integrationForm->addRow("", statusLabel);
        integrationForm->addRow("", cloverHint);
        integrationForm->addRow("", updaterSection);
        integrationForm->addRow("Source", updaterSourceValue);
        integrationForm->addRow("Installer args", updaterArgsEdit);
        integrationForm->addRow("Signer match", updaterPublisherEdit);
        integrationForm->addRow("", updaterRequireSignature);
        integrationForm->addRow("", updaterAutoCheck);
        integrationForm->addRow("", updaterAutoInstall);
        integrationForm->addRow("", updaterCheckNowBtn);
        integrationForm->addRow("", updaterHint);

        connect(updaterCheckNowBtn, &QPushButton::clicked, &dlg,
                [this, updaterArgsEdit, updaterRequireSignature, updaterPublisherEdit, statusLabel]() {
            if (!appUpdater) return;

            const QString args = updaterArgsEdit->text().trimmed();
            appUpdater->setUpdateSource(kUpdaterSourceFixed);
            appUpdater->setDefaultInstallerArguments(args.split(' ', Qt::SkipEmptyParts));
            appUpdater->setSignaturePolicy(updaterRequireSignature->isChecked(), updaterPublisherEdit->text().trimmed());

            statusLabel->setText("Checking for updates (GitHub Releases)...");
            statusLabel->setStyleSheet("color: #1565c0;");
            appUpdater->checkForUpdates(true);
        });

        connect(testConnectionBtn, &QPushButton::clicked, &dlg, [this, merchantEdit, tokenEdit, sandboxCheck, statusLabel]() {
             const QString merchant = merchantEdit->text().trimmed();
             const QString token = tokenEdit->text().trimmed();
             if (token.isEmpty()) {
                 statusLabel->setText("Enter API token before testing Clover connection.");
                 statusLabel->setStyleSheet("color: #b91c1c;");
                 return;
             }

             if (!ls) {
                 statusLabel->setText("Label system not available.");
                 statusLabel->setStyleSheet("color: #b91c1c;");
                 return;
             }

             statusLabel->setText("Testing Clover connection...");
             statusLabel->setStyleSheet("color: #1565c0;");

             QApplication::setOverrideCursor(Qt::WaitCursor);
             const bool ok = ls->dtb.validateCloverConnection(merchant.toStdString(), token.toStdString(), sandboxCheck->isChecked());
             QApplication::restoreOverrideCursor();

             if (ok) {
                 statusLabel->setText("Clover connection test passed.");
                 statusLabel->setStyleSheet("color: #15803d; font-weight: bold;");
             } else {
                 statusLabel->setText("Clover connection test failed: " + QString::fromStdString(ls->dtb.getLastOAuthError()));
                 statusLabel->setStyleSheet("color: #b91c1c;");
             }
        });
        
        connect(connectBtn, &QPushButton::clicked, &dlg, [this, clientIdEdit, clientSecretEdit, sandboxCheck, merchantEdit, tokenEdit, statusLabel, &dlg]() {
             const QStringList candidateDirs = {
                 QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
                 QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("RetailLabeler"),
                 QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("RetailLabeler"),
                 QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("RetailLabeler")
             };

             QString oauthLogPath;
             for (const QString &dirPath : candidateDirs) {
                 if (dirPath.trimmed().isEmpty()) continue;
                 QDir dir(dirPath);
                 if (!dir.exists() && !dir.mkpath(".")) continue;
                 const QString candidateLog = dir.filePath("oauth_debug.log");
                 QFile testFile(candidateLog);
                 if (testFile.open(QIODevice::Append | QIODevice::Text)) {
                     oauthLogPath = candidateLog;
                     testFile.close();
                     break;
                 }
             }

             auto logOAuth = [oauthLogPath](const QString &msg) {
                 const QString line = QDateTime::currentDateTime().toString(Qt::ISODate) + " " + msg;
                 qDebug() << line;
                 if (oauthLogPath.isEmpty()) return;
                 QFile f(oauthLogPath);
                 if (f.open(QIODevice::Append | QIODevice::Text)) {
                     QTextStream ts(&f);
                     ts << line << "\n";
                 }
             };

             QString cId = clientIdEdit->text().trimmed();
             QString cSec = clientSecretEdit->text().trimmed();
             if(cId.isEmpty()) {
                 statusLabel->setText("Please enter Client ID first.");
                 statusLabel->setStyleSheet("color: #b91c1c;");
                 logOAuth("Connect clicked but Client ID was empty.");
                 return;
             }
             bool isSandbox = sandboxCheck->isChecked();
             if (!oauthLogPath.isEmpty()) {
                 statusLabel->setText("OAuth log: " + QDir::toNativeSeparators(oauthLogPath));
                 statusLabel->setStyleSheet("color: #0f766e;");
             }
             logOAuth(QString("OAuth log path: %1").arg(oauthLogPath.isEmpty() ? "<none>" : oauthLogPath));
             logOAuth(QString("Connect clicked. sandbox=%1, clientIdLen=%2, clientSecretLen=%3")
                      .arg(isSandbox)
                      .arg(cId.length())
                      .arg(cSec.length()));

             const QString pkceVerifier = generatePkceCodeVerifier();
             const QString pkceChallenge = generatePkceCodeChallenge(pkceVerifier);
             const QString oauthState = QString::number(QRandomGenerator::global()->generate64(), 16);
             logOAuth(QString("Generated PKCE values. verifierLen=%1 challengeLen=%2")
                      .arg(pkceVerifier.length())
                      .arg(pkceChallenge.length()));
             logOAuth(QString("Generated OAuth state len=%1").arg(oauthState.length()));

             const QStringList authBases = isSandbox
                 ? QStringList{ "https://sandbox.dev.clover.com", "https://apisandbox.dev.clover.com" }
                 : QStringList{ "https://www.clover.com", "https://api.clover.com" };
             const QStringList redirectUris = {
                 "http://localhost:8080/",
                 "http://127.0.0.1:8080/"
             };
             auto buildAuthorizeUrl = [cId, pkceChallenge, oauthState](const QString &authBase, const QString &redirectUri) {
                 QUrl url(authBase + "/oauth/v2/authorize");
                 QUrlQuery query;
                 query.addQueryItem("client_id", cId);
                 query.addQueryItem("redirect_uri", redirectUri);
                 // Keep request aligned with Clover PKCE examples: minimal set.
                 query.addQueryItem("code_challenge", pkceChallenge);
                 Q_UNUSED(oauthState);
                 url.setQuery(query);
                 return url;
             };

             QList<QPair<QString, QString>> authAttemptMeta;
             for (const QString &base : authBases) {
                 for (const QString &redir : redirectUris) {
                     authAttemptMeta.push_back({base, redir});
                 }
             }

             QSettings oauthPrefSettings;
             const QString preferredAuthBase = oauthPrefSettings.value("clover_oauth_last_auth_base", "").toString();
             const QString preferredRedirect = oauthPrefSettings.value("clover_oauth_last_redirect_uri", "").toString();
             for (int i = 0; i < authAttemptMeta.size(); ++i) {
                 if (authAttemptMeta[i].first == preferredAuthBase && authAttemptMeta[i].second == preferredRedirect) {
                     authAttemptMeta.move(i, 0);
                     logOAuth(QString("Prioritizing last successful authorize attempt: base=%1 redirect=%2")
                              .arg(preferredAuthBase, preferredRedirect));
                     break;
                 }
             }

             // Keep retries lightweight now that we persist and prioritize the last known working route.
             const int maxAuthorizeAttempts = 3;
             if (authAttemptMeta.size() > maxAuthorizeAttempts) {
                 authAttemptMeta = authAttemptMeta.mid(0, maxAuthorizeAttempts);
             }

             QList<QUrl> authAttempts;
             for (const auto &attempt : authAttemptMeta) {
                 authAttempts.push_back(buildAuthorizeUrl(attempt.first, attempt.second));
             }

             QSharedPointer<int> attemptIndex(new int(0));
             QSharedPointer<bool> callbackHandled(new bool(false));
             
             QTcpServer *server = new QTcpServer(statusLabel);
             QString listenAddress = "Any";
             bool listening = server->listen(QHostAddress::Any, 8080);
             if (!listening) {
                 listenAddress = "LocalHost";
                 listening = server->listen(QHostAddress::LocalHost, 8080);
             }
             if (!listening) {
                 listenAddress = "LocalHostIPv6";
                 listening = server->listen(QHostAddress::LocalHostIPv6, 8080);
             }
             if(!listening) {
                 statusLabel->setText("Port 8080 busy. Cannot start listener.");
                 statusLabel->setStyleSheet("color: #b91c1c;");
                 logOAuth(QString("Failed to listen on localhost:8080. error=%1").arg(server->errorString()));
                 server->deleteLater();
                 return;
             }

             logOAuth(QString("Listening on localhost:8080 for OAuth callback. bind=%1").arg(listenAddress));

             QTimer *oauthTimeout = new QTimer(&dlg);
             oauthTimeout->setSingleShot(true);
             oauthTimeout->setInterval(12000);
             connect(oauthTimeout, &QTimer::timeout, &dlg, [server, statusLabel, logOAuth, authAttempts, attemptIndex, callbackHandled, oauthTimeout]() {
                 if (*callbackHandled) return;
                 if (server->isListening()) {
                     if ((*attemptIndex + 1) < authAttempts.size()) {
                         *attemptIndex = *attemptIndex + 1;
                         const QUrl retryUrl = authAttempts[*attemptIndex];
                         logOAuth(QString("No callback yet. Retrying authorize attempt %1/%2: %3")
                                  .arg(*attemptIndex + 1)
                                  .arg(authAttempts.size())
                                  .arg(retryUrl.toString()));
                         statusLabel->setText(QString("No callback yet. Retrying (%1/%2)...")
                                              .arg(*attemptIndex + 1)
                                              .arg(authAttempts.size()));
                         statusLabel->setStyleSheet("color: #b45309;");
                         QDesktopServices::openUrl(retryUrl);
                         oauthTimeout->start();
                     } else {
                         logOAuth("OAuth callback timeout reached after all endpoint attempts. Closing listener.");
                         server->close();
                         statusLabel->setText("OAuth timed out waiting for callback after all retries. Verify Clover redirect URI and allow localhost redirects.");
                         statusLabel->setStyleSheet("color: #b91c1c;");
                         server->deleteLater();
                     }
                 }
             });
             oauthTimeout->start();

             statusLabel->setText(QString("Waiting for browser authorization... (1/%1)").arg(authAttempts.size()));
             statusLabel->setStyleSheet("color: #1565c0;");
             
             QUrl authUrl = authAttempts[*attemptIndex];
             qDebug() << "Starting Clover OAuth authorize:" << authUrl;
             logOAuth(QString("Starting Clover OAuth authorize: %1").arg(authUrl.toString()));
             const bool browserOpened = QDesktopServices::openUrl(authUrl);
             logOAuth(QString("openUrl result=%1").arg(browserOpened));
             
             connect(server, &QTcpServer::newConnection, server, [server, this, cId, cSec, pkceVerifier, oauthState, isSandbox, merchantEdit, tokenEdit, statusLabel, logOAuth, callbackHandled, authAttemptMeta, attemptIndex]() {
                  logOAuth("Incoming TCP connection received on OAuth listener.");
                  while(server->hasPendingConnections()) {
                      QTcpSocket *socket = server->nextPendingConnection();
                      connect(socket, &QTcpSocket::readyRead, socket, [socket, server, this, cId, cSec, pkceVerifier, oauthState, isSandbox, merchantEdit, tokenEdit, statusLabel, logOAuth, callbackHandled, authAttemptMeta, attemptIndex]() {
                            // Accumulate data
                            QByteArray data = socket->readAll();
                            QByteArray buffer = socket->property("reqBuffer").toByteArray();
                            buffer.append(data);
                            socket->setProperty("reqBuffer", buffer);

                            // Check if headers are complete
                            if (!buffer.contains("\r\n\r\n")) return;
                            
                            QString reqStr = QString::fromUtf8(buffer);
                            const QString firstLine = reqStr.section("\r\n", 0, 0);
                            qDebug() << "OAuth callback request line:" << firstLine;
                            logOAuth(QString("OAuth callback request line: %1").arg(firstLine));
                            
                            // Ignore favicon requests to avoid processing them as auth
                            if (firstLine.startsWith("GET /favicon.ico")) {
                                logOAuth("Ignoring favicon request.");
                                socket->write("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n");
                                socket->flush();
                                socket->disconnectFromHost();
                                socket->deleteLater();
                                return;
                            }
                            
                               QString code;
                               QString callbackMerchantId;
                               const QString pathAndQuery = firstLine.section(' ', 1, 1);
                               QUrl callbackUrl("http://localhost:8080" + pathAndQuery);
                               QUrlQuery callbackQuery(callbackUrl);
                               code = callbackQuery.queryItemValue("code");
                               callbackMerchantId = callbackQuery.queryItemValue("merchant_id");
                               const QString stateFromCallback = callbackQuery.queryItemValue("state");
                               qDebug() << "OAuth callback parsed. code present:" << !code.isEmpty()
                                      << "merchant_id present:" << !callbackMerchantId.isEmpty();
                               logOAuth(QString("OAuth callback parsed. codePresent=%1 merchantIdPresent=%2 path=%3")
                                     .arg(!code.isEmpty())
                                     .arg(!callbackMerchantId.isEmpty())
                                     .arg(pathAndQuery));
                               if (!stateFromCallback.isEmpty() && stateFromCallback != oauthState) {
                                   logOAuth(QString("OAuth state mismatch. expected=%1 actual=%2").arg(oauthState, stateFromCallback));
                               }
                            
                            QString body = !code.isEmpty() ? 
                                "<html><head><title>Success</title></head><body><h1 style='color:green;font-family:sans-serif;'>Authorization Successful</h1><p>You can close this window and return to the application.</p><script>window.close();</script></body></html>" : 
                                "<html><head><title>Failed</title></head><body><h1 style='color:red;font-family:sans-serif;'>Authorization Failed</h1><p>No code found in callback.</p></body></html>";
                            
                            QByteArray bodyData = body.toUtf8();
                            QString response = QString("HTTP/1.1 200 OK\r\n"
                                                       "Content-Type: text/html; charset=utf-8\r\n"
                                                       "Content-Length: %1\r\n"
                                                       "Connection: close\r\n"
                                                       "\r\n").arg(bodyData.size());
                            
                            socket->write(response.toUtf8());
                            socket->write(bodyData);
                            socket->flush();
                            
                            // Close connection after writing
                            connect(socket, &QTcpSocket::bytesWritten, socket, [socket](qint64){
                                if(socket->bytesToWrite() == 0) {
                                    socket->disconnectFromHost();
                                    socket->deleteLater();
                                }
                            });
                            
                            // If we got a code, process it and stop listening
                            if(!code.isEmpty()) {
                                *callbackHandled = true;
                                if (*attemptIndex >= 0 && *attemptIndex < authAttemptMeta.size()) {
                                    QSettings s;
                                    s.setValue("clover_oauth_last_auth_base", authAttemptMeta[*attemptIndex].first);
                                    s.setValue("clover_oauth_last_redirect_uri", authAttemptMeta[*attemptIndex].second);
                                    logOAuth(QString("Saved successful authorize attempt: base=%1 redirect=%2")
                                             .arg(authAttemptMeta[*attemptIndex].first, authAttemptMeta[*attemptIndex].second));
                                }
                                logOAuth("Authorization code received. Closing listener and exchanging token.");
                                server->close();
                                server->deleteLater();
                                
                                statusLabel->setText("Exchanging code for token...");
                                
                                // Perform exchange on next loop to keep UI responsive
                                QMetaObject::invokeMethod(statusLabel, [this, cId, cSec, code, pkceVerifier, callbackMerchantId, isSandbox, merchantEdit, tokenEdit, statusLabel, logOAuth](){
                                     if(ls) {
                                         // Low-trust PKCE flow: exchange without client_secret first.
                                         auto res = ls->dtb.exchangeCloverAuthCode(
                                             cId.toStdString(),
                                             "",
                                             code.toStdString(),
                                             isSandbox,
                                             pkceVerifier.toStdString());

                                         // Compatibility fallback: if PKCE-only exchange fails and user provided
                                         // a client secret, try high-trust exchange once.
                                         if (res.first.empty() && !cSec.isEmpty()) {
                                             logOAuth("PKCE-only token exchange failed; retrying with client_secret fallback.");
                                             res = ls->dtb.exchangeCloverAuthCode(
                                                 cId.toStdString(),
                                                 cSec.toStdString(),
                                                 code.toStdString(),
                                                 isSandbox,
                                                 pkceVerifier.toStdString());
                                         }
                                         
                                         if(!res.first.empty()) {
                                             tokenEdit->setText(QString::fromStdString(res.first));
                                             if(!res.second.empty()) merchantEdit->setText(QString::fromStdString(res.second));
                                             if (merchantEdit->text().trimmed().isEmpty() && !callbackMerchantId.isEmpty()) {
                                                 merchantEdit->setText(callbackMerchantId);
                                             }

                                             QSettings s;
                                             s.setValue("clover_token", tokenEdit->text().trimmed());
                                             s.setValue("clover_merchant_id", merchantEdit->text().trimmed());
                                             const std::string refresh = ls->dtb.getLastOAuthRefreshToken();
                                             if (!refresh.empty()) {
                                                 s.setValue("clover_refresh_token", QString::fromStdString(refresh));
                                             }
                                             const int expiresIn = ls->dtb.getLastOAuthExpiresIn();
                                             if (expiresIn > 0) {
                                                 s.setValue("clover_token_expires_at", QDateTime::currentSecsSinceEpoch() + expiresIn);
                                             }

                                             statusLabel->setText("Authentication connection successful!");
                                             statusLabel->setStyleSheet("color: #15803d; font-weight: bold;");
                                             logOAuth("Token exchange succeeded and token field updated.");
                                         } else {
                                             statusLabel->setText("Error: Token exchange failed. See debug output for Clover response details.");
                                             statusLabel->setStyleSheet("color: #b91c1c;");
                                             logOAuth("Token exchange failed (empty access token result).");
                                             logOAuth(QString("Token exchange detail: %1").arg(QString::fromStdString(ls->dtb.getLastOAuthError())));
                                         }
                                     }
                                }, Qt::QueuedConnection);
                            } else {
                                // If no code found in this request (and not favicon), keep listening?
                                // Usually if it's not favicon and not code, it might be some other browser probe.
                                // We'll keep the server open.
                                logOAuth("Callback request did not include code. Listener remains open.");
                            }
                      });
                  }
             });
        });
        tabs->addTab(integrationTab, integrationsTabTitle);

        QLabel *validationSummary = new QLabel(&dlg);
        validationSummary->setWordWrap(true);
        root->addWidget(validationSummary);

        const QString normalInputStyle;
        const QString invalidInputStyle = "border: 1px solid #dc2626; background: #fff1f2;";
        int firstInvalidTab = -1;
        QWidget *firstInvalidWidget = nullptr;

        auto updateModeVisibility = [modeCombo, dbPathRow, barcodeCol, nameCol, priceCol, wasPriceCol, qtyCol, hasHeaderCheck, merchantEdit, tokenEdit, clientIdEdit, clientSecretEdit, sandboxCheck]() {
            const int mode = modeCombo->currentData().toInt();
            const bool csv = (mode == 1);
            const bool clover = (mode == 2);

            dbPathRow->setVisible(csv);
            barcodeCol->setVisible(csv);
            nameCol->setVisible(csv);
            priceCol->setVisible(csv);
            wasPriceCol->setVisible(csv);
            qtyCol->setVisible(csv);
            hasHeaderCheck->setVisible(csv);

            merchantEdit->setEnabled(clover);
            tokenEdit->setEnabled(clover);
            clientIdEdit->setEnabled(clover);
            clientSecretEdit->setEnabled(clover);
            sandboxCheck->setEnabled(clover);
        };
        connect(modeCombo, &QComboBox::currentIndexChanged, &dlg, updateModeVisibility);
        updateModeVisibility();

        auto updateValidationState = [&]() {
            const int mode = modeCombo->currentData().toInt();
            bool dataOk = true;
            bool visualOk = true;
            bool integrationsOk = true;
            firstInvalidTab = -1;
            firstInvalidWidget = nullptr;

            QStringList issues;

            dbPathEdit->setStyleSheet(normalInputStyle);
            merchantEdit->setStyleSheet(normalInputStyle);
            tokenEdit->setStyleSheet(normalInputStyle);
            iconEdit->setStyleSheet(normalInputStyle);
            logoEdit->setStyleSheet(normalInputStyle);
            csvHint->setStyleSheet(QString());
            assetHint->setStyleSheet(QString());
            cloverHint->setStyleSheet(QString());
            updaterHint->setStyleSheet(QString());

            if (mode == 1) {
                const QString csvPath = dbPathEdit->text().trimmed();
                if (csvPath.isEmpty()) {
                    dataOk = false;
                    issues << "CSV mode requires a database file path.";
                    dbPathEdit->setStyleSheet(invalidInputStyle);
                    csvHint->setStyleSheet("color: #b91c1c;");
                    if (firstInvalidTab < 0) {
                        firstInvalidTab = 0;
                        firstInvalidWidget = dbPathEdit;
                    }
                } else if (!QFile::exists(csvPath)) {
                    dataOk = false;
                    issues << "Configured CSV file does not exist.";
                    dbPathEdit->setStyleSheet(invalidInputStyle);
                    csvHint->setStyleSheet("color: #b91c1c;");
                    if (firstInvalidTab < 0) {
                        firstInvalidTab = 0;
                        firstInvalidWidget = dbPathEdit;
                    }
                }
            }

            if (mode == 2) {
                if (merchantEdit->text().trimmed().isEmpty()) {
                    integrationsOk = false;
                    issues << "Clover mode requires Merchant ID.";
                    merchantEdit->setStyleSheet(invalidInputStyle);
                    cloverHint->setStyleSheet("color: #b91c1c;");
                    if (firstInvalidTab < 0) {
                        firstInvalidTab = 3;
                        firstInvalidWidget = merchantEdit;
                    }
                }
                if (tokenEdit->text().trimmed().isEmpty()) {
                    integrationsOk = false;
                    issues << "Clover mode requires API token.";
                    tokenEdit->setStyleSheet(invalidInputStyle);
                    cloverHint->setStyleSheet("color: #b91c1c;");
                    if (firstInvalidTab < 0) {
                        firstInvalidTab = 3;
                        firstInvalidWidget = tokenEdit;
                    }
                }
            }

            if (!iconEdit->text().trimmed().isEmpty() && !QFile::exists(iconEdit->text().trimmed())) {
                visualOk = false;
                issues << "Custom app icon path is set but file is missing.";
                iconEdit->setStyleSheet(invalidInputStyle);
                assetHint->setStyleSheet("color: #b91c1c;");
                if (firstInvalidTab < 0) {
                    firstInvalidTab = 1;
                    firstInvalidWidget = iconEdit;
                }
            }
            if (!logoEdit->text().trimmed().isEmpty() && !QFile::exists(logoEdit->text().trimmed())) {
                visualOk = false;
                issues << "Custom label logo path is set but file is missing.";
                logoEdit->setStyleSheet(invalidInputStyle);
                assetHint->setStyleSheet("color: #b91c1c;");
                if (firstInvalidTab < 0) {
                    firstInvalidTab = 1;
                    firstInvalidWidget = logoEdit;
                }
            }

            const QString updaterUrl = kUpdaterSourceFixed;
            const bool updaterEnabled = updaterAutoCheck->isChecked() || updaterAutoInstall->isChecked() || !updaterUrl.isEmpty();
            if (updaterEnabled) {
                if (updaterUrl.compare(kUpdaterSourceFixed, Qt::CaseInsensitive) != 0) {
                    integrationsOk = false;
                    issues << QString("Updater is locked to github:%1.").arg(kUpdaterRepoFull);
                    updaterHint->setStyleSheet("color: #b91c1c;");
                }
            }

            tabs->setTabText(0, dataOk ? dataTabTitle : (dataTabTitle + " (!)") );
            tabs->setTabText(1, visualOk ? visualTabTitle : (visualTabTitle + " (!)") );
            tabs->setTabText(2, layoutTabTitle);
            tabs->setTabText(3, integrationsOk ? integrationsTabTitle : (integrationsTabTitle + " (!)") );

            if (issues.isEmpty()) {
                validationSummary->setText("Validation: All settings look good.");
                validationSummary->setStyleSheet("color: #15803d;");
            } else {
                validationSummary->setText("Validation issues: " + issues.join("  "));
                validationSummary->setStyleSheet("color: #b91c1c;");
            }

            return issues.isEmpty();
        };

        connect(dbBrowseBtn, &QPushButton::clicked, &dlg, [dbPathEdit, this]() {
            const QString p = QFileDialog::getOpenFileName(this, "Select CSV Database", "", "CSV Files (*.csv)");
            if (!p.isEmpty()) dbPathEdit->setText(p);
        });
        connect(logoBrowse, &QPushButton::clicked, &dlg, [logoEdit, this]() {
            const QString p = QFileDialog::getOpenFileName(this, "Select Label Logo", "", "Images (*.png *.jpg *.jpeg *.bmp *.webp)");
            if (!p.isEmpty()) logoEdit->setText(p);
        });
        connect(iconBrowse, &QPushButton::clicked, &dlg, [iconEdit, this]() {
            const QString p = QFileDialog::getOpenFileName(this, "Select App Icon", "", "Icons (*.ico *.png *.svg)");
            if (!p.isEmpty()) iconEdit->setText(p);
        });

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
        QPushButton *saveBtn = qobject_cast<QPushButton *>(buttonBox->button(QDialogButtonBox::Save));
        if (saveBtn) {
            saveBtn->setDefault(true);
            saveBtn->setAutoDefault(true);
        }
        QPushButton *cancelBtn = qobject_cast<QPushButton *>(buttonBox->button(QDialogButtonBox::Cancel));
        if (cancelBtn) {
            cancelBtn->setAutoDefault(false);
        }
        QPushButton *exportBtn = buttonBox->addButton("Export Profile", QDialogButtonBox::ActionRole);
        QPushButton *importBtn = buttonBox->addButton("Import Profile", QDialogButtonBox::ActionRole);
        root->addWidget(buttonBox);

        QWidget::setTabOrder(modeCombo, dbPathEdit);
        QWidget::setTabOrder(dbPathEdit, barcodeCol);
        QWidget::setTabOrder(barcodeCol, nameCol);
        QWidget::setTabOrder(nameCol, priceCol);
        QWidget::setTabOrder(priceCol, wasPriceCol);
        QWidget::setTabOrder(wasPriceCol, qtyCol);
        QWidget::setTabOrder(qtyCol, hasHeaderCheck);
        QWidget::setTabOrder(hasHeaderCheck, logoEdit);
        QWidget::setTabOrder(logoEdit, iconEdit);
        QWidget::setTabOrder(iconEdit, themeCombo);
        QWidget::setTabOrder(themeCombo, simpleQueueCheck);
        QWidget::setTabOrder(simpleQueueCheck, focusDebugCheck);
        QWidget::setTabOrder(focusDebugCheck, nativePrintDialogCheck);
        QWidget::setTabOrder(nativePrintDialogCheck, qtTextRenderCheck);
        QWidget::setTabOrder(qtTextRenderCheck, merchantEdit);
        QWidget::setTabOrder(merchantEdit, tokenEdit);
        QWidget::setTabOrder(tokenEdit, clientIdEdit);
        QWidget::setTabOrder(clientIdEdit, clientSecretEdit);
        QWidget::setTabOrder(clientSecretEdit, sandboxCheck);
        QWidget::setTabOrder(sandboxCheck, saveBtn);

        auto syncSaveButtonEnabled = [&]() {
            const bool valid = updateValidationState();
            if (saveBtn) saveBtn->setEnabled(valid);
        };

        connect(modeCombo, &QComboBox::currentIndexChanged, &dlg, syncSaveButtonEnabled);
        connect(dbPathEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(merchantEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(tokenEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(clientIdEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(clientSecretEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(iconEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        connect(logoEdit, &QLineEdit::textChanged, &dlg, syncSaveButtonEnabled);
        syncSaveButtonEnabled();

        connect(exportBtn, &QPushButton::clicked, &dlg, [this]() {
            QSettings s;
            const QString path = QFileDialog::getSaveFileName(this, "Export Settings Profile", "settings-profile.json", "JSON Files (*.json)");
            if (path.isEmpty()) return;

            QJsonObject rootObj;
            const QStringList keys = s.allKeys();
            for (const QString &k : keys) {
                rootObj.insert(k, QJsonValue::fromVariant(s.value(k)));
            }

            QFile out(path);
            if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Export Failed", "Could not write the settings profile file.");
                return;
            }
            out.write(QJsonDocument(rootObj).toJson(QJsonDocument::Indented));
            out.close();
            QMessageBox::information(this, "Export Complete", "Settings profile exported.");
        });

        connect(importBtn, &QPushButton::clicked, &dlg, [&, this]() {
            QSettings s;
            const QString path = QFileDialog::getOpenFileName(this, "Import Settings Profile", "", "JSON Files (*.json)");
            if (path.isEmpty()) return;

            QFile in(path);
            if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Import Failed", "Could not open the profile file.");
                return;
            }

            QJsonParseError err;
            const QJsonDocument doc = QJsonDocument::fromJson(in.readAll(), &err);
            in.close();
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                QMessageBox::warning(this, "Import Failed", "Profile file is not valid JSON.");
                return;
            }

            const QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                s.setValue(it.key(), it.value().toVariant());
            }

            // Load imported values into the currently open form so users can review
            // and save immediately without closing/reopening the dialog.
            modeCombo->setCurrentIndex(std::max(0, modeCombo->findData(s.value("db_mode", 0).toInt())));
            dbPathEdit->setText(s.value("database_path", "").toString());
            barcodeCol->setValue(s.value("db_col_barcode", 0).toInt());
            nameCol->setValue(s.value("db_col_name", 1).toInt());
            priceCol->setValue(s.value("db_col_price", 2).toInt());
            wasPriceCol->setValue(s.value("db_col_original_price", 4).toInt());
            qtyCol->setValue(s.value("db_col_label_quantity", 5).toInt());
            hasHeaderCheck->setChecked(s.value("db_has_header", true).toBool());

            logoEdit->setText(s.value("custom_logo_path", "").toString());
            iconEdit->setText(s.value("custom_app_icon_path", "").toString());
            simpleQueueCheck->setChecked(s.value("simple_queue_mode", false).toBool());
            focusDebugCheck->setChecked(s.value("accessibility/focusDebug", false).toBool());
            themeCombo->setCurrentIndex(std::max(0, themeCombo->findData(s.value("theme", "light").toString())));

            nativePrintDialogCheck->setChecked(s.value("print/useNativeDialog", false).toBool());
            qtTextRenderCheck->setChecked(s.value("print/useQtAddText", true).toBool());

            merchantEdit->setText(s.value("clover_merchant_id", "").toString());
            tokenEdit->setText(s.value("clover_token", "").toString());
            clientIdEdit->setText(s.value("clover_client_id", "").toString());
            clientSecretEdit->setText(s.value("clover_client_secret", "").toString());
            sandboxCheck->setChecked(s.value("clover_sandbox", false).toBool());

            workingCfg.TL = s.value("printLayout/TL", workingCfg.TL).toInt();
            workingCfg.TS = s.value("printLayout/TS", workingCfg.TS).toInt();
            workingCfg.PS = s.value("printLayout/PS", workingCfg.PS).toInt();
            workingCfg.TX = s.value("printLayout/TX", workingCfg.TX).toInt();
            workingCfg.TY = s.value("printLayout/TY", workingCfg.TY).toInt();
            workingCfg.PX = s.value("printLayout/PX", workingCfg.PX).toInt();
            workingCfg.PY = s.value("printLayout/PY", workingCfg.PY).toInt();
            workingCfg.STX = s.value("printLayout/STX", workingCfg.STX).toInt();
            workingCfg.STY = s.value("printLayout/STY", workingCfg.STY).toInt();
            workingCfg.XO = s.value("printLayout/XO", workingCfg.XO).toInt();
            refreshLayoutSummary();

            updateModeVisibility();
            syncSaveButtonEnabled();

            QMessageBox::information(this, "Import Complete", "Profile imported and loaded into the current form.");
        });

        connect(buttonBox, &QDialogButtonBox::accepted, &dlg, [&]() {
            if (!updateValidationState()) {
                if (firstInvalidTab >= 0) tabs->setCurrentIndex(firstInvalidTab);
                if (firstInvalidWidget) firstInvalidWidget->setFocus(Qt::OtherFocusReason);
                QMessageBox::warning(this, "Validation", "Please fix settings marked with (!). ");
                return;
            }

            const int newMode = modeCombo->currentData().toInt();
            CSVMapping newMap;
            newMap.barcodeCol = barcodeCol->value();
            newMap.nameCol = nameCol->value();
            newMap.priceCol = priceCol->value();
            newMap.originalPriceCol = wasPriceCol->value();
            newMap.labelQuantityCol = qtyCol->value();
            newMap.hasHeader = hasHeaderCheck->isChecked();

            const QString newDbPath = dbPathEdit->text().trimmed();
            const QString newLogo = logoEdit->text().trimmed();
            const QString newIcon = iconEdit->text().trimmed();
            const QString newTheme = themeCombo->currentData().toString();

            settings.setValue("db_mode", newMode);
            settings.setValue("database_path", newDbPath);
            settings.setValue("db_col_barcode", newMap.barcodeCol);
            settings.setValue("db_col_name", newMap.nameCol);
            settings.setValue("db_col_price", newMap.priceCol);
            settings.setValue("db_col_original_price", newMap.originalPriceCol);
            settings.setValue("db_col_label_quantity", newMap.labelQuantityCol);
            settings.setValue("db_has_header", newMap.hasHeader);

            settings.setValue("custom_logo_path", newLogo);
            settings.setValue("custom_app_icon_path", newIcon);
            settings.setValue("simple_queue_mode", simpleQueueCheck->isChecked());
            settings.setValue("accessibility/focusDebug", focusDebugCheck->isChecked());
            settings.setValue("theme", newTheme);

            settings.setValue("print/useNativeDialog", nativePrintDialogCheck->isChecked());
            settings.setValue("print/useQtAddText", qtTextRenderCheck->isChecked());

            settings.setValue("clover_merchant_id", merchantEdit->text().trimmed());
            settings.setValue("clover_token", tokenEdit->text().trimmed());
            settings.setValue("clover_client_id", clientIdEdit->text().trimmed());
            settings.setValue("clover_client_secret", clientSecretEdit->text().trimmed());
            settings.setValue("clover_sandbox", sandboxCheck->isChecked());
            settings.setValue("updater/manifestUrl", kUpdaterSourceFixed);
            settings.setValue("updater/installerArgs", updaterArgsEdit->text().trimmed());
            settings.setValue("updater/requireSignature", updaterRequireSignature->isChecked());
            settings.setValue("updater/expectedPublisher", updaterPublisherEdit->text().trimmed());
            settings.setValue("updater/autoCheck", updaterAutoCheck->isChecked());
            settings.setValue("updater/autoInstall", updaterAutoInstall->isChecked());
            const std::string refresh = ls->dtb.getLastOAuthRefreshToken();
            if (!refresh.empty()) {
                settings.setValue("clover_refresh_token", QString::fromStdString(refresh));
            }
            const int expiresIn = ls->dtb.getLastOAuthExpiresIn();
            if (expiresIn > 0) {
                settings.setValue("clover_token_expires_at", QDateTime::currentSecsSinceEpoch() + expiresIn);
            }

            ls->setLabelConfig(workingCfg);
            if (!ls->saveConfig("resources/Config.txt")) {
                QMessageBox::warning(this, "Save Error", "Could not save label layout config.");
            }

            if (!newIcon.isEmpty() && QFile::exists(newIcon)) {
                setWindowIcon(QIcon(newIcon));
            }

            setTheme(newTheme);
            updateThemeMenuChecks();
            updateFocusDebugLogging();
            if (appUpdater) {
                appUpdater->setUpdateSource(kUpdaterSourceFixed);
                appUpdater->setDefaultInstallerArguments(updaterArgsEdit->text().trimmed().split(' ', Qt::SkipEmptyParts));
                appUpdater->setSignaturePolicy(updaterRequireSignature->isChecked(), updaterPublisherEdit->text().trimmed());
            }

            if (databaseWatcher && !databaseWatcher->files().isEmpty()) {
                databaseWatcher->removePaths(databaseWatcher->files());
            }

            if (newMode == 1) {
                if (!newDbPath.isEmpty() && QFile::exists(newDbPath)) {
                    if (!ls->dtb.loadFromCSV(newDbPath.toStdString(), newMap)) {
                        QMessageBox::warning(this, "Database Load", "Failed to load configured CSV file.");
                    } else if (databaseWatcher) {
                        databaseWatcher->addPath(newDbPath);
                    }
                }
            } else if (newMode == 2) {
                QString activeToken;
                QString tokenErr;
                if (!ensureCloverTokenForUse(activeToken, tokenErr, false)) {
                    QMessageBox::warning(this, "Clover Auth", tokenErr);
                    updateAddButtonState();
                    setLabelSystem(ls);
                    dlg.accept();
                    return;
                }
                QApplication::setOverrideCursor(Qt::WaitCursor);
                const bool ok = ls->dtb.loadFromClover(
                    merchantEdit->text().trimmed().toStdString(),
                    activeToken.toStdString(),
                    sandboxCheck->isChecked());
                QApplication::restoreOverrideCursor();
                if (!ok) {
                    QMessageBox::warning(this, "Clover Sync", "Clover synchronization failed. Check credentials and connectivity.");
                }
            } else {
                const QString internalDb = "resources/Database.csv";
                if (QFile::exists(internalDb)) {
                    ls->dtb.loadFromCSV(internalDb.toStdString(), getAppCSVMapping());
                    if (databaseWatcher) databaseWatcher->addPath(internalDb);
                }
            }

            updateAddButtonState();
            setLabelSystem(ls);
            dlg.accept();
        });
        connect(buttonBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        dlg.exec();
    });
    settingsWorkspaceAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    settingsWorkspaceAction->setShortcutContext(Qt::ApplicationShortcut);

    // Toolbar
    QToolBar *toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20,20));
    toolbar->addAction(printAction);
    toolbar->addAction(openQueueWorkspaceAction);
    toolbar->addAction(scanAction);
    toolbar->addAction(clearQueueAction);
    toolbar->addSeparator();
    toolbar->addAction(saveAction);
    toolbar->addAction(loadAction);

    // Toolbar appearance handled by global QSS

    // Theme toggle
    isHighContrast = false;
    // Theme submenu
    QMenu *themeMenu = optionsMenu->addMenu("Theme");
    // Make actions checkable so the current theme is visible
    themeLightAct = themeMenu->addAction("Light");
    themeLightAct->setCheckable(true);
    themeDarkAct = themeMenu->addAction("Dark");
    themeDarkAct->setCheckable(true);
    themeHighContrastAct = themeMenu->addAction("High Contrast");
    themeHighContrastAct->setCheckable(true);
    themeReloadAct = themeMenu->addAction("Reload Stylesheet");
    themeChooseCustomAct = themeMenu->addAction("Choose Custom Stylesheet...");
    connect(themeLightAct, &QAction::triggered, this, [this]() { setTheme("light"); updateThemeMenuChecks(); });
    connect(themeDarkAct, &QAction::triggered, this, [this]() { setTheme("dark"); updateThemeMenuChecks(); });
    connect(themeHighContrastAct, &QAction::triggered, this, [this]() { setTheme("high_contrast"); updateThemeMenuChecks(); });
    connect(themeReloadAct, &QAction::triggered, this, &MainWindow::reloadStylesheet);
    connect(themeChooseCustomAct, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Select Stylesheet", "", "QSS Files (*.qss);;All Files (*)");
        if (!fileName.isEmpty()) {
            // Persist custom path and apply immediately
            QSettings settings;
            settings.setValue("custom_qss", fileName);
            currentCustomQssPath = fileName;
            settings.setValue("theme", "file");
            updateThemeMenuChecks();
            reloadStylesheet();
            // Ensure watcher monitors the selected file or its directory
            if (qssWatcher) {
                if (QFile::exists(fileName) && !qssWatcher->files().contains(fileName)) qssWatcher->addPath(fileName);
                QString dir = QFileInfo(fileName).absolutePath();
                if (!qssWatcher->directories().contains(dir)) qssWatcher->addPath(dir);
            }
        }
    });

    // Setup QFileSystemWatcher to live-reload QSS files when edited.
    // Use the same candidate resolution as run.cpp: resources/, ../resources/, ../../resources/
    qssWatcher = new QFileSystemWatcher(this);
    QStringList themeFiles = { "style_light.qss", "style_dark.qss", "style_high_contrast.qss", "style.qss" };
    QStringList candidatesPrefix = { "resources/", "../resources/", "../../resources/" };
    QStringList addedPaths;
    for (const QString &name : themeFiles) {
        for (const QString &pref : candidatesPrefix) {
            QString p = pref + name;
            if (QFile::exists(p)) {
                if (!addedPaths.contains(p)) {
                    qssWatcher->addPath(p);
                    addedPaths << p;
                }
            }
        }
    }
    // If no explicit theme files were found, try watching the local resources directories (if they exist)
    if (addedPaths.isEmpty()) {
        for (const QString &pref : candidatesPrefix) {
            QString dir = pref;
            if (QFile::exists(dir) && !addedPaths.contains(dir)) {
                qssWatcher->addPath(dir);
                addedPaths << dir;
            }
        }
    }
    connect(qssWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onQssFileChanged);
    connect(qssWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onQssDirChanged);

    // Setup Database Watcher for Synchronization
    databaseWatcher = new QFileSystemWatcher(this);
    // Locate Database.csv (similar to QSS logic)
    QString dbPath = settings.value("database_path").toString();

    if (dbPath.isEmpty() || !QFile::exists(dbPath)) {
        QStringList dbCandidates = { "resources/Database.csv", "../resources/Database.csv", "../../resources/Database.csv" };
        // Also check relative to application directory
        dbCandidates << QCoreApplication::applicationDirPath() + "/resources/Database.csv";
        
        for(const QString &p : dbCandidates) {
            if(QFile::exists(p)) {
                dbPath = p;
                break;
            }
        }
    }
    
    if(!dbPath.isEmpty()) {
        databaseWatcher->addPath(dbPath);
        connect(databaseWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onDatabaseFileChanged);
        qDebug() << "Watching database file:" << dbPath;
        
        // Initial load
        if(ls->dtb.loadFromCSV(dbPath.toStdString(), getAppCSVMapping())) {
            // Cannot call setLabelSystem here easily if it is not yet fully constructed?
            // Actually this is constructor. It's safe if ui is setup.
            // But we need to call it at the END of constructor usually, or use a timer.
            // Let's defer it to end of constructor logic or call it here.
            // Since we are in constructor, virtual methods might be an issue? No, setLabelSystem is not virtual.
            // But let's see. `setLabelSystem` uses `ui` which is set up.
            // I'll call it at the end of constructor.
        }
    }

    // Configure product table (widget created by Designer)
    ui->productTable->setAccessibleName("Product table");
    ui->productTable->setColumnCount(5);
    ui->productTable->setHorizontalHeaderLabels({"Product Name", "Barcode", "Price", "Was Price", "Print Qty"});
    ui->productTable->horizontalHeader()->setStretchLastSection(true);
    ui->productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Edit triggers enabled via code above, removing NoEditTriggers
    // ui->productTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->productTable->setAlternatingRowColors(true);
    ui->productTable->setShowGrid(true);
    ui->productTable->verticalHeader()->setVisible(false);
    
    // Allow editing with constraints
    ui->productTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
    ui->productTable->setItemDelegate(new ProductDelegate(ui->productTable));
    
    connect(ui->productTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (!item || !ls) return;
        validateAndApplyCellEdit(item);
        updateBatchActionBar();
    });

    connect(ui->productTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        updateBatchActionBar();
    });

    // productTable appearance is controlled by global QSS; avoid per-widget stylesheet here
    // Let QSS control palette and font for the product table; remove programmatic overrides
    ui->productTable->setGridStyle(Qt::SolidLine);
    ui->productTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->productTable->horizontalHeader()->setFixedHeight(34);

    // Use Designer buttons (set icons for consistency if present)
    if (ui->addButton) ui->addButton->setIcon(QIcon::fromTheme("list-add"));
    if (ui->removeButton) ui->removeButton->setIcon(QIcon::fromTheme("list-remove"));
    if (ui->editButton) ui->editButton->setIcon(QIcon::fromTheme("document-edit"));
    if (ui->addButton) ui->addButton->setAccessibleName("Add product");

    // runtime-created flag button removed; Designer `ui->flagButton` is used instead

    // Phase 1 shell: search + quick filter controls and visible results summary.
    searchBarInput = new QLineEdit(this);
    searchBarInput->setObjectName("searchBar");
    searchBarInput->setPlaceholderText("Search products by name or barcode");
    searchBarInput->setAccessibleName("Product search");
    searchBarInput->setToolTip("Type to filter products by name or barcode");

    QComboBox *quickFilterCombo = new QComboBox(this);
    quickFilterCombo->setObjectName("quickFilterCombo");
    quickFilterCombo->addItem("All products", "all");
    quickFilterCombo->addItem("Queued only", "queued");
    quickFilterCombo->addItem("Price changes", "price_changed");
    quickFilterCombo->setAccessibleName("Quick filter");
    quickFilterCombo->setToolTip("Filter table rows by queue and price-change status");

    QPushButton *clearFilterButton = new QPushButton("Reset", this);
    clearFilterButton->setObjectName("clearFilterButton");
    clearFilterButton->setAccessibleName("Reset filters");
    clearFilterButton->setToolTip("Clear search text and quick filter");

    resultsSummaryLabel = new QLabel("0 shown", this);
    resultsSummaryLabel->setObjectName("resultsSummaryLabel");

    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(8);
    searchLayout->addWidget(searchBarInput, 1);
    searchLayout->addWidget(quickFilterCombo);
    searchLayout->addWidget(clearFilterButton);
    searchLayout->addWidget(resultsSummaryLabel);

    if (ui->topRow) {
        if (ui->topRow->layout()) {
            delete ui->topRow->layout();
        }
        ui->topRow->setLayout(searchLayout);
    }

    connect(searchBarInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        currentSearchQuery = text;
        rebuildProductTable();
    });

    connect(quickFilterCombo, &QComboBox::currentIndexChanged, this, [this, quickFilterCombo](int) {
        currentQuickFilter = quickFilterCombo->currentData().toString();
        rebuildProductTable();
    });

    connect(clearFilterButton, &QPushButton::clicked, this, [this, quickFilterCombo]() {
        currentSearchQuery.clear();
        currentQuickFilter = "all";
        if (searchBarInput) searchBarInput->clear();
        quickFilterCombo->setCurrentIndex(0);
        rebuildProductTable();
    });

    QWidget::setTabOrder(searchBarInput, quickFilterCombo);
    QWidget::setTabOrder(quickFilterCombo, clearFilterButton);
    QWidget::setTabOrder(clearFilterButton, ui->productTable);
    QWidget::setTabOrder(ui->productTable, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->removeButton);

    QTimer::singleShot(0, this, [this]() {
        if (searchBarInput) searchBarInput->setFocus(Qt::OtherFocusReason);
    });

    batchActionBar = new QFrame(this);
    batchActionBar->setObjectName("batchActionBar");
    batchActionBar->setAccessibleName("Batch actions");
    QHBoxLayout *batchLayout = new QHBoxLayout(batchActionBar);
    batchLayout->setContentsMargins(8, 6, 8, 6);
    batchLayout->setSpacing(8);

    QLabel *batchLabel = new QLabel("Batch actions", batchActionBar);
    QPushButton *batchQueueUp = new QPushButton("Queue +1", batchActionBar);
    QPushButton *batchQueueDown = new QPushButton("Queue -1", batchActionBar);
    QPushButton *batchClear = new QPushButton("Clear Qty", batchActionBar);
    QPushButton *batchRemove = new QPushButton("Remove Selected", batchActionBar);
    batchQueueUp->setToolTip("Increase print quantity for selected rows");
    batchQueueDown->setToolTip("Decrease print quantity for selected rows");
    batchClear->setToolTip("Set selected rows quantity to zero");
    batchRemove->setToolTip("Delete selected products");

    batchLayout->addWidget(batchLabel);
    batchLayout->addWidget(batchQueueUp);
    batchLayout->addWidget(batchQueueDown);
    batchLayout->addWidget(batchClear);
    batchLayout->addWidget(batchRemove);
    batchLayout->addStretch();

    if (ui->centralwidget && ui->centralwidget->layout()) {
        QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(ui->centralwidget->layout());
        if (mainLayout) {
            mainLayout->insertWidget(1, batchActionBar);

            undoActionBar = new QFrame(this);
            undoActionBar->setObjectName("undoActionBar");
            undoActionBar->setAccessibleName("Undo actions");
            QHBoxLayout *undoLayout = new QHBoxLayout(undoActionBar);
            undoLayout->setContentsMargins(8, 6, 8, 6);
            undoLayout->setSpacing(8);

            undoActionLabel = new QLabel("", undoActionBar);
            undoActionLabel->setObjectName("undoActionLabel");
            undoActionButton = new QPushButton("Undo", undoActionBar);
            undoActionButton->setObjectName("undoActionButton");

            undoLayout->addWidget(undoActionLabel);
            undoLayout->addStretch();
            undoLayout->addWidget(undoActionButton);

            mainLayout->insertWidget(2, undoActionBar);
            undoActionBar->hide();

            undoActionExpiryTimer = new QTimer(this);
            undoActionExpiryTimer->setSingleShot(true);
            connect(undoActionExpiryTimer, &QTimer::timeout, this, [this]() {
                clearUndoAction();
            });

            undoActionCountdownTimer = new QTimer(this);
            undoActionCountdownTimer->setSingleShot(false);
            undoActionCountdownTimer->setInterval(1000);
            connect(undoActionCountdownTimer, &QTimer::timeout, this, [this]() {
                if (undoActionSecondsRemaining > 0) {
                    --undoActionSecondsRemaining;
                }
                if (undoActionLabel && !undoActionMessage.isEmpty()) {
                    undoActionLabel->setText(QString("%1 Undo available for %2s.").arg(undoActionMessage).arg(undoActionSecondsRemaining));
                }
            });

            connect(undoActionButton, &QPushButton::clicked, this, [this]() {
                if (!pendingUndoAction) {
                    clearUndoAction();
                    return;
                }
                auto action = pendingUndoAction;
                clearUndoAction();
                action();
            });
        }
    }
    batchActionBar->hide();

    connect(batchQueueUp, &QPushButton::clicked, this, [this]() {
        if (!ls) return;
        QSet<QString> barcodes;
        const QList<QTableWidgetItem *> selected = ui->productTable->selectedItems();
        for (QTableWidgetItem *it : selected) {
            if (it) barcodes.insert(it->data(Qt::UserRole).toString());
        }
        for (const QString &bc : barcodes) {
            try {
                product &p = ls->dtb.searchByBarcode(bc.toStdString());
                p.setLabelQuantity(p.getLabelQuantity() + 1);
            } catch (...) {}
        }
        rebuildProductTable();
        statusBar()->showMessage("Batch action applied: queue increased.", 2500);
    });

    connect(batchQueueDown, &QPushButton::clicked, this, [this]() {
        if (!ls) return;
        QSet<QString> barcodes;
        const QList<QTableWidgetItem *> selected = ui->productTable->selectedItems();
        for (QTableWidgetItem *it : selected) {
            if (it) barcodes.insert(it->data(Qt::UserRole).toString());
        }
        for (const QString &bc : barcodes) {
            try {
                product &p = ls->dtb.searchByBarcode(bc.toStdString());
                p.setLabelQuantity(std::max(0, p.getLabelQuantity() - 1));
            } catch (...) {}
        }
        rebuildProductTable();
        statusBar()->showMessage("Batch action applied: queue reduced.", 2500);
    });

    connect(batchClear, &QPushButton::clicked, this, [this]() {
        if (!ls) return;
        QSet<QString> barcodes;
        const QList<QTableWidgetItem *> selected = ui->productTable->selectedItems();
        for (QTableWidgetItem *it : selected) {
            if (it) barcodes.insert(it->data(Qt::UserRole).toString());
        }
        if (barcodes.isEmpty()) {
            return;
        }

        const std::vector<product> snapshot = ls->dtb.listProduct();
        for (const QString &bc : barcodes) {
            try {
                product &p = ls->dtb.searchByBarcode(bc.toStdString());
                p.setLabelQuantity(0);
            } catch (...) {}
        }
        rebuildProductTable();
        statusBar()->showMessage("Batch action applied: queue cleared for selected rows.", 2500);

        queueUndoAction(QString("Cleared queue qty for %1 selected product(s).").arg(barcodes.size()), [this, snapshot]() {
            if (!ls) return;
            ls->dtb.clear();
            for (const product &pd : snapshot) {
                ls->dtb.add(pd);
            }
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Undo applied: quantities restored.", 2200);
        });
    });

    connect(batchRemove, &QPushButton::clicked, this, [this]() {
        if (!ls) return;
        QSet<QString> barcodes;
        const QList<QTableWidgetItem *> selected = ui->productTable->selectedItems();
        for (QTableWidgetItem *it : selected) {
            if (it) barcodes.insert(it->data(Qt::UserRole).toString());
        }
        if (barcodes.isEmpty()) {
            return;
        }

        const std::vector<product> snapshot = ls->dtb.listProduct();
        for (const QString &bc : barcodes) {
            ls->dtb.removeByBarcode(bc.toStdString());
        }
        rebuildProductTable();
        statusBar()->showMessage("Batch action applied: selected products removed.", 2500);

        queueUndoAction(QString("Removed %1 selected product(s).").arg(barcodes.size()), [this, snapshot]() {
            if (!ls) return;
            ls->dtb.clear();
            for (const product &pd : snapshot) {
                ls->dtb.add(pd);
            }
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Undo applied: removed products restored.", 2200);
        });
    });

    // Status bar
    QStatusBar *appStatusBar = this->statusBar();
    appStatusBar->showMessage("Ready");

    // Connect add/remove/edit buttons
    updateAddButtonState();

    auto removeSelectedProducts = [this](bool warnIfEmpty) {
        if (!ls || !ui || !ui->productTable) return;
        QSet<QString> barcodes;
        const QList<QTableWidgetItem *> selectedItems = ui->productTable->selectedItems();
        for (QTableWidgetItem *it : selectedItems) {
            if (it) barcodes.insert(it->data(Qt::UserRole).toString());
        }
        if (barcodes.isEmpty()) {
            if (warnIfEmpty) {
                QMessageBox::warning(this, "Remove Product", "Please select one or more products to remove.");
            }
            return;
        }

        const std::vector<product> snapshot = ls->dtb.listProduct();

        for (const QString &bc : barcodes) {
            ls->dtb.removeByBarcode(bc.toStdString());
        }
        rebuildProductTable();
        statusBar()->showMessage(QString("Removed %1 product(s).").arg(barcodes.size()), 2500);

        queueUndoAction(QString("Removed %1 product(s).").arg(barcodes.size()), [this, snapshot]() {
            if (!ls) return;
            ls->dtb.clear();
            for (const product &pd : snapshot) {
                ls->dtb.add(pd);
            }
            rebuildProductTable();
            if (statusBar()) statusBar()->showMessage("Undo applied: products restored.", 2200);
        });
    };

    if (ui->removeButton) {
        connect(ui->removeButton, &QPushButton::clicked, this, [removeSelectedProducts]() {
            removeSelectedProducts(true);
        });
    }

    QShortcut *focusSearchShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this);
    connect(focusSearchShortcut, &QShortcut::activated, this, [this]() {
        if (searchBarInput) {
            searchBarInput->setFocus(Qt::ShortcutFocusReason);
            searchBarInput->selectAll();
        }
    });

    QShortcut *addProductShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this);
    connect(addProductShortcut, &QShortcut::activated, this, [this]() {
        if (ui && ui->addButton && ui->addButton->isEnabled()) {
            ui->addButton->click();
        }
    });

    QShortcut *removeShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(removeShortcut, &QShortcut::activated, this, [removeSelectedProducts]() {
        removeSelectedProducts(false);
    });

    QShortcut *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escapeShortcut, &QShortcut::activated, this, [this]() {
        if (ui && ui->productTable && !ui->productTable->selectedItems().isEmpty()) {
            ui->productTable->clearSelection();
            statusBar()->showMessage("Selection cleared.", 1200);
            return;
        }
        if (searchBarInput && searchBarInput->hasFocus()) {
            searchBarInput->clearFocus();
        }
    });

    QShortcut *undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, [this]() {
        if (!pendingUndoAction) return;
        auto action = pendingUndoAction;
        clearUndoAction();
        action();
    });

    QShortcut *cycleFocusShortcut = new QShortcut(QKeySequence(Qt::Key_F6), this);
    connect(cycleFocusShortcut, &QShortcut::activated, this, [this]() {
        QVector<QWidget *> focusOrder;
        if (searchBarInput) focusOrder.append(searchBarInput);
        if (ui && ui->productTable) focusOrder.append(ui->productTable);
        if (batchActionBar && batchActionBar->isVisible()) focusOrder.append(batchActionBar);
        if (ui && ui->addButton) focusOrder.append(ui->addButton);

        if (focusOrder.isEmpty()) return;

        QWidget *current = QApplication::focusWidget();
        int idx = focusOrder.indexOf(current);
        int nextIdx = (idx < 0) ? 0 : (idx + 1) % focusOrder.size();
        focusOrder[nextIdx]->setFocus(Qt::ShortcutFocusReason);
        if (statusBar()) {
            statusBar()->showMessage(QString("Focus moved to %1").arg(focusOrder[nextIdx]->accessibleName().isEmpty() ? focusOrder[nextIdx]->objectName() : focusOrder[nextIdx]->accessibleName()), 1200);
        }
    });
/* Edit button disabled as direct table editing is now enabled
    if (ui->editButton) {
        connect(ui->editButton, &QPushButton::clicked, this, [this]() {
            auto selectedItems = ui->productTable->selectedItems();
            if (selectedItems.isEmpty()) {
                QMessageBox::warning(this, "Edit Product", "Please select a product to edit.");
                return;
            }

            int row = ui->productTable->row(selectedItems.first());
            QString barcode = ui->productTable->item(row, 1)->text();

            product &selectedProduct = ls->dtb.searchByBarcode(barcode.toStdString());

            bool ok;
            QString description = QInputDialog::getText(this, "Edit Product", "Description:", QLineEdit::Normal, QString::fromStdString(selectedProduct.getDescription()), &ok);
            if (ok && !description.isEmpty()) {
                selectedProduct.setDescription(description.toStdString());
            }

            float price = QInputDialog::getDouble(this, "Edit Product", "Price:", selectedProduct.getPrice(), 0, 10000, 2, &ok);
            if (ok) {
                selectedProduct.setPrice(price);
            }

            QString size = QInputDialog::getText(this, "Edit Product", "Size:", QLineEdit::Normal, QString::fromStdString(selectedProduct.getSize()), &ok);
            if (ok && !size.isEmpty()) {
                selectedProduct.setSize(size.toStdString());
            }

            QCheckBox *labelFlagCheckBox = new QCheckBox("Label Flag", this);
            labelFlagCheckBox->setChecked(selectedProduct.getLabelFlag());
            QMessageBox msgBox;
            msgBox.setWindowTitle("Edit Product");
            msgBox.setText("Set Label Flag:");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.layout()->addWidget(labelFlagCheckBox);
            int result = msgBox.exec();
            if (result == QMessageBox::Ok) {
                selectedProduct.setLabelFlag(labelFlagCheckBox->isChecked());
            }

            setLabelSystem(ls);
        });
    }
    */
    if (ui->editButton) ui->editButton->setVisible(false); // Hide the button

        // Connect the Designer-placed Flag button (if present) to the scan dialog -> Now Add to Queue
        if (ui->flagButton) {
            connect(ui->flagButton, &QPushButton::clicked, this, [this]() {
                if (!ls) return;
                QDialog dlg(this);
                dlg.setWindowTitle("Add to Print Queue (scan)");
                QVBoxLayout *layout = new QVBoxLayout(&dlg);
                QLabel *instr = new QLabel("Scan a barcode or type it and press Process.\nDialog remains open for repeated scans.", &dlg);
                layout->addWidget(instr);
                QLineEdit *input = new QLineEdit(&dlg);
                input->setPlaceholderText("Barcode");
                layout->addWidget(input);

                QSettings s;
                bool simpleQueue = s.value("simple_queue_mode", false).toBool();

                QLabel *qtyLabel2 = new QLabel("Quantity to add:", &dlg);
                layout->addWidget(qtyLabel2);
                QSpinBox *qtyBox2 = new QSpinBox(&dlg);
                qtyBox2->setRange(1, 999);
                qtyBox2->setValue(1);
                layout->addWidget(qtyBox2);
                
                if (simpleQueue) {
                    qtyLabel2->setVisible(false);
                    qtyBox2->setVisible(false);
                }

                QLabel *status = new QLabel("", &dlg);
                layout->addWidget(status);
                QDialogButtonBox *buttons2 = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
                QPushButton *processBtn = new QPushButton("Process", &dlg);
                buttons2->addButton(processBtn, QDialogButtonBox::ActionRole);
                layout->addWidget(buttons2);

                auto processBarcode = [this, input, qtyBox2, status, simpleQueue]() {
                    QString bc = input->text().trimmed();
                    if (bc.isEmpty()) {
                        status->setText("No barcode entered.");
                        return;
                    }
                    std::vector<std::string> barcodes;
                    barcodes.push_back(bc.toStdString());
                    
                    int qtyToAdd = simpleQueue ? 1 : qtyBox2->value();
                    int matched = ls ? ls->queueProducts(barcodes, qtyToAdd) : 0;

                    setLabelSystem(ls);
                    if (matched > 0) status->setText(QString("Added %1 product(s) to queue.").arg(matched));
                    else status->setText("No matching product found.");
                    input->clear();
                    input->setFocus();
                };

                connect(processBtn, &QPushButton::clicked, &dlg, processBarcode);
                connect(input, &QLineEdit::returnPressed, &dlg, processBarcode);
                connect(buttons2, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

                input->setFocus();
                dlg.exec();
            });
        }

    // Autoload product list from per-user autosave location
    autosavePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/autosave.csv";
    // Ensure the parent directory exists
    QFileInfo ai(autosavePath);
    QDir adir = ai.dir();
    if (!adir.exists()) adir.mkpath(".");

    QString configuredDb = settings.value("database_path").toString();
    int dbMode = settings.value("db_mode", 0).toInt();
    bool externalLoaded = false;
    
    if (dbMode == 1) {
        // Clover Mode
        QString mId = settings.value("clover_merchant_id").toString();
        QString token;
        QString tokenErr;
        bool isSandbox = settings.value("clover_sandbox", false).toBool();
        if (!mId.isEmpty() && ensureCloverTokenForUse(token, tokenErr, false)) {
            if (ls->dtb.loadFromClover(mId.toStdString(), token.toStdString(), isSandbox)) {
                 setLabelSystem(ls);
                 externalLoaded = true;
            }
        }
    } else {
        // Prioritize external DB if configured and available
        if (!configuredDb.isEmpty() && QFile::exists(configuredDb)) {
            if(ls->dtb.loadFromCSV(configuredDb.toStdString(), getAppCSVMapping())) {
                setLabelSystem(ls);
                externalLoaded = true;
            }
        }
    }

    if (!externalLoaded && QFile::exists(autosavePath)) {
        ls->dtb.loadFromCSV(autosavePath.toStdString());
        setLabelSystem(ls);
    }

    // Debounce timer for QSS reloads (avoid duplicate reloads from editors)
    qssReloadTimer = new QTimer(this);
    qssReloadTimer->setSingleShot(true);
    qssReloadTimer->setInterval(150);
    connect(qssReloadTimer, &QTimer::timeout, this, &MainWindow::reloadStylesheet);

    // Load persisted custom path if present
    currentCustomQssPath = settings.value("custom_qss", "").toString();
    updateThemeMenuChecks();

    // Ensure UI is populated with current in-memory data and active quick filters.
    setLabelSystem(ls);
    updateFocusDebugLogging();

    const bool autoCheckUpdates = settings.value("updater/autoCheck", true).toBool();
    settings.setValue("updater/manifestUrl", kUpdaterSourceFixed);
    const QString updaterSource = kUpdaterSourceFixed;
    if (appUpdater && autoCheckUpdates && !updaterSource.isEmpty()) {
        QTimer::singleShot(2200, this, [this, updaterSource]() {
            if (!appUpdater) return;
            QSettings updaterSettings;
            const QString installerArgs = updaterSettings.value("updater/installerArgs", "").toString().trimmed();
            const bool requireSignature = updaterSettings.value("updater/requireSignature", true).toBool();
            const QString expectedPublisher = updaterSettings.value("updater/expectedPublisher", "").toString().trimmed();
            appUpdater->setUpdateSource(updaterSource);
            appUpdater->setDefaultInstallerArguments(installerArgs.split(' ', Qt::SkipEmptyParts));
            appUpdater->setSignaturePolicy(requireSignature, expectedPublisher);
            appUpdater->checkForUpdates(false);
        });
    }
}

void MainWindow::setLabelSystem(labelSystem *system)
{
    ls = system;

    if (!ls) {
        qDebug() << "Label system is null.";
        return;
    }

    rebuildProductTable();
    updateBatchActionBar();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Persist autosave to per-user writable location
    ls->dtb.saveToCSV(autosavePath.toStdString());

    event->accept();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::toggleTheme()
{
    if (!isHighContrast) {
        // stronger buttons, lighter window background
        QString high = R"(
            QMainWindow { background: #f4f7fb; }
            QPushButton { background: qlineargradient(x1:0,y1:0,x2:0,y2:1, stop:0 #e8f0ff, stop:1 #d6e6ff); color: #111111; border: 1px solid rgba(0,0,0,0.12); }
            QPushButton:hover { background: #cfe3ff; }
        )";
        // Toggle saved theme to 'dark' and reload QSS
        QSettings settings;
        settings.setValue("theme", "dark");
        reloadStylesheet();
        isHighContrast = true;
    } else {
        // revert to softer default
        QString soft = R"(
            QMainWindow { background: #fbfcfd; }
            QTableWidget { background: #ffffff; border-radius: 6px; }
            QTableView { alternate-background-color: #eef4fb; }
            QHeaderView::section { background: #2f3b45; color: #ffffff; padding: 6px; border: none; }
            QPushButton { background: transparent; color: #222222; border: 1px solid rgba(0,0,0,0.04); border-radius: 6px; padding: 6px 10px; }
            QPushButton:hover { background: rgba(0,0,0,0.02); }
        )";
        // Toggle saved theme to 'light' and reload QSS
        QSettings settings;
        settings.setValue("theme", "light");
        reloadStylesheet();
        isHighContrast = false;
    }
}

void MainWindow::setTheme(const QString &theme)
{
    QSettings settings;
    settings.setValue("theme", theme);
    // reload the stylesheet to apply
    reloadStylesheet();
}

void MainWindow::updateThemeMenuChecks()
{
    QSettings settings;
    QString theme = settings.value("theme", "light").toString();
    themeLightAct->setChecked(theme == "light");
    themeDarkAct->setChecked(theme == "dark");
    if (themeHighContrastAct) themeHighContrastAct->setChecked(theme == "high_contrast");
}

void MainWindow::reloadStylesheet()
{
    QSettings settings;
    // Honor transient command-line override if present, otherwise QSettings
    QString theme;
    QVariant override = qApp->property("themeOverride");
    if (override.isValid()) theme = override.toString();
    else theme = settings.value("theme", "light").toString();
    QString chosenName;
    if (theme == "file") chosenName = "style.qss";
    else if (theme == "dark") chosenName = "style_dark.qss";
    else if (theme == "high_contrast") chosenName = "style_high_contrast.qss";
    else chosenName = "style_light.qss";

    QStringList candidates;
    candidates << (QStringLiteral("resources/") + chosenName);
    candidates << (QStringLiteral("../resources/") + chosenName);
    candidates << (QStringLiteral("../../resources/") + chosenName);
    // fallback to editable default if chosen not present
    candidates << QStringLiteral("resources/style.qss");

    QString qssPath;
    for (const QString &c : candidates) {
        if (QFile::exists(c)) { qssPath = c; break; }
    }

    if (!qssPath.isEmpty() && QFile::exists(qssPath)) {
        QFile qssFile(qssPath);
        if (qssFile.open(QFile::ReadOnly | QFile::Text)) {
            QString style = QString::fromUtf8(qssFile.readAll());
            qApp->setStyleSheet(style);
            // Also set a matching QPalette so widgets and style-drawn glyphs
            // (headers, checkboxes, arrows) use readable foreground colors
            QPalette pal = qApp->palette();
            if (theme == "dark") {
                pal.setColor(QPalette::Window, QColor("#0b0f12"));
                pal.setColor(QPalette::WindowText, QColor("#e6eef6"));
                pal.setColor(QPalette::Button, QColor("#0f1417"));
                pal.setColor(QPalette::ButtonText, QColor("#e6eef6"));
                pal.setColor(QPalette::Base, QColor("#0f1417"));
                pal.setColor(QPalette::Text, QColor("#e6eef6"));
                pal.setColor(QPalette::Highlight, QColor("#4f7be6"));
                pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
            } else if (theme == "high_contrast") {
                pal.setColor(QPalette::Window, QColor("#000000"));
                pal.setColor(QPalette::WindowText, QColor("#ffffff"));
                pal.setColor(QPalette::Button, QColor("#000000"));
                pal.setColor(QPalette::ButtonText, QColor("#ffffff"));
                pal.setColor(QPalette::Base, QColor("#000000"));
                pal.setColor(QPalette::Text, QColor("#ffffff"));
                pal.setColor(QPalette::Highlight, QColor("#ffff00"));
                pal.setColor(QPalette::HighlightedText, QColor("#000000"));
            } else {
                // Light theme defaults
                pal.setColor(QPalette::Window, QColor("#f7f9fb"));
                pal.setColor(QPalette::WindowText, QColor("#1f2933"));
                pal.setColor(QPalette::Button, QColor("#ffffff"));
                pal.setColor(QPalette::ButtonText, QColor("#0f1720"));
                pal.setColor(QPalette::Base, QColor("#ffffff"));
                pal.setColor(QPalette::Text, QColor("#111827"));
                pal.setColor(QPalette::Highlight, QColor("#4f7be6"));
                pal.setColor(QPalette::HighlightedText, QColor("#ffffff"));
            }
            qApp->setPalette(pal);
            qssFile.close();
            qDebug() << "Reloaded stylesheet:" << qssPath << "(theme=" << theme << ")";
            if (statusBar()) statusBar()->showMessage(QString("Stylesheet reloaded: %1").arg(qssPath), 2000);
            // Ensure the watcher is watching the active file and its directory (help editors that replace files)
            if (qssWatcher) {
                if (!qssWatcher->files().contains(qssPath) && QFile::exists(qssPath)) qssWatcher->addPath(qssPath);
                QString dir = QFileInfo(qssPath).absolutePath();
                if (!qssWatcher->directories().contains(dir)) qssWatcher->addPath(dir);
            }
        }
    } else {
        qDebug() << "No stylesheet found to reload for theme" << theme;
    }
}

void MainWindow::onDatabaseFileChanged(const QString &path)
{
    // Auto-load changes from the synced file
    // Debounce or just reload? fileChanged might trigger multiple times during write.
    // Ideally we debounce. For now, direct reload.
    // Note: If we are editing in the app, saving might trigger this. 
    // We should distinguish between external and internal updates, but file watcher is blind.
    // For now, let's assume external updates are primary for this feature.
    
    // Safety check: is file readable?
    QFileInfo check(path);
    if(check.exists() && check.size() > 0) {
        if(ls->dtb.loadFromCSV(path.toStdString(), getAppCSVMapping())) {
            setLabelSystem(ls);
            if(statusBar()) statusBar()->showMessage("Database reloaded from external change.", 3000);
        }
    }
}

void MainWindow::onQssFileChanged(const QString &path)
{
    Q_UNUSED(path);
    if (qssReloadTimer) {
        // restart the timer so multiple events coalesce into one reload
        qssReloadTimer->start();
    } else {
        reloadStylesheet();
    }
}

bool MainWindow::passesQuickFilter(const product &pd) const
{
    if (currentQuickFilter == "queued") {
        return pd.getLabelQuantity() > 0;
    }
    if (currentQuickFilter == "price_changed") {
        return std::abs(pd.getPrice() - pd.getOriginalPrice()) > 0.0001f;
    }
    return true;
}

void MainWindow::updateResultsSummary(int shown, int total)
{
    if (!resultsSummaryLabel) return;
    resultsSummaryLabel->setText(QString("%1 shown of %2").arg(shown).arg(total));
}

void MainWindow::updateBatchActionBar()
{
    if (!batchActionBar || !ui || !ui->productTable) return;
    const bool hasSelection = !ui->productTable->selectedItems().isEmpty();
    batchActionBar->setVisible(hasSelection);
}

void MainWindow::rebuildProductTable()
{
    if (!ui || !ui->productTable || !ls) return;

    const auto &products = ls->dtb.listProduct();

    QSettings settings;
    const bool simpleQueue = settings.value("simple_queue_mode", false).toBool();
    QTableWidgetItem *headerItem = ui->productTable->horizontalHeaderItem(4);
    if (!headerItem) {
        headerItem = new QTableWidgetItem();
        ui->productTable->setHorizontalHeaderItem(4, headerItem);
    }
    headerItem->setText(simpleQueue ? "Print" : "Print Qty");

    const QString query = currentSearchQuery.trimmed();

    const bool wasBlocked = ui->productTable->blockSignals(true);
    ui->productTable->setRowCount(0);

    int shownCount = 0;
    for (const product &pd : products) {
        const QString name = QString::fromStdString(pd.getName());
        const QString barcode = QString::fromStdString(pd.getBarcode());

        const bool matchesSearch = query.isEmpty()
            || name.contains(query, Qt::CaseInsensitive)
            || barcode.contains(query, Qt::CaseInsensitive);
        if (!matchesSearch || !passesQuickFilter(pd)) {
            continue;
        }

        const int row = ui->productTable->rowCount();
        ui->productTable->insertRow(row);

        QTableWidgetItem *it0 = new QTableWidgetItem(name);
        QTableWidgetItem *it1 = new QTableWidgetItem(barcode);
        QTableWidgetItem *it2 = new QTableWidgetItem(QString::number(pd.getPrice(), 'f', 2));
        QTableWidgetItem *it3 = new QTableWidgetItem(QString::number(pd.getOriginalPrice(), 'f', 2));
        QTableWidgetItem *it4 = nullptr;

        if (simpleQueue) {
            it4 = new QTableWidgetItem();
            it4->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            it4->setCheckState(pd.getLabelQuantity() > 0 ? Qt::Checked : Qt::Unchecked);
            it4->setText("");
        } else {
            it4 = new QTableWidgetItem(QString::number(pd.getLabelQuantity()));
        }

        it0->setData(Qt::UserRole, barcode);
        it1->setData(Qt::UserRole, barcode);
        it2->setData(Qt::UserRole, barcode);
        it3->setData(Qt::UserRole, barcode);
        it4->setData(Qt::UserRole, barcode);

        ui->productTable->setItem(row, 0, it0);
        ui->productTable->setItem(row, 1, it1);
        ui->productTable->setItem(row, 2, it2);
        ui->productTable->setItem(row, 3, it3);
        ui->productTable->setItem(row, 4, it4);
        ++shownCount;
    }

    ui->productTable->blockSignals(wasBlocked);

    updateResultsSummary(shownCount, static_cast<int>(products.size()));
    updateBatchActionBar();
}

bool MainWindow::validateAndApplyCellEdit(QTableWidgetItem *item)
{
    if (!item || !ls) return false;

    const QString originalBarcode = item->data(Qt::UserRole).toString();
    if (originalBarcode.isEmpty()) return false;

    try {
        product &p = ls->dtb.searchByBarcode(originalBarcode.toStdString());
        const int col = item->column();
        const QString text = item->text().trimmed();

        bool valid = true;
        QString errorText;

        if (col == 0) {
            if (text.isEmpty()) {
                valid = false;
                errorText = "Name cannot be empty.";
            }
        } else if (col == 1) {
            if (text.isEmpty()) {
                valid = false;
                errorText = "Barcode cannot be empty.";
            } else {
                for (const product &other : ls->dtb.listProduct()) {
                    if (other.getBarcode() == text.toStdString() && other.getBarcode() != originalBarcode.toStdString()) {
                        valid = false;
                        errorText = "Barcode must be unique.";
                        break;
                    }
                }
            }
        } else if (col == 2 || col == 3) {
            bool ok = false;
            const float num = text.toFloat(&ok);
            if (!ok || num < 0.0f) {
                valid = false;
                errorText = "Price values must be numeric and >= 0.";
            }
        } else if (col == 4) {
            QSettings s;
            if (!s.value("simple_queue_mode", false).toBool()) {
                bool ok = false;
                const int qty = text.toInt(&ok);
                if (!ok || qty < 0) {
                    valid = false;
                    errorText = "Print quantity must be an integer >= 0.";
                }
            }
        }

        if (!valid) {
            item->setBackground(QColor("#ffe8e8"));
            item->setToolTip(errorText);
            statusBar()->showMessage(errorText, 3000);
            return false;
        }

        item->setBackground(QBrush());
        item->setToolTip(QString());

        if (col == 0) {
            p.setDescription(text.toStdString());
        } else if (col == 1) {
            const std::string newBarcode = text.toStdString();
            p.setBarcode(newBarcode);

            const int row = item->row();
            const bool wasBlocked = ui->productTable->blockSignals(true);
            for (int c = 0; c < ui->productTable->columnCount(); ++c) {
                QTableWidgetItem *cell = ui->productTable->item(row, c);
                if (cell) cell->setData(Qt::UserRole, QString::fromStdString(newBarcode));
            }
            ui->productTable->blockSignals(wasBlocked);
        } else if (col == 2) {
            p.setPrice(text.toFloat());
        } else if (col == 3) {
            p.setOriginalPrice(text.toFloat());
        } else if (col == 4) {
            QSettings s;
            if (s.value("simple_queue_mode", false).toBool()) {
                p.setLabelQuantity(item->checkState() == Qt::Checked ? 1 : 0);
            } else {
                p.setLabelQuantity(text.toInt());
            }
        }

        statusBar()->showMessage("Changes saved.", 1200);
        return true;
    } catch (const std::exception &e) {
        qDebug() << "Edit failed (product lookup error):" << e.what();
        statusBar()->showMessage("Unable to apply edit.", 2500);
        return false;
    }
}

void MainWindow::updateAddButtonState() {
    if (!ui->addButton) return;
    
    QSettings settings;
    int modeInt = settings.value("db_mode", 0).toInt();
    bool isClover = (modeInt == 2);
    
    // Disconnect all previous connections
    ui->addButton->disconnect();
    
    if (isClover) {
        ui->addButton->setText("Sync with Clover");
        // Try to pick a better icon if available, else standard fallback
        QIcon icon = QIcon::fromTheme("view-refresh");
        if (icon.isNull()) icon = QIcon::fromTheme("system-software-update"); 
        ui->addButton->setIcon(icon);
        
        connect(ui->addButton, &QPushButton::clicked, this, [this]() {
             QSettings s;
             QString mId = s.value("clover_merchant_id").toString();
             QString token;
             QString tokenErr;
             bool isSandbox = s.value("clover_sandbox", false).toBool();
             
             if (mId.isEmpty()) {
                 QMessageBox::warning(this, "Configuration Error", "Clover Merchant ID or Token is missing.\nPlease go to Options -> Settings Workspace -> Integrations to configure integration.");
                 return;
             }

             if (!ensureCloverTokenForUse(token, tokenErr, false)) {
                 QMessageBox::warning(this, "Clover Auth", tokenErr);
                 return;
             }
             
             QApplication::setOverrideCursor(Qt::WaitCursor);
             bool success = ls->dtb.loadFromClover(mId.toStdString(), token.toStdString(), isSandbox);

             // If sync failed with a stale token, try one forced refresh and retry once.
             if (!success) {
                 QString refreshErr;
                 if (ensureCloverTokenForUse(token, refreshErr, true)) {
                     success = ls->dtb.loadFromClover(mId.toStdString(), token.toStdString(), isSandbox);
                 }
             }

             QApplication::restoreOverrideCursor();
             
             if (success) {
                 setLabelSystem(ls);
                 QMessageBox::information(this, "Success", "Inventory synced with Clover.");
             } else {
                 QMessageBox::warning(this, "Sync Failed", "Could not sync with Clover.\nCheck internet connection or credentials.");
             }
        });
        
    } else {
        ui->addButton->setText("Add Product");
        ui->addButton->setIcon(QIcon::fromTheme("list-add"));
        
        connect(ui->addButton, &QPushButton::clicked, this, [this]() {
            bool ok;
            QString description = QInputDialog::getText(this, "Add Product", "Description:", QLineEdit::Normal, "", &ok);
            if (!ok || description.isEmpty()) return;

            float price = QInputDialog::getDouble(this, "Add Product", "Price:", 0, 0, 10000, 2, &ok);
            if (!ok) return;

            QString barcode = QInputDialog::getText(this, "Add Product", "Barcode:", QLineEdit::Normal, "", &ok);
            if (!ok || barcode.isEmpty()) return;

            int qty = 1; // Default to flagged
            QSettings s;
            if (!s.value("simple_queue_mode", false).toBool()) {
                qty = QInputDialog::getInt(this, "Add Product", "Print Queue Quantity:", 1, 0, 999, 1, &ok);
                if (!ok) return;
            }

            product newProduct(description.toStdString(), price, barcode.toStdString(), qty);
            ls->dtb.add(newProduct);
            setLabelSystem(ls);
        });
    }
}

void MainWindow::onQssDirChanged(const QString &path)
{
    // Directory change could indicate an editor replaced the file. Re-scan watched candidate files
    Q_UNUSED(path);
    // Re-add known theme files if they now exist
    QStringList themeFiles = { "style_light.qss", "style_dark.qss", "style_high_contrast.qss", "style.qss" };
    QStringList candidatesPrefix = { "resources/", "../resources/", "../../resources/" };
    for (const QString &name : themeFiles) {
        for (const QString &pref : candidatesPrefix) {
            QString p = pref + name;
            if (QFile::exists(p) && !qssWatcher->files().contains(p)) {
                qDebug() << "Directory change: adding watch for" << p;
                qssWatcher->addPath(p);
            }
        }
    }
    // Trigger a reload (debounced)
    if (qssReloadTimer) qssReloadTimer->start();
}

void MainWindow::updateFocusDebugLogging()
{
    QSettings settings;
    const bool shouldEnable = settings.value("accessibility/focusDebug", false).toBool();

    if (focusDebugConnection) {
        QObject::disconnect(focusDebugConnection);
        focusDebugConnection = QMetaObject::Connection();
    }

    focusDebugEnabled = shouldEnable;
    if (!focusDebugEnabled) {
        if (statusBar()) statusBar()->showMessage("Focus debug logging disabled.", 1400);
        return;
    }

    focusDebugConnection = QObject::connect(qApp, &QApplication::focusChanged, this,
        [this](QWidget *oldWidget, QWidget *newWidget) {
            if (!focusDebugEnabled) return;

            auto describe = [](QWidget *w) {
                if (!w) return QString("<none>");
                const QString name = w->accessibleName().isEmpty() ? w->objectName() : w->accessibleName();
                return QString("%1 (%2)")
                    .arg(name.isEmpty() ? QString("unnamed") : name)
                    .arg(w->metaObject()->className());
            };

            const QString msg = QString("Focus: %1 -> %2")
                .arg(describe(oldWidget))
                .arg(describe(newWidget));
            qDebug() << msg;
            if (statusBar()) statusBar()->showMessage(msg, 1500);
        }
    );

    if (statusBar()) statusBar()->showMessage("Focus debug logging enabled.", 1400);
}

void MainWindow::queueUndoAction(const QString &message, std::function<void()> undoAction)
{
    pendingUndoAction = std::move(undoAction);
    undoActionMessage = message;
    undoActionSecondsRemaining = 10;
    if (undoActionLabel) {
        undoActionLabel->setText(QString("%1 Undo available for %2s.").arg(undoActionMessage).arg(undoActionSecondsRemaining));
    }
    if (undoActionBar) {
        undoActionBar->show();
    }
    if (undoActionCountdownTimer) {
        undoActionCountdownTimer->start();
    }
    if (undoActionExpiryTimer) {
        undoActionExpiryTimer->start(10000);
    }
}

void MainWindow::clearUndoAction()
{
    pendingUndoAction = nullptr;
    undoActionMessage.clear();
    undoActionSecondsRemaining = 0;
    if (undoActionExpiryTimer && undoActionExpiryTimer->isActive()) {
        undoActionExpiryTimer->stop();
    }
    if (undoActionCountdownTimer && undoActionCountdownTimer->isActive()) {
        undoActionCountdownTimer->stop();
    }
    if (undoActionLabel) {
        undoActionLabel->clear();
    }
    if (undoActionBar) {
        undoActionBar->hide();
    }
}

static QString generatePkceCodeVerifier() {
    QByteArray randomBytes(64, '\0');
    QRandomGenerator *rng = QRandomGenerator::global();
    for (int i = 0; i < randomBytes.size(); ++i) {
        randomBytes[i] = static_cast<char>(rng->generate() & 0xFF);
    }
    return QString::fromLatin1(randomBytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

static QString generatePkceCodeChallenge(const QString &verifier) {
    const QByteArray digest = QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
