/**
 * @file tcpserver.cpp
 * @brief Implementation of the TcpServer class.
 *
 * Manages TCP client connections, broadcasts CAN frames, handles incoming
 * frames from clients (gateway relay), and drives periodic frame transmission.
 */

#include "tcpserver.h"
#include <QDataStream>
#include <QDateTime>

// ============================================================================
// Constructor / Destructor
// ============================================================================

/**
 * Initialize the TCP server and the periodic transmission timer.
 * The timer ticks every 10ms to check if any periodic frame is due for sending.
 */
TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
    , server(new QTcpServer(this))
    , periodicTimer(new QTimer(this))
{
    connect(server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
    connect(periodicTimer, &QTimer::timeout, this, &TcpServer::onSendPeriodicFrames);
    periodicTimer->setInterval(10);  // 10ms resolution for periodic checks
}

/**
 * Destructor: safely tears down all connections.
 * Signals are disconnected first to prevent callbacks during destruction.
 */
TcpServer::~TcpServer()
{
    // Disconnect signals to prevent callbacks during teardown
    server->disconnect();
    periodicTimer->stop();

    // Disconnect and schedule deletion for each client socket
    for (auto* client : clients) {
        client->disconnect();
        client->disconnectFromHost();
        client->deleteLater();
    }
    clients.clear();
    clientBuffers.clear();

    server->close();
}

// ============================================================================
// Server Lifecycle
// ============================================================================

/**
 * Begin listening on all network interfaces (0.0.0.0) at the specified port.
 * Emits errorOccurred() if the port is unavailable.
 */
bool TcpServer::startServer(quint16 port)
{
    if (!server->listen(QHostAddress::Any, port)) {
        emit errorOccurred(server->errorString());
        return false;
    }
    return true;
}

/**
 * Stop the server: halt periodic transmissions, disconnect all clients,
 * and close the listening socket.
 */
void TcpServer::stopServer()
{
    periodicTimer->stop();

    for (auto* client : std::as_const(clients)) {
        client->disconnectFromHost();
        client->deleteLater();
    }
    clients.clear();
    server->close();
}

// ============================================================================
// Frame Transmission
// ============================================================================

/**
 * Broadcast a 3-byte settings packet to every connected client.
 * Packet layout: [0xFF: 1B][CanType: 1B][IdFormat: 1B]
 * The 0xFF magic byte is distinct from the CanType values (0-2) that begin
 * a normal CAN frame header, so clients can distinguish the two packet types.
 */
void TcpServer::sendSettings(CanType type, IdFormat fmt)
{
    QByteArray packet;
    packet.append(static_cast<char>(0xFF));
    packet.append(static_cast<char>(type));
    packet.append(static_cast<char>(fmt));

    for (auto* c : std::as_const(clients)) {
        if (c->state() == QAbstractSocket::ConnectedState) {
            c->write(packet);
            c->flush();
        }
    }
}

/**
 * Broadcast a single CAN frame to every connected client.
 * The frame is serialized once and written to each socket.
 * Emits frameSent() on success, errorOccurred() if the server is not running.
 */
void TcpServer::sendFrame(const CANFrame& frame)
{
    if (!server->isListening()) {
        emit errorOccurred("Server not running - start server first");
        return;
    }

    QByteArray data = frame.serialize();

    for (auto* client : std::as_const(clients)) {
        if (client->state() == QAbstractSocket::ConnectedState) {
            client->write(data);
            client->flush();
        }
    }

    emit frameSent(frame);
}

/**
 * Add a frame to the periodic transmission schedule.
 * The frame will be sent at the specified interval. Setting lastSent to 0
 * ensures the first transmission happens immediately on the next timer tick.
 */
void TcpServer::addPeriodicFrame(const CANFrame& frame, int intervalMs)
{
    PeriodicFrame pf;
    pf.frame = frame;
    pf.interval = intervalMs;
    pf.lastSent = 0;  // Triggers immediate first transmission
    periodicFrames.append(pf);

    if (!periodicTimer->isActive()) {
        periodicTimer->start();
    }
}

/**
 * Remove a single periodic frame by CAN ID.
 *
 * Iterates the periodic frame list and erases the first entry whose frame ID
 * matches @p canId. Using an index-based loop (rather than a range-for) allows
 * safe in-place removal without invalidating the iterator.
 *
 * If the list is empty after removal, the timer is stopped so it no longer
 * wakes the CPU every 10 ms when there is nothing left to send.
 */
void TcpServer::removePeriodicFrame(uint32_t canId)
{
    for (int i = 0; i < periodicFrames.size(); ++i) {
        if (periodicFrames[i].frame.getId() == canId) {
            periodicFrames.removeAt(i);
            break;  // CAN IDs are unique in the list — stop after first match
        }
    }

    // Stop the timer only when no periodic frames remain
    if (periodicFrames.isEmpty()) {
        periodicTimer->stop();
    }
}

/**
 * Remove all periodic frames and stop the periodic timer.
 */
void TcpServer::clearPeriodicFrames()
{
    periodicFrames.clear();
    periodicTimer->stop();
}

// ============================================================================
// Connection Event Handlers
// ============================================================================

/**
 * Called when a new client connects to the server.
 * Sets up signal connections for disconnection and data reception,
 * adds the client to the list, and notifies the UI.
 */
void TcpServer::onNewConnection()
{
    QTcpSocket* client = server->nextPendingConnection();

    connect(client, &QTcpSocket::disconnected, this, &TcpServer::onClientDisconnected);
    connect(client, &QTcpSocket::readyRead, this, &TcpServer::onClientReadyRead);

    clients.append(client);

    emit clientConnected(client->peerAddress().toString());
}

/**
 * Called when a client socket disconnects.
 * Removes the client from the list, cleans up its receive buffer,
 * and schedules the socket for deletion.
 */
void TcpServer::onClientDisconnected()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (client) {
        emit clientDisconnected(client->peerAddress().toString());
        clients.removeOne(client);
        clientBuffers.remove(client);
        client->deleteLater();
    }
}

// ============================================================================
// Periodic Transmission
// ============================================================================

/**
 * Timer callback (every 10ms): iterate through all periodic frames and
 * transmit any whose interval has elapsed since their last send time.
 * Each frame's timestamp is updated to the current time before sending.
 */
void TcpServer::onSendPeriodicFrames()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    for (auto& pf : periodicFrames) {
        if (now - pf.lastSent >= pf.interval) {
            pf.frame.setTimestamp(now);
            sendFrame(pf.frame);
            pf.lastSent = now;
        }
    }
}

// ============================================================================
// Client Data Reception (Gateway Mode)
// ============================================================================

/**
 * Called when data arrives from a connected client.
 *
 * Incoming bytes are appended to the client's receive buffer. Two-phase
 * parsing extracts complete frames:
 *  Phase 1: wait for 18-byte header, then read DataLen from it.
 *  Phase 2: wait for 18 + DataLen total bytes, then deserialize.
 *
 * Complete frames are relayed to all OTHER clients (gateway behavior)
 * and emitted via frameReceived() for the UI.
 */
void TcpServer::onClientReadyRead()
{
    QTcpSocket* client = qobject_cast<QTcpSocket*>(sender());
    if (!client) return;

    clientBuffers[client].append(client->readAll());

    static constexpr int HEADER_SIZE = 18;
    QByteArray& buf = clientBuffers[client];

    while (true) {
        // Phase 1: need at least the 18-byte header
        if (buf.size() < HEADER_SIZE)
            break;

        // Peek at DataLen (bytes 8-9 in the header, after ct+idf+id+dlc = 8 bytes)
        QDataStream peek(buf);
        peek.setVersion(QDataStream::Qt_6_0);
        quint8  ct, idf;
        quint32 peekId;
        quint16 peekDlc, dataLen;
        peek >> ct >> idf >> peekId >> peekDlc >> dataLen;

        // Phase 2: need full header + data
        int totalSize = HEADER_SIZE + dataLen;
        if (buf.size() < totalSize)
            break;

        QByteArray frameData = buf.left(totalSize);
        CANFrame frame = CANFrame::deserialize(frameData);
        buf.remove(0, totalSize);

        // Relay to all OTHER connected clients (gateway behavior)
        QByteArray serialized = frame.serialize();
        for (auto* c : std::as_const(clients)) {
            if (c != client && c->state() == QAbstractSocket::ConnectedState) {
                c->write(serialized);
                c->flush();
            }
        }

        emit frameReceived(frame);
    }
}
