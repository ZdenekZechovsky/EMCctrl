#include "ftdibitbang.h"
#include <QDebug>

FtdiBitBang::FtdiBitBang(QObject *parent)
    : QObject(parent), m_ftHandle(nullptr)
{
}

FtdiBitBang::~FtdiBitBang()
{
    closeDevice();
}

bool FtdiBitBang::openDevice()
{
    // Otevře zařízení na indexu 0 (první připojené)
    FT_STATUS status = FT_Open(0, &m_ftHandle);
    if (status != FT_OK) {
        m_lastError = QString("Nepodařilo se otevřít FTDI zařízení. Kód: %1").arg(status);
        return false;
    }
    return true;
}

void FtdiBitBang::closeDevice()
{
    if (m_ftHandle) {
        // Resetuje čip zpět do standardního režimu před zavřením
        FT_SetBitMode(m_ftHandle, 0x00, 0x00);
        FT_Close(m_ftHandle);
        m_ftHandle = nullptr;
    }
}

bool FtdiBitBang::setDirection(quint8 mask)
{
    if (!m_ftHandle) {
        m_lastError = "Zařízení není otevřené.";
        return false;
    }

    // 0x01 reprezentuje Asynchronous Bit Bang mód
    FT_STATUS status = FT_SetBitMode(m_ftHandle, mask, 0x01);
    if (status != FT_OK) {
        m_lastError = "Nepodařilo se nastavit bitbang mód.";
        return false;
    }

    // V bitbang módu určuje baudrate rychlost interního časovače.
    // Pro FT245BM je frekvence aktualizace pinů = baudrate * 16.
    // 9600 je obvykle pro běžné I/O operace zcela dostatečné.
    FT_SetBaudRate(m_ftHandle, 9600);

    return true;
}

bool FtdiBitBang::writeByte(quint8 data)
{
    if (!m_ftHandle) return false;

    DWORD bytesWritten = 0;
    FT_STATUS status = FT_Write(m_ftHandle, &data, 1, &bytesWritten);

    if (status != FT_OK || bytesWritten != 1) {
        m_lastError = "Chyba při zápisu dat.";
        return false;
    }
    return true;
}

bool FtdiBitBang::readPins(quint8 &data)
{
    if (!m_ftHandle) return false;

    // FT_GetBitMode načte okamžitý fyzický stav všech 8 pinů na sběrnici
    FT_STATUS status = FT_GetBitMode(m_ftHandle, &data);

    if (status != FT_OK) {
        m_lastError = "Chyba při čtení stavu pinů.";
        return false;
    }
    return true;
}

void FtdiBitBang::listDevices()
{
    DWORD numDevs = 0;
    // Nejdřív vytvoříme interní seznam zařízení v ovladači
    FT_STATUS status = FT_CreateDeviceInfoList(&numDevs);

    if (status != FT_OK) {
        qWarning() << "Chyba při zjišťování počtu FTDI zařízení.";
        return;
    }

    //qDebug() << "Nalezeno FTDI zařízení:" << numDevs;

    for (DWORD i = 0; i < numDevs; i++) {
        DWORD flags, type, id, locId;
        char serialNumber[16];
        char description[64];
        FT_HANDLE ftHandleTemp;

        // Načtení detailů o každém zařízení
        status = FT_GetDeviceInfoDetail(i, &flags, &type, &id, &locId, serialNumber, description, &ftHandleTemp);

        if (status == FT_OK) {            
#if 0
            qDebug() << "--- Zařízení index" << i << "---";
            qDebug() << "Typ čipu:" << type; // FT245BM bude mít specifickou hodnotu (typicky 0 pro BM, 4 pro R)
            qDebug() << "VID:PID:" << QString::number(id, 16).toUpper(); // Mělo by být 403:6001
            qDebug() << "Sériové číslo:" << serialNumber;
            qDebug() << "Popis:" << description;
#endif
        }
    }
}

bool FtdiBitBang::openDeviceBySerial(const QString &serialNumber)
{
    // FT_OpenEx umožňuje otevřít zařízení podle specifikátoru (zde sériové číslo)
    // Převádíme QString na klasický C-string, který API vyžaduje
    FT_STATUS status = FT_OpenEx((PVOID)serialNumber.toStdString().c_str(),
                                 FT_OPEN_BY_SERIAL_NUMBER,
                                 &m_ftHandle);

    if (status != FT_OK) {
        m_lastError = QString("Nepodařilo se otevřít FTDI se sériovým číslem %1. Kód: %2")
                          .arg(serialNumber).arg(status);
        return false;
    }

    return true;
}

QString FtdiBitBang::getLastError() const
{
    return m_lastError;
}
