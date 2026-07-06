#include "mainwindow.h"
#include <QFile>
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QApplication::setStyle("Fusion");

    QFile f(":/darkorangestyle.qss");
    if (f.open(QFile::ReadOnly))
    {
        QString style = f.readAll();
        qApp->setStyleSheet(style);
        //qDebug() << "style size:" << style.size();
        f.close();        
    }

    MainWindow w;
    w.show();    
    return app.exec();
}
