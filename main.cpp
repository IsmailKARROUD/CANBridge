/**
 * @file main.cpp
 * @brief Application entry point for CANBridge.
 *
 * Creates the QApplication instance and shows the main window.
 * The event loop runs until the window is closed.
 */

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow window;
    window.show();
    return a.exec();  // Enter the Qt event loop
}
