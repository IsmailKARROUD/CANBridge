#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QTableView>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>

#include "tcpserver.h"
#include "tcpclient.h"
#include "messagemodel.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Simulator slots
    void onStartServer();
    void onStopServer();
    void onSendFrame();
    void onSendPeriodic();
    void onStopPeriodic();

    // Analyzer slots
    void onConnect();
    void onDisconnect();
    void onClearMessages();

    // Status updates
    void onServerClientConnected(const QString& address);
    void onClientConnected();
    void onClientDisconnected();

    // logs
    void onSaveLog();
    void onLoadLog();

private:
    void setupSimulatorTab();
    void setupAnalyzerTab();
    void addServerEvent(const QString& message);

    // Tabs
    QTabWidget* tabWidget;

    // Simulator widgets
    QSpinBox* serverPortSpin;
    QPushButton* startServerBtn;
    QPushButton* stopServerBtn;
    QLineEdit* canIdEdit;
    QSpinBox* dlcSpin;
    QLineEdit* dataEdit;
    QPushButton* sendOnceBtn;
    QSpinBox* intervalSpin;
    QPushButton* sendPeriodicBtn;
    QPushButton* stopPeriodicBtn;
    QLabel* serverStatusLabel;

    // Analyzer widgets
    QLineEdit* hostEdit;
    QSpinBox* clientPortSpin;
    QPushButton* connectBtn;
    QPushButton* disconnectBtn;
    QTableView* messageTable;
    QPushButton* clearBtn;
    QLabel* clientStatusLabel;
    QLabel* messageCountLabel;

    // Backend
    TcpServer* server;
    TcpClient* client;
    MessageModel* messageModel;

    //Logs
    QPushButton* saveLogBtn;
    QPushButton* loadLogBtn;

    QLabel* serverStatusIndicator;
    QTextEdit* serverEventLog;
    QStringList eventLogHistory;  // Keep last 5 events
};

#endif // MAINWINDOW_H
