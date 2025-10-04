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
#include <QDialogButtonBox>
#include <QLabel>
#include "../include/ConfigEditorDialog.h"
#include "../forms/ui_MainWindow.h"

// Icons are embedded in resources/icons.qrc and accessed as :/icons/<name>.svg

MainWindow::MainWindow(labelSystem *labelSys, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow()), ls(labelSys)
{
    ui->setupUi(this);

    // Apply a modern font and basic stylesheet for a cleaner look
    QFont baseFont("Segoe UI", 10);
    QApplication::setFont(baseFont);
    // Do not set a default stylesheet here; run.cpp loads the initial QSS so MainWindow
    // won't duplicate or override that logic. We still set fonts/palettes programmatically.

    // Configure menus
    QMenuBar *menuBar = this->menuBar();
    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *printAction = fileMenu->addAction(QIcon::fromTheme("document-print"), "Print Labels (Preview)", [this]() {
        if (!ls) return;
        ls->viewLabels();

        QMessageBox::StandardButton clearBtn = QMessageBox::question(this,
            "Remove Label Flags",
            "Remove label flags from products?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);

        if (clearBtn == QMessageBox::Yes) {
            ls->clearLabelFlags();
            setLabelSystem(ls);
        }
    });
    fileMenu->addAction(QIcon::fromTheme("application-exit"), "Exit", this, &QWidget::close);

    QAction *saveAction = fileMenu->addAction(QIcon::fromTheme("document-save"), "Save", [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "Save Product List", "", "Text Files (*.txt)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Save Error", "Unable to save file.");
            return;
        }

        QTextStream out(&file);
        const auto &products = ls->dtb.listProduct();
        for (const auto &product : products) {
            out << QString::fromStdString(product.getName()) << ","
                << QString::fromStdString(product.getBarcode()) << ","
                << product.getPrice() << ","
                << QString::fromStdString(product.getSize()) << ","
                << product.getLabelFlag() << "\n";
        }

        file.close();
    });

    QAction *loadAction = fileMenu->addAction(QIcon::fromTheme("document-open"), "Load", [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "Load Product List", "", "Text Files (*.txt)");
        if (fileName.isEmpty()) return;

        QFile file(fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, "Load Error", "Unable to open file.");
            return;
        }

        QTextStream in(&file);
        ls->dtb.clear();
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split(",");
            if (parts.size() != 3) continue;

            product newProduct(parts[0].toStdString(), parts[2].toFloat(), "", parts[1].toStdString(), false);
            ls->dtb.add(newProduct);
        }

        file.close();
        setLabelSystem(ls);
    });

    // Scan barcodes -> allow pasting/scanning multiple barcodes (one per line) and set label flags
    QAction *scanAction = fileMenu->addAction(QIcon::fromTheme("system-search"), "Scan Barcodes...", [this]() {
        QDialog dlg(this);
        dlg.setWindowTitle("Scan Barcodes");
        QVBoxLayout *layout = new QVBoxLayout(&dlg);
        QLabel *lbl = new QLabel("Scan or paste barcodes (one per line).\nEach barcode found will toggle its Label Flag (check to set, uncheck to clear).", &dlg);
        layout->addWidget(lbl);
        QTextEdit *edit = new QTextEdit(&dlg);
        edit->setPlaceholderText("e.g.\n012345678905\n012345678906\n...");
        layout->addWidget(edit);
        QCheckBox *setFlagBox = new QCheckBox("Set label flag for matched products (unchecked = clear)", &dlg);
        setFlagBox->setChecked(true);
        layout->addWidget(setFlagBox);
        QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

        if (dlg.exec() == QDialog::Accepted) {
            QString text = edit->toPlainText();
            QStringList lines = text.split('\n', Qt::SkipEmptyParts);
            std::vector<std::string> barcodes;
            barcodes.reserve(lines.size());
            for (const QString &l : lines) {
                QString s = l.trimmed();
                if (s.isEmpty()) continue;
                barcodes.push_back(s.toStdString());
            }
            int matched = 0;
            if (ls) matched = ls->flagProducts(barcodes, setFlagBox->isChecked());
            setLabelSystem(ls);
            QMessageBox::information(this, "Scan Complete", QString("Processed %1 barcodes. %2 matched products updated.").arg(lines.size()).arg(matched));
        }
    });

    QMenu *helpMenu = menuBar->addMenu("Help");
    helpMenu->addAction(QIcon::fromTheme("help-about"), "About", []() {
        QMessageBox::about(nullptr, "About", "Retail Label System v1.0\nStyled Edition");
    });

    QMenu *optionsMenu = menuBar->addMenu("Options");
    optionsMenu->addAction("Edit Config", [this]() {
        if (!ls) return;
        labelConfig cfg = ls->getLabelConfig();
        ConfigEditorDialog dlg(cfg, this);
        if (dlg.exec() == QDialog::Accepted) {
            labelConfig newCfg = dlg.getConfig();
            ls->setLabelConfig(newCfg);
            if (!ls->saveConfig("resources/Config.txt")) {
                QMessageBox::warning(this, "Save Error", "Failed to save config to resources/Config.txt");
            }
        }
    });

    // Toolbar
    QToolBar *toolbar = addToolBar("Main");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(20,20));
    toolbar->addAction(printAction);
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
    themeReloadAct = themeMenu->addAction("Reload Stylesheet");
    themeChooseCustomAct = themeMenu->addAction("Choose Custom Stylesheet...");
    connect(themeLightAct, &QAction::triggered, this, [this]() { setTheme("light"); updateThemeMenuChecks(); });
    connect(themeDarkAct, &QAction::triggered, this, [this]() { setTheme("dark"); updateThemeMenuChecks(); });
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
    QStringList themeFiles = { "style_light.qss", "style_dark.qss", "style.qss" };
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

    // Configure product table (widget created by Designer)
    ui->productTable->setColumnCount(5);
    ui->productTable->setHorizontalHeaderLabels({"Product Name", "Barcode", "Price", "Size", "Label Flag"});
    ui->productTable->horizontalHeader()->setStretchLastSection(true);
    ui->productTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->productTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->productTable->setAlternatingRowColors(true);
    ui->productTable->setShowGrid(true);
    ui->productTable->verticalHeader()->setVisible(false);
    // productTable appearance is controlled by global QSS; avoid per-widget stylesheet here
    // Let QSS control palette and font for the product table; remove programmatic overrides
    ui->productTable->setGridStyle(Qt::SolidLine);
    ui->productTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->productTable->horizontalHeader()->setFixedHeight(34);

    // Use Designer buttons (set icons for consistency if present)
    if (ui->addButton) ui->addButton->setIcon(QIcon::fromTheme("list-add"));
    if (ui->removeButton) ui->removeButton->setIcon(QIcon::fromTheme("list-remove"));
    if (ui->editButton) ui->editButton->setIcon(QIcon::fromTheme("document-edit"));

    // runtime-created flag button removed; Designer `ui->flagButton` is used instead

    // Add search bar and button programmatically and place into topRow
    QLineEdit *searchBar = new QLineEdit(this);
    searchBar->setObjectName("searchBar");
    searchBar->setPlaceholderText("Search by name or barcode");
    QPushButton *searchButton = new QPushButton(QIcon::fromTheme("edit-find"), "Search", this);
    searchButton->setObjectName("searchButton");

    QHBoxLayout *searchLayout = new QHBoxLayout;
    searchLayout->addWidget(searchBar);
    searchLayout->addWidget(searchButton);

    if (ui->topRow) {
        ui->topRow->setLayout(searchLayout);
    }

    // Status bar
    QStatusBar *statusBar = this->statusBar();
    statusBar->showMessage("Ready");

    // Connect add/remove/edit buttons
    if (ui->addButton) {
        connect(ui->addButton, &QPushButton::clicked, this, [this]() {
            bool ok;
            QString description = QInputDialog::getText(this, "Add Product", "Description:", QLineEdit::Normal, "", &ok);
            if (!ok || description.isEmpty()) return;

            float price = QInputDialog::getDouble(this, "Add Product", "Price:", 0, 0, 10000, 2, &ok);
            if (!ok) return;

            QString size = QInputDialog::getText(this, "Add Product", "Size:", QLineEdit::Normal, "", &ok);
            if (!ok || size.isEmpty()) return;

            QString barcode = QInputDialog::getText(this, "Add Product", "Barcode:", QLineEdit::Normal, "", &ok);
            if (!ok || barcode.isEmpty()) return;

            QCheckBox *labelFlagCheckBox = new QCheckBox("Label Flag", this);
            QMessageBox msgBox;
            msgBox.setWindowTitle("Add Product");
            msgBox.setText("Set Label Flag:");
            msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.layout()->addWidget(labelFlagCheckBox);
            int result = msgBox.exec();
            if (result != QMessageBox::Ok) return;

            bool labelFlag = labelFlagCheckBox->isChecked();

            product newProduct(description.toStdString(), price, size.toStdString(), barcode.toStdString(), labelFlag);
            ls->dtb.add(newProduct);
            setLabelSystem(ls);
        });
    }

    if (ui->removeButton) {
        connect(ui->removeButton, &QPushButton::clicked, this, [this]() {
            auto selectedItems = ui->productTable->selectedItems();
            if (selectedItems.isEmpty()) {
                QMessageBox::warning(this, "Remove Product", "Please select a product to remove.");
                return;
            }

            int row = ui->productTable->row(selectedItems.first());
            QString barcode = ui->productTable->item(row, 1)->text();

            ls->dtb.removeByBarcode(barcode.toStdString());
            setLabelSystem(ls);
        });
    }

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

        // Connect the Designer-placed Flag button (if present) to the scan dialog
        if (ui->flagButton) {
            connect(ui->flagButton, &QPushButton::clicked, this, [this]() {
                if (!ls) return;
                QDialog dlg(this);
                dlg.setWindowTitle("Flag Product (scan)");
                QVBoxLayout *layout = new QVBoxLayout(&dlg);
                QLabel *instr = new QLabel("Scan a barcode or type it and press Process.\nDialog remains open for repeated scans.", &dlg);
                layout->addWidget(instr);
                QLineEdit *input = new QLineEdit(&dlg);
                input->setPlaceholderText("Barcode");
                layout->addWidget(input);
                QCheckBox *setFlagBox2 = new QCheckBox("Set label flag (unchecked = clear)", &dlg);
                setFlagBox2->setChecked(true);
                layout->addWidget(setFlagBox2);
                QLabel *status = new QLabel("", &dlg);
                layout->addWidget(status);
                QDialogButtonBox *buttons2 = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
                QPushButton *processBtn = new QPushButton("Process", &dlg);
                buttons2->addButton(processBtn, QDialogButtonBox::ActionRole);
                layout->addWidget(buttons2);

                auto processBarcode = [this, input, setFlagBox2, status]() {
                    QString bc = input->text().trimmed();
                    if (bc.isEmpty()) {
                        status->setText("No barcode entered.");
                        return;
                    }
                    std::vector<std::string> barcodes;
                    barcodes.push_back(bc.toStdString());
                    int matched = ls ? ls->flagProducts(barcodes, setFlagBox2->isChecked()) : 0;
                    setLabelSystem(ls);
                    if (matched > 0) status->setText(QString("Updated %1 product(s).").arg(matched));
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

    // connect search
    connect(searchButton, &QPushButton::clicked, this, [this, searchBar]() {
        QString query = searchBar->text();
        if (query.isEmpty()) {
            setLabelSystem(ls);
            return;
        }

        const auto &products = ls->dtb.listProduct();
        ui->productTable->setRowCount(0);

        for (const auto &product : products) {
            if (QString::fromStdString(product.getName()).contains(query, Qt::CaseInsensitive) ||
                QString::fromStdString(product.getBarcode()).contains(query, Qt::CaseInsensitive)) {

                int row = ui->productTable->rowCount();
                ui->productTable->insertRow(row);
                ui->productTable->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(product.getName())));
                ui->productTable->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(product.getBarcode())));
                ui->productTable->setItem(row, 2, new QTableWidgetItem(QString::number(product.getPrice())));
            }
        }
    });

    // Autoload product list
    QFile file("autosave.txt");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        ls->dtb.clear();
        while (!in.atEnd()) {
            QString line = in.readLine();
            QStringList parts = line.split(",");
            if (parts.size() != 5) continue;

            bool ok;
            float price = parts[2].toFloat(&ok);
            if (!ok) continue;

            QString size = parts[3];
            bool labelFlag = parts[4].toInt(&ok);
            if (!ok) continue;

            product newProduct(parts[0].toStdString(), price, size.toStdString(), parts[1].toStdString(), labelFlag);
            ls->dtb.add(newProduct);
        }
        file.close();
        setLabelSystem(ls);
    }

    // Debounce timer for QSS reloads (avoid duplicate reloads from editors)
    qssReloadTimer = new QTimer(this);
    qssReloadTimer->setSingleShot(true);
    qssReloadTimer->setInterval(150);
    connect(qssReloadTimer, &QTimer::timeout, this, &MainWindow::reloadStylesheet);

    // Load persisted custom path if present
    QSettings settings;
    currentCustomQssPath = settings.value("custom_qss", "").toString();
    updateThemeMenuChecks();
}

void MainWindow::setLabelSystem(labelSystem *system)
{
    ls = system;

    if (!ls) {
        qDebug() << "Label system is null.";
        return;
    }

    const auto &products = ls->dtb.listProduct();
    ui->productTable->setRowCount(products.size());

    for (size_t i = 0; i < products.size(); ++i) {
        const auto &product = products[i];
        QTableWidgetItem *it0 = new QTableWidgetItem(QString::fromStdString(product.getName()));
        QTableWidgetItem *it1 = new QTableWidgetItem(QString::fromStdString(product.getBarcode()));
        QTableWidgetItem *it2 = new QTableWidgetItem(QString::number(product.getPrice()));
        QTableWidgetItem *it3 = new QTableWidgetItem(QString::fromStdString(product.getSize()));
        QTableWidgetItem *it4 = new QTableWidgetItem(product.getLabelFlag() ? "Yes" : "No");

    // Let the global QSS / application palette control text color and fonts
    // Avoid forcing foreground colors or fonts here so themes (dark/light) apply correctly.

        ui->productTable->setItem((int)i, 0, it0);
        ui->productTable->setItem((int)i, 1, it1);
        ui->productTable->setItem((int)i, 2, it2);
        ui->productTable->setItem((int)i, 3, it3);
        ui->productTable->setItem((int)i, 4, it4);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QFile file("autosave.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        const auto &products = ls->dtb.listProduct();
        for (const auto &product : products) {
            out << QString::fromStdString(product.getName()) << ","
                << QString::fromStdString(product.getBarcode()) << ","
                << product.getPrice() << ","
                << QString::fromStdString(product.getSize()) << ","
                << product.getLabelFlag() << "\n";
        }
        file.close();
    }

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

void MainWindow::onQssDirChanged(const QString &path)
{
    // Directory change could indicate an editor replaced the file. Re-scan watched candidate files
    Q_UNUSED(path);
    // Re-add known theme files if they now exist
    QStringList themeFiles = { "style_light.qss", "style_dark.qss", "style.qss" };
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