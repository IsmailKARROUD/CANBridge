/**
 * @file tcpserver.h
 * @brief Definition of the TcpServer class for CAN frame broadcasting over TCP.
 *
 * TcpServer listens for incoming TCP connections and acts as a CAN frame
 * gateway: it can broadcast frames to all connected clients, receive frames
 * from clients, and forward them to all other clients (gateway/bridge mode).
 *
 * It also supports periodic frame transmission at configurable intervals,
 * useful for simulating repetitive CAN bus traffic.
 */

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QList>
#include "CANFrame.h"

/**
 * @class TcpServer
 * @brief TCP server that broadcasts and relays CAN frames to connected clients.
 *
 * Responsibilities:
 *  - Listen on a configurable TCP port for client connections
 *  - Broadcast CAN frames to all connected clients (one-shot or periodic)
 *  - Receive frames from clients and relay them to all other clients (gateway)
 *  - Manage per-client receive buffers for handling partial frame data
 */
class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    /**
     * @brief Start listening for connections on the given port.
     * @param port TCP port number (typically 1024-65535).
     * @return true if the server started successfully, false otherwise.
     */
    bool startServer(quint16 port);

    /// Stop the server, disconnect all clients, and halt periodic transmissions.
    void stopServer();

    /**
     * @brief Broadcast a single CAN frame to all connected clients.
     * @param frame The CAN frame to send.
     */
    void sendFrame(const CANFrame& frame);

    /**
     * @brief Schedule a frame for periodic transmission.
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

    /// Check if the server is currently listening for connections.
    bool isListening() const { return server->isListening(); }

    /// Change the periodic transmission timer tick interval (milliseconds).
    void setTimerInterval(int ms) { periodicTimer->setInterval(ms); }

    /**
     * @brief Broadcast a 3-byte settings packet [0xFF][CanType][IdFormat] to all
     *        connected clients so they can adapt their UI to the server's bus mode.
     */
    void sendSettings(CanType type, IdFormat fmt);

signals:
    /// Emitted when a new client connects. Provides the client's IP address.
    void clientConnected(const QString& address);

    /// Emitted when a client disconnects. Provides the client's IP address.
    void clientDisconnected(const QString& address);

    /// Emitted after a frame is successfully broadcast to clients.
    void frameSent(const CANFrame& frame);

    /// Emitted when a network error occurs (e.g., port already in use).
    void errorOccurred(const QString& error);

    /// Emitted when a frame is received from a connected client.
    void frameReceived(const CANFrame& frame);

private slots:
    /// Handle a new incoming client connection from the QTcpServer.
    void onNewConnection();

    /// Handle a client socket disconnection event.
    void onClientDisconnected();

    /// Timer callback: check and send any periodic frames whose interval has elapsed.
    void onSendPeriodicFrames();

    /// Handle incoming data from a client socket (frame reception).
    void onClientReadyRead();

private:
    QTcpServer* server;             ///< Underlying Qt TCP server
    QList<QTcpSocket*> clients;     ///< List of currently connected client sockets
    QTimer* periodicTimer;          ///< Timer that drives periodic frame transmission (10ms tick)

    /**
     * @brief Describes a frame scheduled for periodic transmission.
     */
    struct PeriodicFrame {
        CANFrame frame;     ///< The frame to send
        int interval;       ///< Transmission interval in milliseconds
        qint64 lastSent;    ///< Timestamp of last transmission (ms since epoch)
    };
    QList<PeriodicFrame> periodicFrames;            ///< All scheduled periodic frames
    QHash<QTcpSocket*, QByteArray> clientBuffers;   ///< Per-client receive buffer for partial frames
};

#endif // TCPSERVER_H
