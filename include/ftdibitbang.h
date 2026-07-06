#ifndef FTDIBITBANG_H
#define FTDIBITBANG_H

#include <QObject>
#include <QString>
#include "ftd2xx.h" // Knihovna od FTDI

class FtdiBitBang : public QObject
{
    Q_OBJECT
public:
    explicit FtdiBitBang(QObject *parent = nullptr);
    ~FtdiBitBang();

    // Otevře první nalezené FTDI zařízení
    bool openDevice();

    // Uzavře komunikaci
    void closeDevice();

    // Nastaví, které piny jsou výstupní a které vstupní
    // Maska (8 bitů): 1 = Výstup (TX), 0 = Vstup (RX)
    bool setDirection(quint8 mask);

    // Zapíše 8 bitů na výstupní piny
    bool writeByte(quint8 data);

    // Přečte aktuální logický stav všech 8 pinů
    bool readPins(quint8 &data);

    // Vypíše do konzole (qDebug) seznam všech připojených FTDI a jejich údaje
    void listDevices();

    // Otevře konkrétní zařízení podle jeho sériového čísla
    bool openDeviceBySerial(const QString &serialNumber);

    QString getLastError() const;

private:
    FT_HANDLE m_ftHandle;
    QString m_lastError;
};

#endif // FTDIBITBANG_H
