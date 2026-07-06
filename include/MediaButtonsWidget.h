#pragma once
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

class MediaButtonsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit MediaButtonsWidget(QWidget *parent = nullptr);

signals:
    void playClicked();
    void pauseClicked();
    void stopClicked();

private:
    QPushButton *btnPlay;
    QPushButton *btnPause;
    QPushButton *btnStop;
};
