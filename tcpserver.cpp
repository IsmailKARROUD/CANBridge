#include "tcpserver.h"
#include <QDateTime>

// Constructor: Initialize server and timer objects
TcpServer::TcpServer(QObject *parent) : QObject(parent) , server(new QTcpServer(this)) , periodicTimer(new QTimer(this))
{
    // Connect server's newConnection signal to our handler
    connect(server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

    // Connect timer to periodic frame transmission handler
    connect(periodicTimer, &QTimer::timeout, this, &TcpServer::onSendPeriodicFrames);

    // Check every 10ms for frames needing transmission (high precision)
    periodicTimer->setInterval(10);
}

// Destructor: Clean up resources
TcpServer::~TcpServer()
{
    // Disconnect signals to prevent callbacks during destruction
    server->disconnect();

    // Stop periodic timer
    periodicTimer->stop();

    // Disconnect all clients without emitting signals
    for (auto* client : clients) {
        client->disconnect();  // Disconnect client's signals
        client->disconnectFromHost();
        client->deleteLater();
    }
    clients.clear();
    clientBuffers.clear();

    // Close server
    server->close();
}

// Start listening on specified port
bool TcpServer::startServer(quint16 port)
{
    // Listen on all network interfaces (0.0.0.0)
    if (!server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(server->errorString());
        return false;
    }
    return true;
}

// Stop server and disconnect all clients
void TcpServer::stopServer()
{
    periodicTimer->stop();

    // Gracefully disconnect and clean up each client
    for (auto* client : std::as_const(clients)) {
        client->disconnectFromHost();
        client->deleteLater();  // Schedule for deletion when safe
    }
    clients.clear();
    server->close();
}

// Send a single CAN frame to all connected clients
void TcpServer::sendFrame(const CANFrame& frame)
{
    // Don't send if server not running
    if (!server->isListening()) {
        emit errorOccurred("Server not running - start server first");
        return;
    }

    QByteArray data = frame.serialize();  // Convert to network format

    // Broadcast to all connected clients
    for (auto* client : std::as_const(clients)) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
            client->flush();  // Force immediate transmission
        }
    }

    // Notify UI that frame was sent
    emit frameSent(frame);
}

// Add a frame to periodic transmission list
void TcpServer::addPeriodicFrame(const CANFrame& frame, int intervalMs)
{
    PeriodicFrame pf;
    pf.frame = frame;
    pf.interval = intervalMs;  // Transmission period in milliseconds
    pf.lastSent = 0;           // Force immediate first transmission
    periodicFrames.append(pf);

    // Start timer if not already running
    if (!periodicTimer->isActive()) {
        periodicTimer->start();
    }
}

// Clear all periodic frames and stop timer
void TcpServer::clearPeriodicFrames()
{
    periodicFrames.clear();
    periodicTimer->stop();
}

// Handle new client connection
void TcpServer::onNewConnection()
{
    QTcpSocket* client = server->nextPendingConnection();

    // Connect client's disconnected signal to our handler
    connect(client, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
    connect(client, &QTcpSocket::readyRead, this, &TcpServer::onClientReadyRead);

    clients.append(client);

    // Notify UI with client's IP address
    emit clientConnected(client->peerAddress().toString());
}

// Handle client disconnection
void TcpServer::onClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        emit clientDisconnected(client->peerAddress().toString());
        clients.removeOne(client);
        clientBuffers.remove(client);  // Clean up buffer
        client->deleteLater();
    }
}

// Timer callback: Check and send periodic frames
void TcpServer::onSendPeriodicFrames()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();  // Current time in ms

    // Check each periodic frame
    for (auto& pf : periodicFrames) {
        // Send if interval has elapsed since last transmission
        if (now - pf.lastSent >= pf.interval) {
            pf.frame.setTimestamp(now);  // Update timestamp to current time
            sendFrame(pf.frame);
            pf.lastSent = now;         // Record transmission time
        }
    }
}

// Handle incoming frames from connected clients
void TcpServer::onClientReadyRead()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    // Append new data to this client's buffer
    clientBuffers[client].append(client->readAll());

    // CANFrame size: 4 (id) + 1 (dlc) + 8 (data) + 8 (timestamp) = 21 bytes
    const int frameSize = 21;

    // Parse all complete frames from buffer
    while (clientBuffers[client].size() >= frameSize) {
        // Extract one frame
        QByteArray frameData = clientBuffers[client].left(frameSize);
        CANFrame frame = CANFrame::deserialize(frameData);
        clientBuffers[client].remove(0, frameSize);  // Remove parsed data

        // Broadcast to all OTHER clients (gateway behavior)
        QByteArray data = frame.serialize();
        for (auto* c : std::as_const(clients)) {
            if (c != client && c->state() == QAbstractSocket::ConnectedState) {
                c->write(data);
                c->flush();
            }
        }

        // Notify UI that frame was received from client
        emit frameReceived(frame);
    }
}
