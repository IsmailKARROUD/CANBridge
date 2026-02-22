/**
 * @file tcpclient.h
 * @brief Definition of the TcpClient class for connecting to a CAN bridge server.
 *
 * TcpClient establishes a TCP connection to a remote TcpServer instance,
 * sends CAN frames to the server, and receives frames broadcast by the server.
 * Incoming data is buffered and parsed into complete 21-byte CAN frames.
 */

#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QList>
#include "CANFrame.h"

/**
 * @class TcpClient
 * @brief TCP client that connects to a CAN bridge server for frame exchange.
 *
 * Responsibilities:
 *  - Connect/disconnect to a remote TcpServer
 *  - Send CAN frames to the server
 *  - Receive and parse incoming CAN frames from the server
 *  - Handle partial frame buffering (TCP stream reassembly)
 */
class TcpClient : public QObject
{
    Q_OBJECT

public:
    explicit TcpClient(QObject *parent = nullptr);
    ~TcpClient();

    /**
     * @brief Initiate a TCP connection to the specified server.
     * @param host Server hostname or IP address.
     * @param port Server TCP port number.
     */
    void connectToServer(const QString& host, quint16 port);

    /// Disconnect from the server if currently connected.
    void disconnectFromServer();

    /// Check whether the client is currently connected to a server.
    bool isConnected() const;

    /**
     * @brief Send a CAN frame to the connected server.
     * @param frame The CAN frame to transmit.
     *
     * Emits frameSent() on success, errorOccurred() if not connected.
     */
    void sendFrame(const CANFrame& frame);

    /**
     * @brief Schedule a frame for periodic transmission to the server.
     * @param frame      The CAN frame to send repeatedly.
     * @param intervalMs Transmission interval in milliseconds.
     */
    void addPeriodicFrame(const CANFrame& frame, int intervalMs);

    /**
     * @brief Remove a single periodic frame identified by its CAN ID.
     *
     * Searches the periodic frame list for an entry whose CAN ID matches
     * @p canId and removes it. If no more periodic frames remain after the
     * removal, the periodic timer is stopped to avoid unnecessary CPU wake-ups.
     *
     * @param canId The CAN identifier of the frame to stop transmitting.
     */
    void removePeriodicFrame(uint32_t canId);

    /// Remove all periodic frames and stop the periodic timer.
    void clearPeriodicFrames();

signals:
    /// Emitted when the TCP connection is successfully established.
    void connected();

    /// Emitted when the TCP connection is closed.
    void disconnected();

    /// Emitted when a complete CAN frame is received from the server.
    void frameReceived(const CANFrame& frame);

    /// Emitted when a socket error occurs (connection refused, timeout, etc.).
    void errorOccurred(const QString& error);

    /// Emitted after a frame is successfully written to the server socket.
    void frameSent(const CANFrame& frame);

private slots:
    void onConnected();       ///< Handle successful TCP connection
    void onDisconnected();    ///< Handle TCP disconnection
    void onReadyRead();       ///< Handle incoming data from the server
    void onError(QAbstractSocket::SocketError socketError);  ///< Handle socket errors
    void onSendPeriodicFrames();  ///< Timer callback for periodic frame transmission

private:
    QTcpSocket* socket;     ///< The TCP socket used for server communication
    QByteArray buffer;      ///< Receive buffer for accumulating partial frame data
    QTimer* periodicTimer;  ///< Timer that drives periodic frame transmission (10ms tick)

    /**
     * @brief Describes a frame scheduled for periodic transmission.
     */
    struct PeriodicFrame {
        CANFrame frame;     ///< The frame to send
        int interval;       ///< Transmission interval in milliseconds
        qint64 lastSent;    ///< Timestamp of last transmission (ms since epoch)
    };
    QList<PeriodicFrame> periodicFrames;  ///< All scheduled periodic frames

    /**
     * @brief Try to extract one complete 21-byte CAN frame from the buffer.
     * @return true if a frame was parsed (and frameReceived() was emitted), false otherwise.
     */
    bool parseFrame();
};

#endif // TCPCLIENT_H
