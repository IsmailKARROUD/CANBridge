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
#include <QComboBox>
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
    // Connection tab - Server
    void onStartServer();
    void onStopServer();

    // Connection tab - Client
    void onConnect();
    void onDisconnect();

    // Simulator tab
    void onSendFrame();
    void onSendPeriodic();
    void onStopPeriodic();

    // Analyzer tab
    void onClearMessages();
    void onSaveFrames();
    void onLoadFrames();

    // Log tab
    void onLogFilterChanged(int index);

    // Status updates
    void onServerClientConnected(const QString& address);
    void onClientConnected();
    void onClientDisconnected();


private:
    void setupConnectionTab();
    void setupLogTab();
    void setupSimulatorTab();
    void setupAnalyzerTab();

    void addLogEvent(const QString& message, const QString& category);
    void updateLogDisplay();

    //Theme
    void setupMenuBar();
    void applyTheme(bool isDark);

    // Tabs
    QTabWidget* tabWidget;

    // Connection tab - Server widgets
    QSpinBox* serverPortSpin;
    QPushButton* startServerBtn;
    QPushButton* stopServerBtn;
    QLabel* serverStatusIndicator;

    // Connection tab - Client widgets
    QLineEdit* hostEdit;
    QSpinBox* clientPortSpin;
    QPushButton* connectBtn;
    QPushButton* disconnectBtn;
    QLabel* clientStatusIndicator;

    // Log tab widgets
    QComboBox* logFilterCombo;
    QTextEdit* logDisplay;

    // Simulator tab widgets
    QLineEdit* canIdEdit;
    QSpinBox* dlcSpin;
    QLineEdit* dataEdit;
    QPushButton* sendOnceBtn;
    QSpinBox* intervalSpin;
    QPushButton* sendPeriodicBtn;
    QPushButton* stopPeriodicBtn;
    QLabel* lastFrameStatusLabel;

    // Analyzer tab widgets
    QTableView* messageTable;
    QPushButton* clearBtn;
    QPushButton* saveFramesBtn;
    QPushButton* loadFramesBtn;
    QLabel* messageCountLabel;

    // Backend
    TcpServer* server;
    TcpClient* client;
    MessageModel* messageModel;

    // Log management
    struct LogEntry {
        QString timestamp;
        QString category;  // "Server", "Client", "Frame"
        QString message;
    };
    QList<LogEntry> logHistory;
    QString currentLogFilter;  // "All", "Server", "Client", "Frame"

    //Theme
    QAction* darkModeAction;
    bool isDarkMode;
};

#endif // MAINWINDOW_H
