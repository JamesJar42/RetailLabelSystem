#include <cassert>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QTableWidget>
#include <QTest>

#include "MainWindow.h"
#include "labelSystem.h"
#include "test_mainwindow_gui_utils.h"

class MainWindowGuiFiltersTests : public QObject {
    Q_OBJECT

private slots:
    void search_filters_rows_by_name_and_barcode();
    void quick_filter_shows_only_queued_rows();
    void row_selection_toggles_batch_action_bar();
    void quick_filter_price_changed_only();
    void simple_queue_mode_renders_checkbox_column();
    void invalid_edit_marks_cell_and_preserves_model();
};

void MainWindowGuiFiltersTests::search_filters_rows_by_name_and_barcode() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);
    ls.dtb.clear();
    ls.dtb.add(product("Milk", 1.50f, "111", 0));
    ls.dtb.add(product("Bread", 0.90f, "222", 2));
    ls.dtb.add(product("Eggs", 2.00f, "333", 0));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    auto *search = window.findChild<QLineEdit *>("searchBar");
    auto *quickFilter = window.findChild<QComboBox *>("quickFilterCombo");

    QVERIFY(table != nullptr);
    QVERIFY(search != nullptr);
    QVERIFY(quickFilter != nullptr);

    const int allIdx = quickFilter->findData("all");
    QVERIFY(allIdx >= 0);
    quickFilter->setCurrentIndex(allIdx);
    search->clear();
    QCoreApplication::processEvents();

    QCOMPARE(table->rowCount(), 3);

    search->setText("milk");
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 1);

    search->setText("222");
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 1);

    search->clear();
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 3);
}

void MainWindowGuiFiltersTests::quick_filter_shows_only_queued_rows() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);
    ls.dtb.clear();
    ls.dtb.add(product("Apple", 1.00f, "a1", 0));
    ls.dtb.add(product("Banana", 2.00f, "b2", 3));
    ls.dtb.add(product("Carrot", 3.00f, "c3", 1));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    auto *quickFilter = window.findChild<QComboBox *>("quickFilterCombo");

    QVERIFY(table != nullptr);
    QVERIFY(quickFilter != nullptr);

    const int allIdx = quickFilter->findData("all");
    QVERIFY(allIdx >= 0);
    quickFilter->setCurrentIndex(allIdx);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 3);

    const int queuedIdx = quickFilter->findData("queued");
    QVERIFY(queuedIdx >= 0);
    quickFilter->setCurrentIndex(queuedIdx);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 2);

    const int allIdx2 = quickFilter->findData("all");
    QVERIFY(allIdx2 >= 0);
    quickFilter->setCurrentIndex(allIdx2);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 3);
}

void MainWindowGuiFiltersTests::row_selection_toggles_batch_action_bar() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);
    ls.dtb.clear();
    ls.dtb.add(product("One", 1.00f, "1", 0));
    ls.dtb.add(product("Two", 2.00f, "2", 0));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    auto *batchBar = window.findChild<QWidget *>("batchActionBar");

    QVERIFY(table != nullptr);
    QVERIFY(batchBar != nullptr);

    table->clearSelection();
    QCoreApplication::processEvents();
    QVERIFY(batchBar->isHidden());

    table->selectRow(0);
    QCoreApplication::processEvents();
    QVERIFY(!batchBar->isHidden());

    table->clearSelection();
    QCoreApplication::processEvents();
    QVERIFY(batchBar->isHidden());
}

void MainWindowGuiFiltersTests::quick_filter_price_changed_only() {
    labelSystem ls;

    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 0, 1.00f));
    ls.dtb.add(product("B", 2.00f, "b", 0, 2.50f));
    ls.dtb.add(product("C", 3.00f, "c", 0, 3.20f));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    auto *quickFilter = window.findChild<QComboBox *>("quickFilterCombo");
    QVERIFY(table != nullptr);
    QVERIFY(quickFilter != nullptr);

    const int idx = quickFilter->findData("price_changed");
    QVERIFY(idx >= 0);
    quickFilter->setCurrentIndex(idx);
    QCoreApplication::processEvents();
    QCOMPARE(table->rowCount(), 2);
}

void MainWindowGuiFiltersTests::simple_queue_mode_renders_checkbox_column() {
    ScopedSetting modeScope("simple_queue_mode");
    QSettings s;
    s.setValue("simple_queue_mode", true);

    labelSystem ls;
    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 0));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    QTableWidgetItem *queueItem = table->item(0, 4);
    QVERIFY(queueItem != nullptr);
    QVERIFY((queueItem->flags() & Qt::ItemIsUserCheckable) != 0);

    queueItem->setCheckState(Qt::Checked);
    QCoreApplication::processEvents();
    QCOMPARE(queueItem->checkState(), Qt::Checked);

    queueItem->setCheckState(Qt::Unchecked);
    QCoreApplication::processEvents();
    QCOMPARE(queueItem->checkState(), Qt::Unchecked);
}

void MainWindowGuiFiltersTests::invalid_edit_marks_cell_and_preserves_model() {
    labelSystem ls;
    MainWindow window(&ls);
    window.show();
    QTest::qWait(20);

    ls.dtb.clear();
    ls.dtb.add(product("A", 1.00f, "a", 1));
    window.setLabelSystem(&ls);

    auto *table = window.findChild<QTableWidget *>("productTable");
    QVERIFY(table != nullptr);
    QCOMPARE(table->rowCount(), 1);

    QTableWidgetItem *priceItem = table->item(0, 2);
    QVERIFY(priceItem != nullptr);
    priceItem->setText("-9");
    QCoreApplication::processEvents();

    assert(ls.dtb.searchByBarcode("a").getPrice() == 1.0f);
    QVERIFY(!priceItem->toolTip().isEmpty());
}

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    MainWindowGuiFiltersTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_mainwindow_gui_filters.moc"
