#include "../include/ProductDelegate.h"
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QSettings>

ProductDelegate::ProductDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QWidget *ProductDelegate::createEditor(QWidget *parent,
                                       const QStyleOptionViewItem &option,
                                       const QModelIndex &index) const
{
    if (index.column() == 2 || index.column() == 3) { // Price & Was Price
        QDoubleSpinBox *editor = new QDoubleSpinBox(parent);
        editor->setFrame(false);
        editor->setMinimum(0.0);
        editor->setMaximum(10000.0);
        editor->setDecimals(2);
        return editor;
    } else if (index.column() == 4) { // Print Qty
        QSettings s;
        if (s.value("simple_queue_mode", false).toBool()) {
            return nullptr; // Let QTableWidget handle check state toggling
        }
        QSpinBox *editor = new QSpinBox(parent);
        editor->setFrame(false);
        editor->setRange(0, 999);
        return editor;
    }

    return QStyledItemDelegate::createEditor(parent, option, index);
}

void ProductDelegate::setEditorData(QWidget *editor,
                                    const QModelIndex &index) const
{
    if (index.column() == 2 || index.column() == 3) {
        double value = index.model()->data(index, Qt::EditRole).toDouble();
        QDoubleSpinBox *spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->setValue(value);
    } else if (index.column() == 4) {
        int value = index.model()->data(index, Qt::EditRole).toInt();
        QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
        spinBox->setValue(value);
    } else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void ProductDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                   const QModelIndex &index) const
{
    if (index.column() == 2 || index.column() == 3) {
        QDoubleSpinBox *spinBox = static_cast<QDoubleSpinBox*>(editor);
        spinBox->interpretText();
        double value = spinBox->value();
        model->setData(index, value, Qt::EditRole);
    } else if (index.column() == 4) {
        QSpinBox *spinBox = static_cast<QSpinBox*>(editor);
        spinBox->interpretText();
        int value = spinBox->value();
        model->setData(index, value, Qt::EditRole);
    } else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

void ProductDelegate::updateEditorGeometry(QWidget *editor,
                                           const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const
{
    editor->setGeometry(option.rect);
}
