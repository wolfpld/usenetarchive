#include <QApplication>

#include "browser.h"

int main( int argc, char *argv[] )
{
    QApplication a( argc, argv );
    Browser w;
    w.show();

    return a.exec();
}
