//main.cpp client_a_1_1
#include <client.h>
#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Client client(QUrl("ws://localhost:1234"), true);

    return a.exec();
}
