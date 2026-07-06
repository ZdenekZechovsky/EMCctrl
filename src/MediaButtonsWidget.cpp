#include "MediaButtonsWidget.h"
#include <QStyle>

MediaButtonsWidget::MediaButtonsWidget(QWidget *parent)
    : QWidget(parent)
{
    // Layout
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(10);      // mezera mezi tlačítky
    layout->setContentsMargins(0,0,0,0);

    // Play button
    btnPlay = new QPushButton(this);
    btnPlay->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    btnPlay->setFlat(true);      // jen ikona bez rámečku
    layout->addWidget(btnPlay);
    connect(btnPlay, &QPushButton::clicked, this, &MediaButtonsWidget::playClicked);

    // Pause button
    btnPause = new QPushButton(this);
    btnPause->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    btnPause->setFlat(true);
    layout->addWidget(btnPause);
    connect(btnPause, &QPushButton::clicked, this, &MediaButtonsWidget::pauseClicked);

    // Stop button
    btnStop = new QPushButton(this);
    btnStop->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    btnStop->setFlat(true);
    layout->addWidget(btnStop);
    connect(btnStop, &QPushButton::clicked, this, &MediaButtonsWidget::stopClicked);
}
