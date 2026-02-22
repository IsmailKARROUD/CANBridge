#ifndef FRAMEWIDGET_H
#define FRAMEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <functional>
#include "../core/canframe.h"

/**
 * @class FrameWidget
 * @brief Widget representing a single configurable CAN frame in the Simulator tab.
 *
 * Provides controls for:
 *  - CAN ID (hex input)
 *  - DLC (0-8)
 *  - Data bytes (auto-formatted hex)
 *  - Periodic transmission checkbox with interval
 *  - Send Once, Hide, and Remove buttons
 */
class FrameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FrameWidget(uint32_t defaultId = 0x100, QWidget *parent = nullptr);

    // Getters
    CANFrame getFrame() const;
    uint32_t getCanId() const;
    bool isPeriodicEnabled() const;
    int getPeriodicInterval() const;

    // Setters
    void setFrame(const CANFrame& frame);
    void setPeriodicEnabled(bool enabled);

    /**
     * @brief Provide a live connection-state checker to this widget.
     *
     * The supplied callable is invoked each time the user clicks Send to
     * determine whether a server is listening or a client is connected.
     * Using a std::function keeps FrameWidget fully decoupled from
     * TcpServer and TcpClient.
     *
     * @param checker  A callable returning true when a connection is active.
     *
     * Example (MainWindow):
     * @code
     *   widget->setConnectionChecker([this]() {
     *       return server->isListening() || client->isConnected();
     *   });
     * @endcode
     */
    void setConnectionChecker(std::function<bool()> checker);

    /**
     * @brief Notify the widget whether a server or client connection is active.
     *
     * Called by MainWindow whenever the connection state changes (server
     * start/stop, client connect/disconnect). onSendOnce() uses this flag
     * to block transmission attempts when no connection is available.
     *
     * @param connected true if server is listening or client is connected.
     */
    void setConnected(bool connected);

signals:
    void sendOnceClicked(const CANFrame& frame);
    void onSendFramePeriodicClicked(const CANFrame& frame, int intervalMs);
    void onStopFramePeriodicClicked(const int& frame);
    void periodicStateChanged(uint32_t canId, bool enabled, int intervalMs);
    void removeClicked(uint32_t canId);
    void hideClicked();
    void canIdChanged(uint32_t oldId, uint32_t newId);

private slots:
    void onSend();
    void onPeriodicToggled();
    void onStopFramePeriodic();
    void onRemove();
    void onHide();
    void onCanIdChanged();
    void onDataChanged(const QString& text);

private:
    /**
     * @brief Check whether a connection is currently active.
     *
     * Invokes m_connectionChecker if one has been set. On failure, updates
     * statusLabel with an error message and returns false so the caller
     * can bail out immediately.
     *
     * @return true if connected, false otherwise.
     */
    bool checkConnection();

    QString formatHexWithSpaces(const QString& input);

    QLineEdit* canIdEdit;
    QSpinBox* dlcSpin;
    QLineEdit* dataEdit;
    QCheckBox* periodicCheckBox;
    QSpinBox* intervalSpin;
    QPushButton* sendBtn;
    QPushButton* stopBtn;
    QPushButton* hideBtn;
    QPushButton* removeBtn;
    QLabel* statusLabel;

    uint32_t currentCanId;

    /// Live connection checker injected by MainWindow via setConnectionChecker().
    /// Returns true when a server is listening or the client is connected.
    /// Null until setConnectionChecker() is called.
    std::function<bool()> m_connectionChecker;
};

#endif // FRAMEWIDGET_H
