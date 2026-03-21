#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMainWindow>
#include "labelSystem.h"
#include <QFileSystemWatcher>
#include <QTimer>
#include <QString>
#include <functional>
#include <QMetaObject>

class QLabel;
class QLineEdit;
class QComboBox;
class QFrame;
class QTableWidgetItem;
class QPushButton;
class QAction;
class AppUpdater;

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
    QFileSystemWatcher *databaseWatcher;
    QTimer *qssReloadTimer;
    // Theme actions
    QAction *themeLightAct;
    QAction *themeDarkAct;
    QAction *themeHighContrastAct;
    QAction *themeReloadAct;
    QAction *themeChooseCustomAct;
    QString currentCustomQssPath;
    QString autosavePath;
    QString currentSearchQuery;
    QString currentQuickFilter;
    QLineEdit *searchBarInput;
    QLabel *resultsSummaryLabel;
    QFrame *batchActionBar;
    QFrame *undoActionBar;
    QLabel *undoActionLabel;
    QPushButton *undoActionButton;
    QTimer *undoActionExpiryTimer;
    QTimer *undoActionCountdownTimer;
    int undoActionSecondsRemaining;
    QString undoActionMessage;
    std::function<void()> pendingUndoAction;
    QAction *openQueueWorkspaceAction;
    AppUpdater *appUpdater;
    bool focusDebugEnabled;
    QMetaObject::Connection focusDebugConnection;

    void rebuildProductTable();
    void updateBatchActionBar();
    void updateResultsSummary(int shown, int total);
    bool passesQuickFilter(const product &pd) const;
    bool validateAndApplyCellEdit(QTableWidgetItem *item);
    void queueUndoAction(const QString &message, std::function<void()> undoAction);
    void clearUndoAction();
    void updateFocusDebugLogging();
    bool ensureCloverTokenForUse(QString &tokenOut, QString &errorOut, bool forceRefresh = false);

private slots:
    void toggleTheme();
    void setTheme(const QString &theme);
    void reloadStylesheet();
    void onQssFileChanged(const QString &path);
    void onQssDirChanged(const QString &path);
    void onDatabaseFileChanged(const QString &path);
    void updateThemeMenuChecks();
    void updateAddButtonState();
};

#endif // MAINWINDOW_H