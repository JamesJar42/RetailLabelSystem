#pragma once

#include <QAction>
#include <QKeySequence>
#include <QSettings>
#include <QShortcut>

class ScopedSetting {
public:
    explicit ScopedSetting(const QString &key)
        : m_key(key), m_hadValue(QSettings().contains(key)), m_oldValue(QSettings().value(key)) {}

    ~ScopedSetting() {
        QSettings s;
        if (m_hadValue) s.setValue(m_key, m_oldValue);
        else s.remove(m_key);
    }

private:
    QString m_key;
    bool m_hadValue;
    QVariant m_oldValue;
};

inline QAction *findActionByText(const QObject *root, const QString &text) {
    const auto actions = root->findChildren<QAction *>();
    for (QAction *a : actions) {
        if (a && a->text() == text) return a;
    }
    return nullptr;
}

inline bool hasShortcut(const QObject *root, const QKeySequence &key) {
    const auto shortcuts = root->findChildren<QShortcut *>();
    for (QShortcut *sc : shortcuts) {
        if (sc && sc->key() == key) return true;
    }
    return false;
}
