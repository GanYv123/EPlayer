#include "EPlayerClient.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    EPlayerClient w;
    w.show();
    return a.exec();  // NOLINT(readability-static-accessed-through-instance)
}
