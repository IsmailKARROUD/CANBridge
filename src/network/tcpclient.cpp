/**
 * @file tcpclient.cpp
 * @brief Implementation of the TcpClient class.
 *
 * Handles TCP connection lifecycle, frame transmission to the server,
 * and reception/parsing of incoming CAN frames from the server.
 */

#include "tcpclient.h"
#include <QDateTime>

// ============================================================================
// Constructor / Destructor
// ============================================================================

/**
 * Initialize the TCP socket and connect all Qt signal/slot pairs
 * for connection, disconnection, data arrival, and error handling.
 */
TcpClient::TcpClient(QObject *parent)
    : QObject(parent)
    , socket(new QTcpSocket(this))
    , periodicTimer(new QTimer(this))
{
    connect(socket, &QTcpSocket::connected, this, &TcpClient::onConnected);
    connect(socket, &QTcpSocket::disconnected, this, &TcpClient::onDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &TcpClient::onReadyRead);
    connect(socket, &QTcpSocket::errorOccurred, this, &TcpClient::onError);
    connect(periodicTimer, &QTimer::timeout, this, &TcpClient::onSendPeriodicFrames);
    periodicTimer->setInterval(10);  // 10ms resolution for periodic checks
}

/**
 * Destructor: disconnect all signals first to prevent callbacks during
 * destruction, then close the connection.
 */
TcpClient::~TcpClient()
{
    socket->disconnect();
    periodicTimer->stop();
    disconnectFromServer();
}

// ============================================================================
// Connection Management
// ============================================================================

/**
 * Initiate an asynchronous TCP connection to the given host and port.
 * The connected() signal will be emitted when the connection succeeds.
 */
void TcpClient::connectToServer(const QString& host, quint16 port)
{
    socket->connectToHost(host, port);
}

/**
 * Disconnect from the server if currently in the connected state.
 */
void TcpClient::disconnectFromServer()
{
    if (socket->state() == QAbstractSocket::ConnectedState) {
        socket->disconnectFromHost();
    }
}

/**
 * @return true if the socket is currently in the ConnectedState.
 */
bool TcpClient::isConnected() const
{
    return socket->state() == QAbstractSocket::ConnectedState;
}

// ============================================================================
// Connection Event Handlers
// ============================================================================

/**
 * Called when the TCP connection is established.
 * Clears any residual data in the receive buffer.
 */
void TcpClient::onConnected()
{
    buffer.clear();
    emit connected();
}

/**
 * Called when the TCP connection is closed.
 * Clears the receive buffer and stops periodic transmissions.
 */
void TcpClient::onDisconnected()
{
    buffer.clear();
    clearPeriodicFrames();
    emit disconnected();
}

/**
 * Called when new data arrives from the server.
 * Appends incoming bytes to the buffer and attempts to extract
 * complete CAN frames in a loop until no more full frames remain.
 */
void TcpClient::onReadyRead()
{
    buffer.append(socket->readAll());

    // Extract all complete frames from the buffer
    while (parseFrame()) {
        // parseFrame() emits frameReceived() for each complete frame
    }
}

/**
 * Called on socket errors (connection refused, host not found, etc.).
 * Forwards the human-readable error string via errorOccurred().
 */
void TcpClient::onError(QAbstractSocket::SocketError socketError)
{
    Q_UNUSED(socketError);
    emit errorOccurred(socket->errorString());
}

// ============================================================================
// Frame Parsing
// ============================================================================

/**
 * Attempt to extract a single 21-byte CAN frame from the receive buffer.
 *
 * Frame size breakdown:
 *   4 bytes (ID) + 1 byte (DLC) + 8 bytes (data) + 8 bytes (timestamp) = 21 bytes
 *
 * @return true if a complete frame was parsed and emitted, false if
 *         the buffer does not yet contain a full frame.
 */
bool TcpClient::parseFrame()
{
    const int frameSize = 21;

    if (buffer.size() < frameSize) {
        return false;  // Not enough data for a complete frame
    }

    // Extract exactly one frame from the front of the buffer
    QByteArray frameData = buffer.left(frameSize);
    CANFrame frame = CANFrame::deserialize(frameData);
    buffer.remove(0, frameSize);

    emit frameReceived(frame);

    return true;
}

// ============================================================================
// Frame Transmission
// ============================================================================

/**
 * Serialize and send a CAN frame to the connected server.
 * Emits frameSent() on success, errorOccurred() if not connected.
 */
void TcpClient::sendFrame(const CANFrame& frame)
{
    if (socket->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred("Client is not connected - connect to server first");
        return;
    }

    QByteArray data = frame.serialize();
    socket->write(data);
    socket->flush();

    emit frameSent(frame);
}

// ============================================================================
// Periodic Transmission
// ============================================================================

/**
 * Add a frame to the periodic transmission schedule.
 * The frame will be sent at the specified interval. Setting lastSent to 0
 * ensures the first transmission happens immediately on the next timer tick.
 */
void TcpClient::addPeriodicFrame(const CANFrame& frame, int intervalMs)
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
void TcpClient::removePeriodicFrame(uint32_t canId)
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
void TcpClient::clearPeriodicFrames()
{
    periodicFrames.clear();
    periodicTimer->stop();
}

/**
 * Timer callback (every 10ms): iterate through all periodic frames and
 * transmit any whose interval has elapsed since their last send time.
 * Each frame's timestamp is updated to the current time before sending.
 */
void TcpClient::onSendPeriodicFrames()
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
