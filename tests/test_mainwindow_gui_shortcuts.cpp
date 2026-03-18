#include <QApplication>
#include <QLineEdit>
#include <QTableWidget>
#include <QTest>

#include "MainWindow.h"
#include "labelSystem.h"
#include "test_mainwindow_gui_utils.h"

class MainWindowGuiShortcutsTests : public QObject {
    Q_OBJECT

private slots:
    void shortcuts_registered_for_primary_commands();
    void ctrl_f_focuses_search();
    void delete_shortcut_and_undo_restore_rows();
    void escape_clears_table_selection();
};

void MainWindowGuiShortcutsTests::shortcuts_registered_for_primary_commands() {
    labelSystem ls;
    MainWindow window(&ls);

    QVERIFY(hasShortcut(&window, QKeySequence(Qt::CTRL | Qt::Key_F)));
    QVERIFY(hasShortcut(&window, QKeySequence(Qt::CTRL | Qt::Key_N)));
    QVERIFY(hasShortcut(&window, QKeySequence(Qt::Key_Delete)));
    QVERIFY(hasShortcut(&window, QKeySequence(Qt::Key_Escape)));
    QVERIFY(hasShortcut(&window, QKeySequence::Undo));
    QVERIFY(hasShortcut(&window, QKeySequence(Qt::Key_F6)));
}

void MainWindowGuiShortcutsTests::ctrl_f_focuses_search() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    auto *search = window.findChild<QLineEdit *>("searchBar");
    auto *table = window.findChild<QTableWidget *>("productTable");
    QVERIFY(search != nullptr);
    QVERIFY(table != nullptr);

    table->setFocus();
    QCoreApplication::processEvents();
    QVERIFY(table->hasFocus());

    QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QVERIFY(search->hasFocus());
}

void MainWindowGuiShortcutsTests::delete_shortcut_and_undo_restore_rows() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 0));
    ls.dtb.add(product("B", 2.00f, "b", 0));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    auto *undoBar = window.findChild<QWidget *>("undoActionBar");
    QVERIFY(table != nullptr);
    QVERIFY(undoBar != nullptr);

    QCOMPARE(table->rowCount(), 2);
    table->selectRow(0);
    QCoreApplication::processEvents();

    QTest::keyClick(&window, Qt::Key_Delete);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 1);
    QVERIFY(!undoBar->isHidden());

    QTest::keyClick(&window, Qt::Key_Z, Qt::ControlModifier);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 2);
}

void MainWindowGuiShortcutsTests::escape_clears_table_selection() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 0));
    ls.dtb.add(product("B", 2.00f, "b", 0));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    QVERIFY(table != nullptr);

    table->selectRow(0);
    QCoreApplication::processEvents();
    QVERIFY(!table->selectedItems().isEmpty());

    QTest::keyClick(&window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY(table->selectedItems().isEmpty());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    MainWindowGuiShortcutsTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_mainwindow_gui_shortcuts.moc"
