#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QList>
#include "canframe.h"

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    bool startServer(quint16 port);
    void stopServer();
    void sendFrame(const CANFrame& frame);
    void addPeriodicFrame(const CANFrame& frame, int intervalMs);
    void clearPeriodicFrames();
    bool isListening() const { return server->isListening(); }

signals:
    void clientConnected(const QString& address);
    void clientDisconnected(const QString& address);
    void frameSent(const CANFrame& frame);
    void errorOccurred(const QString& error);
    void frameReceived(const CANFrame& frame); // Notify when frame received from client

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onSendPeriodicFrames();
    void onClientReadyRead(); // Handle incoming frames from clients

private:
    QTcpServer* server;
    QList<QTcpSocket*> clients;
    QTimer* periodicTimer;

    struct PeriodicFrame {
        CANFrame frame;
        int interval;
        qint64 lastSent;
    };
    QList<PeriodicFrame> periodicFrames;
    QHash<QTcpSocket*, QByteArray> clientBuffers; // Buffer per client for partial frames
};

#endif // TCPSERVER_H
