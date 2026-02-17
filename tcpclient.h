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
#include "canframe.h"

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

private:
    QTcpSocket* socket;     ///< The TCP socket used for server communication
    QByteArray buffer;      ///< Receive buffer for accumulating partial frame data

    /**
     * @brief Try to extract one complete 21-byte CAN frame from the buffer.
     * @return true if a frame was parsed (and frameReceived() was emitted), false otherwise.
     */
    bool parseFrame();
};

#endif // TCPCLIENT_H
