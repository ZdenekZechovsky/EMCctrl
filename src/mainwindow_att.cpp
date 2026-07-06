#include "mainwindow.h"
#include "ui_mainwindow.h"

// Standard and Qt includes
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QDir>

#include "GpibDevice.h"
#include "EmcMeasurementManager.h"

void MainWindow::onAttenuatorValueChanged(int value) {
    ui->attenuatorLabel->setText(QString("Attenuate: %1dB").arg(value));
}

void MainWindow::onAttenuatorReleased() {
    gpibDevice->setRfStepAttenuator(1, ui->rfstepattenuator->value());
}
