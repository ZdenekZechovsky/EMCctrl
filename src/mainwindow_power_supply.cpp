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

void MainWindow::getpwrClicked() {
    double voltage_p6, voltage_p25, voltage_m25;
    double current_p6, current_p25, current_m25;

    gpibDevice->getPowerVoltage(EMC.PWR_addr, &voltage_p6, &voltage_p25, &voltage_m25);
    ui->label_6->setText(QString("%1 V").arg(voltage_p6, 0, 'f', 3));
    ui->label_8->setText(QString("%1 V").arg(voltage_p25, 0, 'f', 3));
    ui->label_10->setText(QString("%1 V").arg(voltage_m25, 0, 'f', 3));

    gpibDevice->getPowerCurrent(EMC.PWR_addr, &current_p6, &current_p25, &current_m25);
    ui->label_25->setText(QString("%1 A").arg(current_p6, 0, 'f', 3));
    ui->label_26->setText(QString("%1 A").arg(current_p25, 0, 'f', 3));
    ui->label_24->setText(QString("%1 A").arg(current_m25, 0, 'f', 3));

    if(ui->disp1checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 0);
    }
    else if(ui->disp2checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 1);
    }
    else if(ui->disp3checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 2);
    }
}

void MainWindow::setpwrClicked() {
    gpibDevice->setPowerSupply(EMC.PWR_addr,
                               ui->p6VSpinBox->value(),
                               ui->p25VSpinBox->value(),
                               ui->m25VSpinBox->value(),
                               ui->p6ISpinBox->value(),
                               ui->p25ISpinBox->value(),
                               ui->m25ISpinBox->value());

    if(ui->disp1checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 0);
    }
    else if(ui->disp2checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 1);
    }
    else if(ui->disp3checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 2);
    }
}

void MainWindow::onpwrClicked() {
    if(ui->onpwrButton->text() == "On") {
        ui->onpwrButton->setText("Off");
        gpibDevice->enablePowerSupply(EMC.PWR_addr, 1);
    }
    else if(ui->onpwrButton->text() == "Off") {
        ui->onpwrButton->setText("On");
        gpibDevice->enablePowerSupply(EMC.PWR_addr, 0);
    }

    if(ui->disp1checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 0);
    }
    else if(ui->disp2checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 1);
    }
    else if(ui->disp3checkBox->isChecked()) {
        gpibDevice->setDisplay(EMC.PWR_addr, 2);
    }
}

void MainWindow::disp1checkBox_stateChanged(int state)
{
    if(ui->disp1checkBox->isChecked()) {
        ui->disp2checkBox->setChecked(false);
        ui->disp3checkBox->setChecked(false);
    }
    gpibDevice->setDisplay(EMC.PWR_addr, 0);
}

void MainWindow::disp2checkBox_stateChanged(int state)
{

    if(ui->disp2checkBox->isChecked()) {
        ui->disp1checkBox->setChecked(false);
        ui->disp3checkBox->setChecked(false);
    }
    gpibDevice->setDisplay(EMC.PWR_addr, 1);
}


void MainWindow::disp3checkBox_stateChanged(int state)
{
    if(ui->disp3checkBox->isChecked()) {
        ui->disp2checkBox->setChecked(false);
        ui->disp1checkBox->setChecked(false);
    }
    gpibDevice->setDisplay(EMC.PWR_addr, 2);
}

