#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QLabel label("Duck Plague: Controller skeleton running");
    label.show();

    return app.exec();
}