#include <QApplication>
#include <QDialog>
#include <QTableWidget>
#include <QTimer>
#include <QTest>

#include "MainWindow.h"
#include "labelSystem.h"
#include "test_mainwindow_gui_utils.h"

class MainWindowGuiActionsTests : public QObject {
    Q_OBJECT

private slots:
    void clear_queue_action_resets_quantities();
    void queue_workspace_action_opens_dialog();
    void scan_barcodes_action_opens_dialog();
};

void MainWindowGuiActionsTests::clear_queue_action_resets_quantities() {
    labelSystem ls;
    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 3));
    ls.dtb.add(product("B", 2.00f, "b", 2));
    window.setLabelSystem(&ls);

    QAction *clearQueue = findActionByText(&window, "Clear Queue");
    QVERIFY(clearQueue != nullptr);

    clearQueue->trigger();
    QCoreApplication::processEvents();

    QCOMPARE(ls.dtb.searchByBarcode("a").getLabelQuantity(), 0);
    QCOMPARE(ls.dtb.searchByBarcode("b").getLabelQuantity(), 0);

    auto *undoBar = window.findChild<QWidget *>("undoActionBar");
    QVERIFY(undoBar != nullptr);
    QVERIFY(!undoBar->isHidden());
}

void MainWindowGuiActionsTests::queue_workspace_action_opens_dialog() {
    labelSystem ls;
    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    QAction *openQueue = findActionByText(&window, "Open Print Queue Workspace...");
    QVERIFY(openQueue != nullptr);

    bool opened = false;
    QTimer::singleShot(40, [&opened]() {
        const auto top = QApplication::topLevelWidgets();
        for (QWidget *w : top) {
            QDialog *dlg = qobject_cast<QDialog *>(w);
            if (dlg && dlg->windowTitle() == "Print Queue Workspace") {
                opened = true;
                dlg->reject();
            }
        }
    });

    openQueue->trigger();
    QTest::qWait(100);
    QVERIFY(opened);
}

void MainWindowGuiActionsTests::scan_barcodes_action_opens_dialog() {
    labelSystem ls;
    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    QAction *scan = findActionByText(&window, "Scan Barcodes...");
    QVERIFY(scan != nullptr);

    bool opened = false;
    QTimer::singleShot(40, [&opened]() {
        const auto top = QApplication::topLevelWidgets();
        for (QWidget *w : top) {
            QDialog *dlg = qobject_cast<QDialog *>(w);
            if (dlg && dlg->windowTitle() == "Scan Barcodes") {
                opened = true;
                dlg->reject();
            }
        }
    });

    scan->trigger();
    QTest::qWait(100);
    QVERIFY(opened);
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    MainWindowGuiActionsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_mainwindow_gui_actions.moc"
