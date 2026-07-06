#include "GpibDevice.h"
#include "StatusBarManager.h"
#include <qelapsedtimer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <QDebug>
#include <QThread>

#ifdef __linux__
#include <gpib/ib.h>
#define GPIB_IBSTA ibsta
#define GPIB_IBERR iberr
#define GPIB_IBCNT ibcnt
#define GPIB_IBCNTL ibcntl
#else
#include <windows.h>
#include <ni4882.h>
#define GPIB_IBSTA Ibsta()
#define GPIB_IBERR Iberr()
#define GPIB_IBCNT Ibcnt()
#define GPIB_IBCNTL Ibcntl()
#endif

constexpr quint8 MAX_DEV = 32;

tESI_SCAN_RANGES ranges[4] =
    {
        {
            .Start_MHz = 0.01,
            .Stop_MHz = 0.15,
            .Step_size_kHz = 0.4,
            .Res_BW_kHz = 1,
            .Meas_Time_ms = 10,
            .Auto_Ranging = 1,
            .RF_Attn_dB = 0,
            .Preamp = 1,
            .Auto_Preamp = 1,
            .Input = 2
        },
        {
            .Start_MHz = 0.15,
            .Stop_MHz = 30.0,
            .Step_size_kHz = 4.0,
            .Res_BW_kHz = 10,
            .Meas_Time_ms = 10,
            .Auto_Ranging = 1,
            .RF_Attn_dB = 0,
            .Preamp = 1,
            .Auto_Preamp = 1,
            .Input = 2
        },
        {
            .Start_MHz = 30.0,
            .Stop_MHz = 1000.0,
            .Step_size_kHz = 40,
            .Res_BW_kHz = 100,
            .Meas_Time_ms = 10,
            .Auto_Ranging = 1,
            .RF_Attn_dB = 0,
            .Preamp = 1,
            .Auto_Preamp = 1,
            .Input = 2
        },
        {
            .Start_MHz = 1000.0,
            .Stop_MHz = 6000.0,
            .Step_size_kHz = 400,
            .Res_BW_kHz = 1000,
            .Meas_Time_ms = 10,
            .Auto_Ranging = 1,
            .RF_Attn_dB = 0,
            .Preamp = 1,
            .Auto_Preamp = 1,
            .Input = 1
        }
};

GpibDevice::GpibDevice(QObject *parent) : QObject(parent) {
    //qDebug() << "GpibDevice hardware layer created";
}

int GpibDevice::checkError(int deviceDescriptor, const char* msg) {
    if (GPIB_IBSTA & ERR) {
        qDebug() << "GPIB Error:" << msg;
        return -1;
    }
    return 0;
}

int GpibDevice::writeCommand(int deviceDescriptor, const char* cmd) {
    ibwrt(deviceDescriptor, cmd, strlen(cmd));
    return checkError(deviceDescriptor, "ibwrt Error");
}

int GpibDevice::readData(int deviceDescriptor, char* buffer, int maxLen) {
    ibrd(deviceDescriptor, buffer, maxLen - 1);
    buffer[GPIB_IBCNT] = '\0';
    return checkError(deviceDescriptor, "ibrd Error");
}

int GpibDevice::close(tEMC_measurement* pEMC) {
    char buffer[] = "SYSTem:LOCal\n";
    unsigned char addr[] = { pEMC->ATT_addr, pEMC->ESI_addr, pEMC->GEN_addr, pEMC->PWR_addr, pEMC->SMT_addr};
    const int numAddr = sizeof(addr) / sizeof(addr[0]);

    for (int i = 0; i < numAddr; i++) {
        if (addr[i]) {
            ibwrt(addr[i], buffer, strlen(buffer));
            ibloc(addr[i]);
            ibonl(addr[i], 0);
        }
    }
    return 0;
}

int GpibDevice::init(tEMC_measurement* pEMC) {
    Addr4882_t instruments[MAX_DEV];
    Addr4882_t result[MAX_DEV];
    char buffer[256];

    memset(pEMC, 0, sizeof(tEMC_measurement));
    SendIFC(0);

    for (int i = 0; i < MAX_DEV; i++) {
        instruments[i] = MakeAddr(i, 0);
    }
    instruments[MAX_DEV - 1] = NOADDR;

    FindLstn(0, instruments, result, MAX_DEV);
    if (checkError(0, "FindLstn Error")) return -1;

    int count = ibcnt;
    for (int i = 1; i < count; i++) {
        int pad = GetPAD(result[i]);
        int ud = ibdev(0, pad, 0, T1s, 1, 0);
        if (ud < 0) continue;

        /*RF Step attenuator - doesn't respond on "*IDN?" command */
        if (pad == 1)
        {
            pEMC->ATT_addr = pad;
            continue;
        }

        writeCommand(ud, "*IDN?\n");        

        if (!(GPIB_IBSTA & ERR)) {
            memset(buffer, 0, sizeof(buffer));
            readData(ud, buffer, sizeof(buffer));

            if (strstr(buffer, "Rohde&Schwarz,SMT02")) pEMC->SMT_addr = pad;
            else if (strstr(buffer, "HEWLETT-PACKARD,E3631A")) pEMC->PWR_addr = pad;
            else if (strstr(buffer, "Rohde&Schwarz,ESI 26")) pEMC->ESI_addr = pad;
            else if (strstr(buffer, "HEWLETT-PACKARD,33120A") || strstr(buffer, "Agilent Technologies,33220A")) pEMC->GEN_addr = pad;
        }
        ibonl(ud, 0);
    }
    return 0;
}

int GpibDevice::setRfStepAttenuator(unsigned char gpibAddr, unsigned char attenuate) {
    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    char commandstr[32];
    snprintf(commandstr, sizeof(commandstr), "A%d,", attenuate);
    writeCommand(ud, commandstr);
    ibonl(ud, 0);
    return 0;
}

int GpibDevice::setGenerator(unsigned char gpibAddr, double freq, double volt, double offset, int function) {
    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    char cmd[64];

    switch(function)
    {
    case 0:
        snprintf(cmd, sizeof(cmd), "FUNC SIN");
        break;
    case 1:
        snprintf(cmd, sizeof(cmd), "FUNC SQU");
        break;
    case 2:
        snprintf(cmd, sizeof(cmd), "FUNC RAMP");
        break;
    case 3:
        snprintf(cmd, sizeof(cmd), "FUNC PULS");
        break;
    case 4:
        snprintf(cmd, sizeof(cmd), "FUNC NOIS");
        break;
    default:
        snprintf(cmd, sizeof(cmd), "FUNC SIN");
        break;
    };

    //qDebug() << function << cmd;
    writeCommand(ud, cmd);


    snprintf(cmd, sizeof(cmd), "VOLT %.2f", volt); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "FREQ %.2f", freq); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "VOLT:OFFS %.2f", offset); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "OUTP ON"); writeCommand(ud, cmd);

    ibonl(ud, 0);
    return 0;
}

int GpibDevice::getPowerVoltage(unsigned char gpibAddr, double* p6V, double* p25V, double* m25V) {
    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    char data[128];
    writeCommand(ud, "MEASURE:VOLT:DC? P6V"); readData(ud, data, sizeof(data)); *p6V = atof(data);
    writeCommand(ud, "MEASURE:VOLT:DC? P25V"); readData(ud, data, sizeof(data)); *p25V = atof(data);
    writeCommand(ud, "MEASURE:VOLT:DC? N25V"); readData(ud, data, sizeof(data)); *m25V = atof(data);

    ibonl(ud, 0);
    return 0;
}

int GpibDevice::getPowerCurrent(unsigned char gpibAddr, double* p6I, double* p25I, double* m25I) {
    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    char data[128];
    writeCommand(ud, "MEASURE:CURRENT:DC? P6V"); readData(ud, data, sizeof(data)); *p6I = atof(data);
    writeCommand(ud, "MEASURE:CURRENT:DC? P25V"); readData(ud, data, sizeof(data)); *p25I = atof(data);
    writeCommand(ud, "MEASURE:CURRENT:DC? N25V"); readData(ud, data, sizeof(data)); *m25I = atof(data);

    ibonl(ud, 0);
    return 0;
}

int GpibDevice::setPowerSupply(unsigned char gpibAddr, double p6V, double p25V, double m25V, double p6I, double p25I, double m25I) {
    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    char cmd[32];
    writeCommand(ud, "INST P6V");
    snprintf(cmd, sizeof(cmd), "VOLT %.2f", p6V); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "CURR %.2f", p6I); writeCommand(ud, cmd);

    writeCommand(ud, "INST P25V");
    snprintf(cmd, sizeof(cmd), "VOLT %.2f", p25V); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "CURR %.2f", p25I); writeCommand(ud, cmd);

    writeCommand(ud, "INST N25V");
    snprintf(cmd, sizeof(cmd), "VOLT %.2f", m25V); writeCommand(ud, cmd);
    snprintf(cmd, sizeof(cmd), "CURR %.2f", m25I); writeCommand(ud, cmd);

    ibonl(ud, 0);
    return 0;
}

int GpibDevice::enablePowerSupply(unsigned char gpibAddr, int on) {
    char cmd[32];

    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    if(on)
    {
        snprintf(cmd, sizeof(cmd), "OUTP ON");
    }
    else
    {
        snprintf(cmd, sizeof(cmd), "OUTP OFF");
    }
    writeCommand(ud, cmd);
    ibonl(ud, 0);
    return 0;
}

int GpibDevice::setDisplay(unsigned char gpibAddr, int num) {
    char cmd[32];

    int ud = ibdev(0, gpibAddr, 0, T10s, 1, 0);
    if (checkError(ud, "ibdev Error")) return -1;

    const char *cmd1[] ={"INST:SEL P6V","INST:SEL P25V","INST:SEL N25V"};
    writeCommand(ud, cmd1[num % 3]);
    ibonl(ud, 0);
    return 0;
}

double GpibDevice::initSpectrumReadCE102(unsigned char gpibAddr, double fstart, double fstop, int data_chunk_time) {
    if (gpibAddr == 0) return 0.0;

    int timeout = T1s;
    if(data_chunk_time <= 2) {
        timeout = T10s;
    }
    else if(data_chunk_time <= 6) {
        timeout = T10s;
    }
    else if(data_chunk_time <= 26) {
        timeout = T30s;
    }
    else if(data_chunk_time <= 96) {
        timeout = T100s;
    }
    else if(data_chunk_time <= 296) {
        timeout = T300s;
    }
    else if(data_chunk_time <= 996) {
        timeout = T1000s;
    }

    esiDescriptor = ibdev(0, gpibAddr, 0, timeout, 1, 0);

    writeCommand(esiDescriptor, "*RST\n");
    writeCommand(esiDescriptor, "*CLS\n");
    //writeCommand(esiDescriptor, "*IDN?\n");

    writeCommand(esiDescriptor, "INST:SEL REC\n");
    writeCommand(esiDescriptor, "DISP:PSAV OFF\n");
    writeCommand(esiDescriptor, "DISP:FORM SING\n");
    writeCommand(esiDescriptor, "DISP:TRAC:Y 100dB\n");
    writeCommand(esiDescriptor, "INIT2:DISP ON\n");
    writeCommand(esiDescriptor, "INP:TYPE INPUT2\n");          // input 2
    writeCommand(esiDescriptor, "INP:ATT:PROT OFF\n");
    writeCommand(esiDescriptor, "INP:ATT:AUTO ON\n");          // auto attenuator on
    writeCommand(esiDescriptor, "INP:GAIN:STAT ON\n");        // preamp off (gain)
    writeCommand(esiDescriptor, "INP:GAIN:AUTO ON\n");
    writeCommand(esiDescriptor, "SYSTem:DISPlay:UPDate ON\n");


    writeCommand(esiDescriptor, "CORR:TRAN OFF\n");
    writeCommand(esiDescriptor, "INIT2:CONT OFF\n");

    writeCommand(esiDescriptor, "TRAC1:MODE WRIT\n");
    writeCommand(esiDescriptor, "FORM REAL,32\n");

    writeCommand(esiDescriptor, "FREQ:MODE SCAN\n");
    writeCommand(esiDescriptor, "SCAN:RANG:COUN 4\n");

    char cmd[64];
    //snprintf(cmd, sizeof(cmd), "FREQ:STAR %lf Hz\n", fstart); writeCommand(esiDescriptor, cmd);
    //snprintf(cmd, sizeof(cmd), "FREQ:STOP %lf Hz\n", fstop); writeCommand(esiDescriptor, cmd);
    writeCommand(esiDescriptor, "SENS:DET:REC:FUNC POS\n");

    int i = 1;
    for (auto& range : ranges) {
        snprintf(cmd, sizeof(cmd), "SCAN%d:STAR %.2f MHz\n", i, range.Start_MHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:STOP %.2f MHz\n", i, range.Stop_MHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:STEP %.1f kHz\n", i, range.Step_size_kHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:BAND:RES %d kHz\n", i, range.Res_BW_kHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:TIME %d ms\n", i, range.Meas_Time_ms);
        writeCommand(esiDescriptor, cmd);

        // OPRAVA: Přidáno :AUTO do příkazu
        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:ATT:AUTO %s\n", i, (range.Auto_Ranging) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        // OPRAVA: Odstraněna mezera za % a smazáno MHz
        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:ATT %ddB\n", i, range.RF_Attn_dB);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:GAIN:STAT %s\n", i, (range.Preamp) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:GAIN:AUTO %s\n", i, (range.Auto_Preamp) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:TYPE INPUT%d\n", i, range.Input);
        writeCommand(esiDescriptor, cmd);
        i++;
    }

    writeCommand(esiDescriptor, "DISP:TRAC1 ON\n");
    writeCommand(esiDescriptor, "DISP:TRAC2 OFF\n");
    writeCommand(esiDescriptor, "DISP:TRAC3 OFF\n");
    writeCommand(esiDescriptor, "DISP:TRAC4 OFF\n");

    writeCommand(esiDescriptor, "INIT2:CONT OFF\n");

    writeCommand(esiDescriptor, "CALC:LIM1:NAME 'CE102'\n");
    writeCommand(esiDescriptor, "CALC:LIM1:TRAC 1\n");
    writeCommand(esiDescriptor, "CALC:LIM1:UPP:STAT ON\n");
    writeCommand(esiDescriptor, "CALC:LIM1:STAT ON\n");

    return 0;
}

double GpibDevice::initSpectrumRead(unsigned char gpibAddr, double fstart, double fstop, int ord, int data_chunk_time) {
    if (gpibAddr == 0) return 0.0;

    int timeout = T1s;
    if(data_chunk_time <= 2) {
        timeout = T10s;
    }
    else if(data_chunk_time <= 6) {
        timeout = T10s;
    }
    else if(data_chunk_time <= 26) {
        timeout = T30s;
    }
    else if(data_chunk_time <= 96) {
        timeout = T100s;
    }
    else if(data_chunk_time <= 296) {
        timeout = T300s;
    }
    else if(data_chunk_time <= 996) {
        timeout = T1000s;
    }

    esiDescriptor = ibdev(0, gpibAddr, 0, timeout, 1, 0);

    writeCommand(esiDescriptor, "*RST\n");
    writeCommand(esiDescriptor, "*CLS\n");
    //writeCommand(esiDescriptor, "*IDN?\n");

    writeCommand(esiDescriptor, "INST:SEL REC\n");
    writeCommand(esiDescriptor, "DISP:PSAV OFF\n");
    writeCommand(esiDescriptor, "DISP:FORM SING\n");
    writeCommand(esiDescriptor, "DISP:TRAC:Y 60dB\n");
    writeCommand(esiDescriptor, "INIT1:DISP ON\n");
    writeCommand(esiDescriptor, "INP:ATT:PROT OFF\n");
    writeCommand(esiDescriptor, "INP:ATT:AUTO ON\n");          // auto attenuator on
    writeCommand(esiDescriptor, "INP:GAIN:STAT ON\n");        // preamp off (gain)
    writeCommand(esiDescriptor, "INP:GAIN:AUTO ON\n");
    writeCommand(esiDescriptor, "SYSTem:DISPlay:UPDate ON\n");


    // Transducer selection
    switch(ord) {
        case 0: writeCommand(esiDescriptor, "CORR:TRAN:SEL 'HFH2-Z6'\n"); break;
        case 1: writeCommand(esiDescriptor, "CORR:TRAN:SEL 'HK116'\n"); break;
        case 2: writeCommand(esiDescriptor, "CORR:TRAN:SEL 'HL223'\n"); break;
        case 3: writeCommand(esiDescriptor, "CORR:TRAN:SEL 'HF906'\n"); break;
    }

    writeCommand(esiDescriptor, "CORR:TRAN ON\n");
    writeCommand(esiDescriptor, "INIT2:CONT OFF\n");
    writeCommand(esiDescriptor, "INIT1:CONT OFF\n");

    writeCommand(esiDescriptor, "TRAC1:MODE WRIT\n");
    writeCommand(esiDescriptor, "FORM REAL,32\n");

    writeCommand(esiDescriptor, "FREQ:MODE SCAN\n");
    writeCommand(esiDescriptor, "SCAN:RANG:COUN 4\n");

    char cmd[128];
    //snprintf(cmd, sizeof(cmd), "FREQ:STAR %lf Hz\n", fstart); writeCommand(esiDescriptor, cmd);
    //snprintf(cmd, sizeof(cmd), "FREQ:STOP %lf Hz\n", fstop); writeCommand(esiDescriptor, cmd);
    writeCommand(esiDescriptor, "SENS:DET:REC:FUNC POS\n");

    int i = 1;
    for (auto& range : ranges) {
        snprintf(cmd, sizeof(cmd), "SCAN%d:STAR %.2f MHz\n", i, range.Start_MHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:STOP %.2f MHz\n", i, range.Stop_MHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:STEP %.1f kHz\n", i, range.Step_size_kHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:BAND:RES %d kHz\n", i, range.Res_BW_kHz);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:TIME %d ms\n", i, range.Meas_Time_ms);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:ATT:AUTO %s\n", i, (range.Auto_Ranging) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:ATT %ddB\n", i, range.RF_Attn_dB);
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:GAIN:STAT %s\n", i, (range.Preamp) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:GAIN:AUTO %s\n", i, (range.Auto_Preamp) ? "ON" : "OFF");
        writeCommand(esiDescriptor, cmd);

        snprintf(cmd, sizeof(cmd), "SCAN%d:INP:TYPE INPUT%d\n", i, range.Input);
        writeCommand(esiDescriptor, cmd);
        i++;
    }

    writeCommand(esiDescriptor, "DISP:TRAC1 ON\n");
    writeCommand(esiDescriptor, "DISP:TRAC2 OFF\n");
    writeCommand(esiDescriptor, "DISP:TRAC3 OFF\n");
    writeCommand(esiDescriptor, "DISP:TRAC4 OFF\n");

    writeCommand(esiDescriptor, "INIT2:CONT OFF\n");
    writeCommand(esiDescriptor, "INIT1:CONT OFF\n");

    writeCommand(esiDescriptor, "CALC:LIM1:NAME 'RE102-3'\n");
    writeCommand(esiDescriptor, "CALC:LIM1:TRAC 1\n");
    writeCommand(esiDescriptor, "CALC:LIM1:UPP:STAT ON\n");
    writeCommand(esiDescriptor, "CALC:LIM1:STAT ON\n");

    return 0;
}
#if 0
int GpibDevice::sweepEsiBlocked(int ud,
                                double fstart,
                                double fstop,
                                double fpart,
                                unsigned char* p,                                
                                const std::shared_ptr<std::atomic<bool>>& stop,
                                const std::shared_ptr<std::atomic<bool>>& pause,
                                double fstart_orig,
                                double fstop_orig) {
    double f0 = fstart;
    char str[256];
    unsigned char header[16];
    unsigned int totalsize = 0;

    //qDebug() << "fstart: " << fstart << " fstop: " << fstop;

    // Explicitně nastavíme formát pro binární přenos (REAL, 32-bit)
    writeCommand(ud, "FORM REAL,32\n");

    while (f0 < fstop) {
        while (*pause && !(*stop)) {
            QThread::msleep(100);
        }
        if (*stop) break;

        double f1 = f0 + fpart;
        if (f1 > fstop) f1 = fstop;

        const int MAX_RETRIES = 5;
        bool chunkSuccess = false;
        int size = 0;

        // 1. Určení správného indexu SCAN tabulky (1 až 4) podle aktuální frekvence f0
        int scanIdx = 1;
        if (f0 >= 1000.0 * 1e6) {
            scanIdx = 4;
        } else if (f0 >= 30.0 * 1e6) {
            scanIdx = 3;
        } else if (f0 >= 0.15 * 1e6) {
            scanIdx = 2;
        } else {
            scanIdx = 1;
        }

        //qDebug() << "scanIdx:" << scanIdx;

        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
            // Vyčištění chyb a bufferů před novým pokusem
            if (attempt > 0) {
                writeCommand(ud, "*CLS\n");
                QThread::msleep(50); // Krátká pauza pro vzpamatování přístroje
            }
            snprintf(str, sizeof(str), "FREQ:STAR %.3fMHz\n", f0 / 1e6);
            writeCommand(ud, str);
            //qDebug() << str;

            snprintf(str, sizeof(str), "FREQ:STOP %.3fMHz\n", f1 / 1e6);
            writeCommand(ud, str);
            //qDebug() << str;

            writeCommand(ud, "ABOR; INIT2; *OPC?\n");

            readData(ud, str, sizeof(str));
            writeCommand(ud, "TRAC? TRACE1\n");

            // 1. Kontrola začátku bloku
            ibrd(ud, header, 2);
            if (header[0] != '#') {
                qDebug() << "Hlavička nezačíná '#', data jsou posunutá, zkusit znovu";
                continue; // Hlavička nezačíná '#', data jsou posunutá, zkusit znovu
            }

            int digits = header[1] - '0';
            if (digits < 1 || digits > 9) {
                qDebug() << "Neplatná délka";
                continue; // Neplatná délka
            }

            ibrd(ud, header, digits);
            header[digits] = 0;
            size = atoi((char*)header);

            if (size <= 0 || size > 64000){
                qDebug() << "Nesmyslná velikost dat";
                continue; // Nesmyslná velikost dat
            }
            //qDebug() << "Size:" << size;

            // Čteme data přímo do bufferu
            ibrd(ud, p, size);
            // Kontrola, zda jsme přečetli tolik bytů, kolik přístroj slíbil v hlavičce
            if (ibcnt != size) {
                continue; // Data jsou nekompletní, zkusit celý blok vyčíst znovu
            }

            // 3. Validace dat (Sanity Check)
            int numFloats = size / sizeof(float);
            float* floatPtr = (float*)p;
            bool dataValid = true;

            for (int i = 0; i < numFloats; ++i) {
                float val = floatPtr[i];
                // Kontrola na NaN, Infinity a nereálné hodnoty spektra (např. mimo -200 až +200 dBm/dBµV)
                if (std::isnan(val) || std::isinf(val) || val < -200.0f || val > 200.0f) {
                    dataValid = false;
                    qDebug() << "data invalid";
                    break;
                }
            }

            if (dataValid) {
                chunkSuccess = true;
                break; // Data jsou v pořádku, vyskočíme z retry cyklu
            }
        }

        if (!chunkSuccess) {
            // Pokud selžou všechny pokusy, můžete buď přerušit měření,
            // nebo vložit do grafu nulová data, aby se předešlo pádu aplikace.
            // Zde pro ukázku přerušíme cyklus.
            break;
        }

        // -- ZPRACOVÁNÍ PLATNÝCH DAT --
        int numFloats = size / sizeof(float);
        float* floatPtr = (float*)p;

        QVector<double> chunkFreqs;
        QVector<double> chunkLevels;

        if (numFloats > 0) {
            chunkFreqs.reserve(numFloats);
            chunkLevels.reserve(numFloats);
            double stepFreq = (numFloats > 1) ? (f1 - f0) / (numFloats - 1) : 0;

            //qDebug() << "stepFreq [Hz]:" << stepFreq;

            for (int i = 0; i < numFloats; ++i) {
                chunkFreqs.append(f0 + i * stepFreq);
                chunkLevels.append(floatPtr[i]);
            }

            emit spectrumChunkReady(chunkFreqs, chunkLevels);
        }

        totalsize += size;
        p += size; // Posuneme ukazatel až po ověření platnosti
        f0 = f1;

        emit hardwareProgress((int)((f0 - fstart_orig) * 100.0 / (fstop_orig - fstart_orig)));
    }

    return totalsize / sizeof(float);
}
#else
static bool readExact(int ud, unsigned char* buf, int n)
{
    int got = 0;

    while (got < n) {
        ibrd(ud, buf + got, n - got);
        if (ibcnt <= 0) return false;
        got += ibcnt;
    }
    return true;
}

static bool findChar(int ud, unsigned char target, unsigned char &out, int maxScan = 2000)
{
    for (int i = 0; i < maxScan; ++i) {
        if (!readExact(ud, &out, 1))
            return false;

        if (out == target)
            return true;
    }
    return false;
}

static bool readIEEEBlock(int ud, unsigned char* p, int maxSize, int &outSize)
{
    unsigned char ch;

    // =========================
    // 1. RESYNC na '#'
    // =========================
    if (!findChar(ud, '#', ch)) {
        qDebug() << "IEEE resync failed (no '#')";
        return false;
    }

    // =========================
    // 2. digits
    // =========================
    unsigned char digitChar;
    if (!readExact(ud, &digitChar, 1)){
        qDebug() << digitChar;
        return false;
    }

    if (digitChar < '0' || digitChar > '9') {
        qDebug() << "Bad digit char:" << digitChar;
        return false;
    }

    int digits = digitChar - '0';

    if (digits == 0) {
        qDebug() << "Zero-length block (skip)";
        return false;
    }

    // =========================
    // 3. length
    // =========================
    char lenStr[16] = {0};

    if (!readExact(ud, (unsigned char*)lenStr, digits))
        return false;

    outSize = atoi(lenStr);

    if (outSize <= 0 || outSize > maxSize) {
        qDebug() << "Invalid block size:" << outSize;
        return false;
    }

    // =========================
    // 4. data
    // =========================
    if (!readExact(ud, p, outSize)) {
        qDebug() << "Incomplete IEEE block";
        return false;
    }

    return true;
}

int GpibDevice::sweepEsiBlocked(
    int ud,
    double fstart,
    double fstop,
    double fpart,
    unsigned char* p,
    const std::shared_ptr<std::atomic<bool>>& stop,
    const std::shared_ptr<std::atomic<bool>>& pause,
    double fstart_orig,
    double fstop_orig)
{
    double f0 = fstart;
    char str[256];

    unsigned char* pBase = p;
    int maxBuffer = 64000 * 4; // bezpečnostní limit (floaty)

    int totalsize = 0;

    writeCommand(ud, "FORM REAL,32\n");

    while (f0 < fstop) {

        while (*pause && !(*stop))
            QThread::msleep(100);

        if (*stop) break;

        double f1 = f0 + fpart;
        if (f1 > fstop) f1 = fstop;

        const int MAX_RETRIES = 5;
        bool chunkSuccess = false;
        int size = 0;

        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {

            if (attempt > 0) {
                writeCommand(ud, "ABOR\n");
                writeCommand(ud, "*CLS\n");
                QThread::msleep(50);
            }

            snprintf(str, sizeof(str), "FREQ:STAR %.3fMHz\n", f0 / 1e6);
            writeCommand(ud, str);

            snprintf(str, sizeof(str), "FREQ:STOP %.3fMHz\n", f1 / 1e6);
            writeCommand(ud, str);

            writeCommand(ud, "ABOR; INIT2; *OPC?\n");

            QElapsedTimer timer;
            timer.start();

            while (timer.elapsed() < 10000) {
                char str[64] = {0};

                readData(ud, str, sizeof(str));

                if (str[0] == '1')
                    break;

                QThread::msleep(10);
            }

            QThread::msleep(100);
            writeCommand(ud, "TRAC? TRACE1\n");

            //int size = 0;
            if (!readIEEEBlock(ud, p, 64000, size)) {
                qDebug() << "IEEE block failed";
                continue;
            }

            // sanity check floatů
            int numFloats = size / sizeof(float);

            float* fptr = reinterpret_cast<float*>(p);

            bool ok = true;
            for (int i = 0; i < numFloats; ++i) {
                float v = fptr[i];
                if (std::isnan(v) || std::isinf(v) || v < -200.f || v > 200.f) {
                    ok = false;
                    break;
                }
            }

            if (!ok) {
                qDebug() << "Data invalid (NaN/INF/out of range)";
                continue;
            }

            chunkSuccess = true;
            break;
        }

        if (!chunkSuccess)
            break;

        // =========================
        // zpracování dat
        // =========================

        int numFloats = size / sizeof(float);
        float* fptr = reinterpret_cast<float*>(p);

        QVector<double> chunkFreqs;
        QVector<double> chunkLevels;

        chunkFreqs.reserve(numFloats);
        chunkLevels.reserve(numFloats);

        double stepFreq = (numFloats > 1)
                              ? (f1 - f0) / (numFloats - 1)
                              : 0;

        StatusBarManager::instance().showMessage(QString("Data block size %1, step %2 kHz").arg(numFloats).arg(stepFreq / 1000.0));

        for (int i = 0; i < numFloats; ++i) {
            chunkFreqs.append(f0 + i * stepFreq);
            chunkLevels.append(fptr[i]);
        }

        emit spectrumChunkReady(chunkFreqs, chunkLevels);

        totalsize += size;
        p += size;
        f0 = f1;

        emit hardwareProgress(
            int((f0 - fstart_orig) * 100.0 / (fstop_orig - fstart_orig))
            );
    }

    return totalsize / sizeof(float);
}
#endif

double GpibDevice::readEsiSingleVoltage(int udESI) {
    char readBuf[64];
    const int MAX_RETRIES = 3;

    for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
        // 1. Vyčištění bufferu před každým čtením
        memset(readBuf, 0, sizeof(readBuf));

        // Případné vyčištění chyb při opakovaném pokusu
        if (attempt > 0) {
            writeCommand(udESI, "*CLS\n");
            QThread::msleep(50);
        }

        writeCommand(udESI, "INIT; *WAI\n");
        writeCommand(udESI, "TRAC? SING\n");
        int err = readData(udESI, readBuf, sizeof(readBuf));

        if (err != 0) {
            qDebug() << "GPIB chyba při čtení z ESI, pokus:" << attempt + 1;
            continue; // Zkusit znovu
        }

        // 2. Převod na QString a odstranění bílých znaků (např. \r, \n)
        QString valStr = QString::fromLatin1(readBuf).trimmed();

        if (valStr.isEmpty()) {
            qDebug() << "ESI vrátilo prázdný řetězec, pokus:" << attempt + 1;
            continue;
        }

        // 3. Bezpečný převod na double
        bool parseOk = false;
        double value = valStr.toDouble(&parseOk);

        // 4. Komplexní validace
        if (parseOk && !std::isnan(value) && !std::isinf(value)) {
            // Kontrola fyzikálních mezí (např. od -100 do +200 dBµV)
            // Zabrání průchodu nesmyslů, kdyby ESI vrátilo např. "+9.999999999E+37" (overload hodnota)
            if (value > -100.0 && value < 250.0) {
                return value; // Máme platnou hodnotu, vracíme a končíme smyčku
            } else {
                qDebug() << "Hodnota z ESI je mimo fyzikální rozsah (" << value << "), pokus:" << attempt + 1;
            }
        } else {
            qDebug() << "Nelze převést na číslo z ESI (" << valStr << "), pokus:" << attempt + 1;
        }
    }

    // Pokud selžou všechny pokusy, vraťte hodnotu indikující tvrdou chybu.
    // Vaše funkce testRegulator chytá hodnoty pod -90.0 jako chybu,
    // takže vrácení -999.0 nebo -100.0 bezpečně ukončí měření a vypne generátor.
    qDebug() << "KRITICKÁ CHYBA: ESI neodpovídá platnými daty.";
    return -999.0;
}

void GpibDevice::setInstrumentsFrequency(int udESI, int udSMT, double freq) {
    char cmd[64];

    snprintf(cmd, sizeof(cmd), "FREQ:FIX %lf\n", freq);
    writeCommand(udESI, cmd);

    snprintf(cmd, sizeof(cmd), "FREQ %lf\n", freq);
    writeCommand(udSMT, cmd);
}

void GpibDevice::setSmtLevel(int udSMT, double level) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "POW %10.2lfDBUV\n", level);
    writeCommand(udSMT, cmd);
}

void GpibDevice::setAttenutor(int udESI, quint8 attenuate) {
    char cmd[64];
    snprintf(cmd, sizeof(cmd), "A%d,", attenuate);
    writeCommand(udESI, cmd);
}
