#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMainWindow>
#include "labelSystem.h"
#include <QFileSystemWatcher>
#include <QTimer>

namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(labelSystem *labelSys, QWidget *parent = nullptr);
    ~MainWindow();

    void setLabelSystem(labelSystem *system);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    labelSystem *ls;
    bool isHighContrast;
    QFileSystemWatcher *qssWatcher;
    QTimer *qssReloadTimer;
    // Theme actions
    QAction *themeLightAct;
    QAction *themeDarkAct;
    QAction *themeReloadAct;
    QAction *themeChooseCustomAct;
    QString currentCustomQssPath;

private slots:
    void toggleTheme();
    void setTheme(const QString &theme);
    void reloadStylesheet();
    void onQssFileChanged(const QString &path);
    void onQssDirChanged(const QString &path);
    void updateThemeMenuChecks();
};

#endif // MAINWINDOW_H