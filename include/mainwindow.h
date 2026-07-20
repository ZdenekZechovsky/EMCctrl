#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <atomic>
#include <QMainWindow>
#include <QVector>
#include "qcustomplot.h"

#include "GpibDevice.h"
#include "EmcMeasurementManager.h"
#include "ftdibitbang.h"
#include "PowerAmplifier.h"
#include "networkautomation.h"
#include "MediaButtonsWidget.h"

// Dopředné deklarace tříd (zlepšuje rychlost kompilace)
class GpibDevice;
class EmcMeasurementManager;
class QTimer;
class QMouseEvent;
class NetworkAutomaton;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // --- Správa připojení a stavu (Connection & State) ---
    void onConnectClicked();
    void onDisConnectClicked();
    void updateInterfaceState();
    void updatePaStatus();

    // --- Ovládání HW a UI prvky (Instrument Controls) ---
    void onAttenuatorValueChanged(int value);
    void onAttenuatorReleased();
    void setpwrClicked();
    void onpwrClicked();
    void getpwrClicked();
    void setgenClicked();
    
    void vertical1Clicked();        
    void TransducerChanged();
    
    void onPARemoteClicked();
    void onPAHVOnClicked();
    void onPAAmpOnClicked();
    void onPARun();
    void onPAStop();

    //networkautomation
    void testpushButton_clicked();

    // --- Spouštění měření (Measurement Triggers) ---
    void onSpectrum1Clicked();        
    void onCS114Clicked();
    void onCS114MeasureClicked();
    void stopMeasurement();
    void pauseMeasurement();

    // --- Vykreslování a grafy (Plotting Updates) ---
    void prepareNewGraph(); // Nový slot pro přípravu grafu
    void prepareNewGraphSpectrum();
    void updatePlot(double frequency, double level);
    void updateMultiPlot(double frequency, double level);
    void updatePlotMeasure(double frequency,
                           double maxUg,
                           double measuredCurrent,
                           double actualUg,
                           double limitImax);
    void addSpectrumChunk(const QVector<double>& freqs, const QVector<double>& levels);    
    void onMouseMove(QMouseEvent *event);
    void emergencyStop();
    void LoadCEGraph();
    void LoadCSGraph();

    /* State machine */
    void processPaRun();
    void on_tracerSpinBox_valueChanged(int arg1);
    void on_tracerSpinBox_1_valueChanged(int arg1);
    void disp1checkBox_stateChanged(int state);
    void disp2checkBox_stateChanged(int state);
    void disp3checkBox_stateChanged(int state);
    void on_corectSMT_clicked();
signals:
    void paSequenceFinished(bool success);

private:
    // --- Pomocné inicializační metody ---
    void setupVariables();
    void storeVariables();
    void setupGroups();
    void setupConnections();
    void setupWidgets();
    void setupPlots();
    void cleanSpectrum();

    void prepareAndStartCalibrationCS114();
    void prepareAndStartMeasurementCS114();
    void onPaReadyForCalibrationCS114(bool success);
    void onPaReadyForMeasurementCS114(bool success);
    void sweepS21Measurement();

    NetworkAutomaton *m_automaton;

    // --- Logika a wrappery ---
    void spectrumProcess(double fstart, double fstop, int ord);
    void spectrumProcess_CE102(double fstart, double fstop);
    QVector<double> getFilteredCS114Frequencies(double fstart, double fstop);
    QString SetFilename(QString devicename,
                                    QString measurement,
                                    QString note,
                                    double fstart,
                                    double fstop);

    // --- Vykreslovací funkce ---
    void setupPlotCE102(QCustomPlot *customPlot, double fstart, double fstop);
    void setupPlot(QCustomPlot *customPlot, double fstart, double fstop, int order);
    void setupPlotCS(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs);
    void setupPlotCSMeasure(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs);
    void setupPlotS21(QCustomPlot *customPlot, double fstart, double fstop, const QVector<double>& testFreqs);

    QCPGraph* m_currentGraph = nullptr; // Ukazatel na aktuální graf
    QList<QColor> m_graphColors;        // Seznam barev
    int m_colorIndex = 0;               // Počítadlo pro cyklování barev
    int retryCount = 0;                 // Čítač opakovaných pokusů pro stavový automat

    /* state machine */
    enum class PaRunState {
        Idle,
        /* on */
        SetRemote,
        WaitRemote,
        SetAmpOn,
        WaitAmpOn,
        WaitStandby,
        SetHV,
        WaitHV,

        /* off */
        ResetHV,
        WaitHVOff,
        ResetAmp,
        WaitAmpOff,

        Done,
        Error
    };

    PaRunState paState = PaRunState::Idle;

    QTimer paTimer;
    QElapsedTimer stepTimer;

    // --- Proměnné ---
    Ui::MainWindow *ui;
    
    // Nástroje pro QCustomPlot
    QCPItemTracer *m_tracer = nullptr;
    QCPItemText *m_tracerLabel = nullptr;

    QCPItemTracer *m_tracer_2 = nullptr;
    QCPItemText *m_tracerLabel_2 = nullptr;

    std::shared_ptr<std::atomic<bool>> stopFlag;
    std::shared_ptr<std::atomic<bool>> pauseFlag;
    
    // Časovače
    QTimer *statusTimer = nullptr;

    //File path
    QString filePath;
    QString filePath_2;

    GpibDevice *gpibDevice = nullptr;
    tEMC_measurement EMC;
    EmcMeasurementManager *measurementManager  = nullptr;
    FtdiBitBang *ftdi = nullptr;
    PowerAmplifier *pa = nullptr;

    bool m_ledState = false;

    MediaButtonsWidget *m_mediaButtons1 = nullptr;
    QFutureWatcher<void> m_watcher;
};

#endif // MAINWINDOW_H
