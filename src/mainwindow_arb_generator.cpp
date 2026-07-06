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

void MainWindow::setgenClicked() {
    gpibDevice->setGenerator(EMC.GEN_addr, ui->genFreqSpinBox->value(), ui->genAmpSpinBox->value(), ui->genOffsetSpinBox->value(), ui->comboBox->currentData().toInt());
}
