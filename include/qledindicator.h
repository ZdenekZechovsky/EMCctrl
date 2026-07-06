#ifndef QLEDINDICATOR_H
#define QLEDINDICATOR_H

#include <QWidget>
#include <QColor>
#include <QTimer>

class QLedIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit QLedIndicator(QWidget *parent = nullptr);

    void setOn(bool on);
    void setColor(const QColor &color);
    void setBlink(bool enable, int period_ms = 500);

private slots:
    void blinkTimeout();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_on;
    bool m_blink;
    QColor m_color;
    QTimer m_timer;
};

#endif
