#include "qledindicator.h"
#include <QPainter>

QLedIndicator::QLedIndicator(QWidget *parent)
    : QWidget(parent), m_on(false), m_color(Qt::green)
{
    setMinimumSize(16,16);
    connect(&m_timer, &QTimer::timeout, this, &QLedIndicator::blinkTimeout);
}

void QLedIndicator::setOn(bool on)
{
    m_on = on;
    update();
}

void QLedIndicator::setColor(const QColor &color)
{
    m_color = color;
    update();
}

void QLedIndicator::setBlink(bool enable, int period_ms)
{
    m_blink = enable;

    if(enable)
        m_timer.start(period_ms);
    else
        m_timer.stop();
}

void QLedIndicator::blinkTimeout()
{
    m_on = !m_on;
    update();
}


void QLedIndicator::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int s = qMin(width(), height());
    QRectF rect((width()-s)/2,(height()-s)/2,s,s);

    QColor dark = m_color.darker(300);
    QColor light = m_color.lighter(150);

    if(!m_on)
    {
        dark = QColor(60,60,60);
        light = QColor(120,120,120);
    }

    // hlavní LED
    QRadialGradient g(rect.center(), s/2, rect.center()-QPointF(s/3,s/3));
    g.setColorAt(0, light);
    g.setColorAt(1, dark);

    p.setPen(QPen(Qt::black,1));
    p.setBrush(g);
    p.drawEllipse(rect);

    // glass efekt
    QRectF highlight(rect.x()+rect.width()*0.15,
                     rect.y()+rect.height()*0.1,
                     rect.width()*0.7,
                     rect.height()*0.4);

    QRadialGradient glass(highlight.center(), highlight.width()/2);
    glass.setColorAt(0, QColor(255,255,255,180));
    glass.setColorAt(1, QColor(255,255,255,0));

    p.setPen(Qt::NoPen);
    p.setBrush(glass);
    p.drawEllipse(highlight);
}
