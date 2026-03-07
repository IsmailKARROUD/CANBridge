/**
 * @file tcpclient.cpp
 * @brief Implementation of the TcpClient class.
 *
 * Handles TCP connection lifecycle, frame transmission to the server,
 * and reception/parsing of incoming CAN frames from the server.
 */

#include "tcpclient.h"
#include <QDataStream>
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
    periodicTimer->setInterval(MIN_INTERVAL);  // MIN_INTERVAL ms resolution for periodic checks
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
 * Attempt to extract one packet from the receive buffer.
 *
 * Two packet types are supported, distinguished by the first byte:
 *
 *  0xFF  — Settings packet (3 bytes): [0xFF][CanType][IdFormat]
 *          Emits settingsReceived() and returns true.
 *
 *  0x00-0x02 — CAN frame, two-phase:
 *          Phase 1: read 18-byte header, extract DataLen.
 *          Phase 2: wait for 18 + DataLen total bytes, deserialize,
 *                   emit frameReceived() and return true.
 *
 * @return true if a packet was consumed, false if more data is needed.
 */
bool TcpClient::parseFrame()
{
    if (buffer.isEmpty())
        return false;

    // --- Settings packet: first byte is 0xFF ---
    if (static_cast<quint8>(buffer[0]) == 0xFF) {
        if (buffer.size() < 3)
            return false;  // Need all 3 bytes

        CanType  ct  = static_cast<CanType> (static_cast<quint8>(buffer[1]));
        IdFormat idf = static_cast<IdFormat>(static_cast<quint8>(buffer[2]));
        buffer.remove(0, 3);
        emit settingsReceived(ct, idf);
        return true;
    }

    // --- CAN frame: two-phase parsing ---
    static constexpr int HEADER_SIZE = 18;
    if (buffer.size() < HEADER_SIZE)
        return false;

    // Peek at DataLen (offset 8: after ct+idf+id+dlc = 8 bytes)
    QDataStream peek(buffer);
    peek.setVersion(QDataStream::Qt_6_0);
    quint8  ct, idf;
    quint32 peekId;
    quint16 peekDlc, dataLen;
    peek >> ct >> idf >> peekId >> peekDlc >> dataLen;

    int totalSize = HEADER_SIZE + dataLen;
    if (buffer.size() < totalSize)
        return false;

    QByteArray frameData = buffer.left(totalSize);
    CANFrame frame = CANFrame::deserialize(frameData);
    buffer.remove(0, totalSize);

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

void TcpClient::setTimerInterval(int ms) {
    if (ms < MIN_INTERVAL) {
        qWarning() << "Timer interval too low, clamping to "<< MIN_INTERVAL<<"ms (was" << ms << "ms)";
        return;
    }
    periodicTimer->setInterval(ms);
}

/**
 * Timer callback : iterate through all periodic frames and
 * transmit any whose interval has elapsed since their last send time.
 * Each frame's timestamp is updated to the current time before sending.
 */
void TcpClient::onSendPeriodicFrames()
{
    if (!isConnected()) return;

    for (auto& pf : periodicFrames) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();

        if (now - pf.lastSent >= pf.interval) {
            pf.frame.setTimestamp(now);
            sendFrame(pf.frame);
            pf.lastSent = now;
        }
    }
}
