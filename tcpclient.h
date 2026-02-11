#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include "canframe.h"

class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    bool isConnected() const;
    void sendFrame(const CANFrame& frame);

signals:
    void connected();
    void disconnected();
    void frameReceived(const CANFrame& frame);
    void errorOccurred(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket* socket;
    QByteArray buffer;  // Accumulate incoming data

    bool parseFrame();  // Extract complete frame from buffer
};

#endif // TCPCLIENT_H
