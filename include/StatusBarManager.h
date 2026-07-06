#ifndef STATUSBARMANAGER_H
#define STATUSBARMANAGER_H

#include <QObject>
#include <QString>

class StatusBarManager : public QObject
{
    Q_OBJECT

public:
    // Přístup k jediné instanci manažera
    static StatusBarManager& instance() {
        static StatusBarManager _instance;
        return _instance;
    }

    // Metoda, kterou budete volat odkudkoliv z projektu
    void showMessage(const QString& message, int timeout = 0) {
        emit messageRequested(message, timeout);
    }

signals:
    // Signál, který zachytí hlavní okno (MainWindow)
    void messageRequested(const QString& message, int timeout);

private:
    // Privátní konstruktory kvůli vzoru Singleton
    StatusBarManager() = default;
    ~StatusBarManager() = default;
    StatusBarManager(const StatusBarManager&) = delete;
    StatusBarManager& operator=(const StatusBarManager&) = delete;
};

#endif // STATUSBARMANAGER_H
