#ifndef FRAMEWIDGET_H
#define FRAMEWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>
#include <functional>
#include "../core/CANFrame.h"

/**
 * @class FrameWidget
 * @brief Widget representing a single configurable CAN frame in the Simulator tab.
 *
 * Supports CAN Classic, CAN-FD, and CAN-XL bus modes as well as Standard
 * (11-bit) and Extended (29-bit) CAN ID formats.
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
     * @brief Enforce a minimum periodic interval matching the timer resolution.
     * @param ms Minimum allowed interval in milliseconds.
     */
    void setMinInterval(int ms);

    /**
     * @brief Enforce a maximum periodic interval from the Settings menu.
     * @param ms Maximum allowed interval in milliseconds. 0 = no upper limit.
     */
    void setMaxInterval(int ms);

    /// Set the current interval spinner value (used during widget initialization).
    void setIntervalValue(int ms);

    /**
     * @brief Configure the widget for the given CAN bus type.
     *
     * - Classic: shows dlcSpin (0-8), hides dlcComboFD, data limit 8 bytes
     * - FD:      hides dlcSpin, shows dlcComboFD {0..8,12,16,20,24,32,48,64}, data limit 64 bytes
     * - XL:      shows dlcSpin (0-2048), hides dlcComboFD, data limit 2048 bytes
     */
    void setCanType(CanType type);

    /**
     * @brief Configure the widget for the given CAN ID format.
     * Updates the maximum CAN ID validated on send.
     */
    void setIdFormat(IdFormat fmt);

    /// Returns the active DLC code (index for FD, value for Classic/XL).
    int getEffectiveDlc() const;

    /// Returns the actual maximum data byte count for the current DLC selection.
    int getEffectiveDataBytes() const;

    /// Serialize this widget's state to a JSON object for project save.
    QJsonObject toJson() const;

    /// Restore this widget's state from a JSON object on project load.
    void fromJson(const QJsonObject& obj);

    /**
     * @brief Provide a live connection-state checker to this widget.
     * @param checker  A callable returning true when a connection is active.
     */
    void setConnectionChecker(std::function<bool()> checker);

    /**
     * @brief Notify the widget whether a connection is active (fallback for
     *        when no connection checker has been injected).
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
    bool checkConnection();
    QString formatHexWithSpaces(const QString& input);

    QLineEdit*   canIdEdit;
    QSpinBox*    dlcSpin;       ///< Classic (0-8) and XL (0-2048) DLC spinner
    QComboBox*   dlcComboFD;    ///< FD-only DLC combo (0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64)
    QLineEdit*   dataEdit;
    QCheckBox*   periodicCheckBox;
    QSpinBox*    intervalSpin;
    QPushButton* sendBtn;
    QPushButton* stopBtn;
    QPushButton* hideBtn;
    QPushButton* removeBtn;
    QLabel*      statusLabel;

    uint32_t currentCanId;
    CanType  m_canType  = CanType::Classic;
    IdFormat m_idFormat = IdFormat::Standard;
    bool     m_connected = false;

    std::function<bool()> m_connectionChecker;

public:
    QPushButton* getSendButton() const { return sendBtn; }
    QPushButton* getStopButton() const { return stopBtn; }
};

#endif // FRAMEWIDGET_H
