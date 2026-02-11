#include "tcpclient.h"

// Constructor: Initialize TCP socket
TcpClient::TcpClient(QObject *parent) : QObject(parent) , socket(new QTcpSocket(this))
{
    // Connect socket signals to handlers
    connect(socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, &TcpClient::onError);
}

// Destructor: Clean up socket
TcpClient::~TcpClient()
{
    disconnectFromServer();
}

// Initiate connection to server
void TcpClient::connectToServer(const QString& host, quint16 port)
{
    socket->connectToHost(host, port);
}

// Disconnect from server
void TcpClient::disconnectFromServer()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->disconnectFromHost();
    }
}

// Check if currently connected
bool TcpClient::isConnected() const
{
    return socket->state() == QAbstractSocket::ConnectedState;
}

// Handle successful connection
void TcpClient::onConnected()
{
    buffer.clear();  // Clear any residual data
    emit connected();
}

// Handle disconnection
void TcpClient::onDisconnected()
{
    buffer.clear();
    emit disconnected();
}

// Handle incoming data from server
void TcpClient::onReadyRead()
{
    // Append new data to buffer
    buffer.append(socket->readAll());

    // Try to parse complete frames from buffer
    while (parseFrame()) {
        // parseFrame() emits frameReceived and removes parsed data
    }
}

// Handle socket errors
void TcpClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit errorOccurred(socket->errorString());
}

// Extract and parse a complete CANFrame from buffer
bool TcpClient::parseFrame()
{
    // Expected frame size in bytes
    // CANFrame: 4 (id) + 1 (dlc) + 8 (data) + 8 (timestamp) = 21 bytes
    const int frameSize = 21;

    // Not enough data yet for a complete frame
    if (buffer.size() < frameSize) {
        return false;
    }

    // Extract exactly one frame's worth of data
    QByteArray frameData = buffer.left(frameSize);

    // Deserialize into CANFrame structure
    CANFrame frame = CANFrame::deserialize(frameData);

    // Remove parsed data from buffer
    buffer.remove(0, frameSize);

    // Notify listeners
    emit frameReceived(frame);

    return true;  // Successfully parsed a frame
}

void TcpClient::sendFrame(const CANFrame& frame)
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = frame.serialize();
        socket->write(data);
        socket->flush();
    }
}
