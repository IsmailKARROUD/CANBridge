#include "framewidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <limits>

/**
 * @brief Construct a FrameWidget with the given default CAN ID.
 *
 * Builds the entire widget layout in code (no .ui file):
 *  Row 1: ID | DLC (spin or combo) | Data | Periodic checkbox + interval | Send | Stop | Hide | Remove
 *  Row 2: Status label (validation errors, send results, periodic state)
 *
 * The DLC combo (FD mode) and DLC spinner (Classic/XL mode) are created
 * simultaneously; only one is visible at any time, switched by setCanType().
 *
 * All signal connections live at the bottom of this constructor to keep
 * widget creation and wiring clearly separated.
 */
FrameWidget::FrameWidget(uint32_t defaultId, QWidget *parent)
    : QWidget(parent)
    , currentCanId(defaultId)
{
    QGroupBox* frameBox = new QGroupBox(QString("Frame ID: 0x%1").arg(defaultId, 0, 16));
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QHBoxLayout* controlsLayout = new QHBoxLayout();

    // CAN ID input
    controlsLayout->addWidget(new QLabel("ID:"));
    canIdEdit = new QLineEdit(QString("0x%1").arg(defaultId, 0, 16));
    canIdEdit->setMaximumWidth(100);
    controlsLayout->addWidget(canIdEdit);

    // DLC spinner — Classic (0-8) / XL (0-2048)
    controlsLayout->addWidget(new QLabel("DLC:"));
    dlcSpin = new QSpinBox();
    dlcSpin->setRange(0, 8);
    dlcSpin->setValue(8);
    dlcSpin->setMaximumWidth(70);
    controlsLayout->addWidget(dlcSpin);

    // FD DLC combo — shown only in CAN-FD mode, hidden by default
    dlcComboFD = new QComboBox();
    dlcComboFD->setMaximumWidth(70);
    const int fdTable[16] = {0,1,2,3,4,5,6,7,8,12,16,20,24,32,48,64};
    for (int v : fdTable) {
        dlcComboFD->addItem(QString::number(v));
    }
    dlcComboFD->setCurrentIndex(8);  // Default: 8 bytes
    dlcComboFD->setVisible(false);
    controlsLayout->addWidget(dlcComboFD);

    // Data bytes input
    controlsLayout->addWidget(new QLabel("Data:"));
    dataEdit = new QLineEdit("00 00 00 00 00 00 00 00");
    dataEdit->setMinimumWidth(200);
    controlsLayout->addWidget(dataEdit);

    // Periodic checkbox and interval
    periodicCheckBox = new QCheckBox("Periodic");
    controlsLayout->addWidget(periodicCheckBox);

    intervalSpin = new QSpinBox();
    intervalSpin->setSuffix(" ms");
    intervalSpin->setMaximumWidth(100);
    intervalSpin->setEnabled(false);
    controlsLayout->addWidget(intervalSpin);

    // Action buttons
    sendBtn = new QPushButton("Send");
    sendBtn->setMaximumWidth(80);
    sendBtn->setVisible(true);
    sendBtn->setStyleSheet("QPushButton { color: green; font-weight: bold; }");
    controlsLayout->addWidget(sendBtn);

    stopBtn = new QPushButton("Stop");
    stopBtn->setMaximumWidth(80);
    stopBtn->setVisible(false);
    stopBtn->setStyleSheet("QPushButton { color: red; font-weight: bold; }");
    controlsLayout->addWidget(stopBtn);

    hideBtn = new QPushButton("Hide");
    hideBtn->setMaximumWidth(80);
    hideBtn->setStyleSheet("QPushButton { color: orange; font-weight: bold; }");
    controlsLayout->addWidget(hideBtn);
    controlsLayout->addStretch();

    removeBtn = new QPushButton("Remove");
    removeBtn->setMaximumWidth(80);
    removeBtn->setStyleSheet("QPushButton { color: red; font-weight: bold; }");
    controlsLayout->addWidget(removeBtn);

    // Status label
    statusLabel = new QLabel("");
    statusLabel->setStyleSheet("QLabel { color: gray; font-weight: bold; }");

    QVBoxLayout* boxLayout = new QVBoxLayout(frameBox);
    boxLayout->addLayout(controlsLayout);
    boxLayout->addWidget(statusLabel);

    mainLayout->addWidget(frameBox);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Connect signals
    connect(sendBtn,          &QPushButton::clicked,   this, &FrameWidget::onSend);
    connect(stopBtn,          &QPushButton::clicked,   this, &FrameWidget::onStopFramePeriodic);
    connect(hideBtn,          &QPushButton::clicked,   this, &FrameWidget::onHide);
    connect(removeBtn,        &QPushButton::clicked,   this, &FrameWidget::onRemove);
    connect(periodicCheckBox, &QCheckBox::toggled,     this, &FrameWidget::onPeriodicToggled);
    connect(canIdEdit,        &QLineEdit::editingFinished, this, &FrameWidget::onCanIdChanged);
    connect(dataEdit,         &QLineEdit::textChanged, this, &FrameWidget::onDataChanged);
    connect(periodicCheckBox, &QCheckBox::toggled, intervalSpin, &QSpinBox::setEnabled);
    connect(intervalSpin, &QSpinBox::valueChanged, this, [this](int ms) {
        if (periodicCheckBox->isChecked()) {
            int minRes = intervalSpin->minimum();
            if (minRes > 0 && ms == minRes) {
                statusLabel->setText(QString("⚠ Minimum interval is %1 ms (timer resolution)").arg(ms));
                statusLabel->setStyleSheet("QLabel { color: orange; }");
            } else {
                statusLabel->setText(QString("⟳ Periodic active (%1 ms)").arg(ms));
                statusLabel->setStyleSheet("QLabel { color: blue; }");
            }
        }
    });
}

// ============================================================================
// CAN Type / ID Format
// ============================================================================

void FrameWidget::setCanType(CanType type)
{
    m_canType = type;
    switch (type) {
    case CanType::Classic:
        dlcSpin->setRange(0, 8);
        dlcSpin->setVisible(true);
        dlcComboFD->setVisible(false);
        break;
    case CanType::FD:
        dlcSpin->setVisible(false);
        dlcComboFD->setVisible(true);
        break;
    case CanType::XL:
        dlcSpin->setRange(0, 2048);
        dlcSpin->setVisible(true);
        dlcComboFD->setVisible(false);
        break;
    }
}

void FrameWidget::setIdFormat(IdFormat fmt)
{
    m_idFormat = fmt;
}

int FrameWidget::getEffectiveDlc() const
{
    if (m_canType == CanType::FD)
        return dlcComboFD->currentIndex();
    return dlcSpin->value();
}

int FrameWidget::getEffectiveDataBytes() const
{
    if (m_canType == CanType::FD)
        return CANFrame::FD_DLC_TABLE[dlcComboFD->currentIndex()];
    return dlcSpin->value();
}

// ============================================================================
// Getters
// ============================================================================

CANFrame FrameWidget::getFrame() const
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok) id = 0x100;

    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    QByteArray data;
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (ok && byte <= 0xFF)
            data.append(static_cast<char>(byte));
    }

    return CANFrame(id, static_cast<quint16>(getEffectiveDlc()), data, m_canType, m_idFormat);
}

uint32_t FrameWidget::getCanId() const
{
    return currentCanId;
}

bool FrameWidget::isPeriodicEnabled() const
{
    return periodicCheckBox->isChecked();
}

int FrameWidget::getPeriodicInterval() const
{
    return intervalSpin->value();
}

// ============================================================================
// Setters
// ============================================================================

void FrameWidget::setFrame(const CANFrame& frame)
{
    canIdEdit->setText(QString("0x%1").arg(frame.getId(), 0, 16));

    if (frame.getFrameType() == CanType::FD) {
        dlcComboFD->setCurrentIndex(frame.getDlc());
    } else {
        dlcSpin->setValue(frame.getDlc());
    }

    const QByteArray& d = frame.getData();
    QStringList dataBytes;
    for (int i = 0; i < d.size(); ++i) {
        dataBytes.append(QString("%1").arg(static_cast<quint8>(d[i]), 2, 16, QChar('0')).toUpper());
    }
    dataEdit->setText(dataBytes.join(" "));

    currentCanId = frame.getId();
}

void FrameWidget::setPeriodicEnabled(bool enabled)
{
    periodicCheckBox->setChecked(enabled);
}

void FrameWidget::setMinInterval(int ms)
{
   intervalSpin->setMinimum(ms);
    if (intervalSpin->value() < ms)
        intervalSpin->setValue(ms);
}

void FrameWidget::setMaxInterval(int ms)
{
    if (ms <= 0)
        intervalSpin->setMaximum(std::numeric_limits<int>::max());
    else
        intervalSpin->setMaximum(ms);

    if (ms > 0 && intervalSpin->value() > ms)
        intervalSpin->setValue(ms);
}

void FrameWidget::setIntervalValue(int ms)
{
    intervalSpin->setValue(ms);
}

QJsonObject FrameWidget::toJson() const
{
    CANFrame frame = getFrame();

    const QByteArray& d = frame.getData();
    QStringList bytes;
    for (int i = 0; i < d.size(); ++i)
        bytes.append(QString("%1").arg(static_cast<quint8>(d[i]), 2, 16, QChar('0')).toUpper());

    QJsonObject obj;
    obj["id"]              = static_cast<qint64>(frame.getId());
    obj["data"]            = bytes.join(" ");
    obj["dlc"]             = getEffectiveDlc();
    obj["periodicEnabled"] = isPeriodicEnabled();
    obj["intervalMs"]      = getPeriodicInterval();
    obj["visible"]         = !isHidden();  // isVisible() is false when parent tab is inactive
    return obj;
}

void FrameWidget::fromJson(const QJsonObject& obj)
{
    // ID was already set by addFrameWidget — keep it
    quint32 id = currentCanId;

    QString dataStr = obj["data"].toString();
    QStringList dataList = dataStr.split(' ', Qt::SkipEmptyParts);
    QByteArray data;
    bool ok;
    for (const QString& hex : dataList) {
        quint32 byte = hex.toUInt(&ok, 16);
        if (ok && byte <= 0xFF)
            data.append(static_cast<char>(byte));
    }

    quint16 dlc = static_cast<quint16>(obj["dlc"].toInt());
    setFrame(CANFrame(id, dlc, data, m_canType, m_idFormat));

    setIntervalValue(obj["intervalMs"].toInt(100));
    setPeriodicEnabled(obj["periodicEnabled"].toBool(false));
    setVisible(obj["visible"].toBool(true));
}

void FrameWidget::setConnectionChecker(std::function<bool()> checker)
{
    m_connectionChecker = std::move(checker);
}

void FrameWidget::setConnected(bool connected)
{
    m_connected = connected;
}

// ============================================================================
// Private helpers
// ============================================================================

bool FrameWidget::checkConnection()
{
    if (m_connectionChecker) {
        if (!m_connectionChecker()) {
            statusLabel->setText("✗ Error: Not connected (start server or connect client)");
            statusLabel->setStyleSheet("QLabel { color: red; }");
            return false;
        }
        return true;
    }
    if (!m_connected) {
        statusLabel->setText("✗ Error: Not connected (start server or connect client)");
        statusLabel->setStyleSheet("QLabel { color: red; }");
        return false;
    }
    return true;
}

// ============================================================================
// Slot handlers
// ============================================================================

/**
 * @brief Handle Send button click.
 *
 * Validates the CAN ID and data bytes against the current bus type and ID
 * format before emitting the appropriate signal:
 *  - One-shot: emits sendOnceClicked() and shows send result in status label.
 *  - Periodic: emits onSendFramePeriodicClicked(), swaps Send→Stop button.
 *
 * Both buttons are disabled during validation to prevent double-clicks.
 */
void FrameWidget::onSend()
{
    if (!checkConnection()) return;

    sendBtn->setEnabled(false);
    stopBtn->setEnabled(false);

    // --- Validate CAN ID ---
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    quint32 maxId = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                        : CANFrame::ID_MAX_EXTENDED;
    if (!ok || id > maxId) {
        statusLabel->setText(QString("✗ Error: Invalid CAN ID (max 0x%1)").arg(maxId, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: red; }");
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        return;
    }

    // --- Validate data bytes ---
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    int maxBytes = (m_canType == CanType::Classic) ? 8
                 : (m_canType == CanType::FD)      ? 64
                 :                                   2048;
    if (dataList.size() > maxBytes) {
        statusLabel->setText(QString("✗ Error: Max %1 data bytes").arg(maxBytes));
        statusLabel->setStyleSheet("QLabel { color: red; }");
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        return;
    }

    QByteArray data;
    for (int i = 0; i < dataList.size(); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (!ok || byte > 0xFF) {
            statusLabel->setText(QString("✗ Error: Invalid byte at position %1").arg(i + 1));
            statusLabel->setStyleSheet("QLabel { color: red; }");
            sendBtn->setVisible(true);
            stopBtn->setVisible(false);
            sendBtn->setEnabled(true);
            return;
        }
        data.append(static_cast<char>(byte));
    }

    CANFrame frame(id, static_cast<quint16>(getEffectiveDlc()), data, m_canType, m_idFormat);

    if (periodicCheckBox->isChecked()) {
        emit onSendFramePeriodicClicked(frame, intervalSpin->value());
        sendBtn->setVisible(false);
        stopBtn->setVisible(true);
        stopBtn->setEnabled(true);
        statusLabel->setText(QString("✓ Sending ID: 0x%1").arg(id, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: green; }");
    } else {
        emit sendOnceClicked(frame);
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        statusLabel->setText(QString("✓ Sent ID: 0x%1").arg(id, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: green; }");
    }
}

/**
 * @brief Handle Stop button click for periodic transmission.
 *
 * Re-validates the CAN ID (user may have edited it while running), then
 * emits onStopFramePeriodicClicked() to remove the frame from the server
 * or client periodic list. Swaps Stop→Send button on success.
 */
void FrameWidget::onStopFramePeriodic()
{
    sendBtn->setEnabled(false);
    stopBtn->setEnabled(false);

    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    quint32 maxId = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                        : CANFrame::ID_MAX_EXTENDED;
    if (!ok || id > maxId) {
        statusLabel->setText(QString("✗ Error: Invalid CAN ID (max 0x%1)").arg(maxId, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: red; }");
        stopBtn->setEnabled(true);
        return;
    }

    emit onStopFramePeriodicClicked(id);
    sendBtn->setEnabled(true);
    stopBtn->setVisible(false);
    sendBtn->setVisible(true);
    statusLabel->setText(QString("✓ Stopped ID: 0x%1").arg(id, 0, 16));
    statusLabel->setStyleSheet("QLabel { color: grey; }");
}

/**
 * @brief Update the status label when the Periodic checkbox is toggled.
 *
 * When checked: shows the current interval and enables intervalSpin.
 * When unchecked: shows "Periodic stopped" (Send button still armed for one-shot use).
 */
void FrameWidget::onPeriodicToggled()
{
    if (periodicCheckBox->isChecked()) {
        statusLabel->setText(QString("⟳ Periodic active (%1 ms)").arg(intervalSpin->value()));
        statusLabel->setStyleSheet("QLabel { color: blue; }");
    } else {
        statusLabel->setText("⏸ Periodic stopped");
        statusLabel->setStyleSheet("QLabel { color: gray; }");
    }
}

/// Emit removeClicked() so MainWindow shows a confirmation dialog and removes this widget.
void FrameWidget::onRemove()
{
    emit removeClicked(currentCanId);
}

/// Hide this widget and emit hideClicked() so MainWindow updates the hidden-frame counter.
void FrameWidget::onHide()
{
    setVisible(false);
    emit hideClicked();
}

/**
 * @brief Validate and commit a CAN ID change from the ID input field.
 *
 * Called on editingFinished (Enter or focus-out). If the new ID is invalid
 * or out of range for the current format, the field is reverted silently.
 * If the ID is valid and different from the current one, emits canIdChanged()
 * so MainWindow can check for duplicates and update its frameWidgets map.
 * Also updates the group box title to reflect the new ID.
 */
void FrameWidget::onCanIdChanged()
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 newId = idText.toUInt(&ok, 16);
    quint32 maxId = (m_idFormat == IdFormat::Standard) ? CANFrame::ID_MAX_STANDARD
                                                        : CANFrame::ID_MAX_EXTENDED;

    if (!ok || newId > maxId) {
        canIdEdit->setText(QString("0x%1").arg(currentCanId, 0, 16));
        return;
    }

    if (newId != currentCanId) {
        uint32_t oldId = currentCanId;
        currentCanId = newId;
        emit canIdChanged(oldId, newId);

        QGroupBox* box = findChild<QGroupBox*>();
        if (box)
            box->setTitle(QString("Frame ID: 0x%1").arg(newId, 0, 16));
    }
}

/**
 * @brief Auto-format data input and update DLC to match byte count.
 *
 * Called on every keystroke in the data field. Steps:
 *  1. Format the raw input into "XX XX XX ..." spacing via formatHexWithSpaces().
 *  2. Preserve the logical cursor position by compensating for spaces that
 *     were inserted or removed during formatting (spacesBefore vs spacesAfter).
 *  3. Warn if the final character count is odd (incomplete last byte).
 *  4. Auto-snap DLC: Classic/XL — clamp to max bytes; FD — round up to
 *     nearest valid DLC code using CANFrame::bytesToFdDlc().
 *
 * Signals are blocked during the setText() call to prevent this slot from
 * re-entering itself.
 */
void FrameWidget::onDataChanged(const QString& text)
{
    dataEdit->blockSignals(true);

    int cursorPos = dataEdit->cursorPosition();
    QString formatted = formatHexWithSpaces(text);

    // Compute how many spaces existed before and after the cursor in both
    // the old and new strings so we can shift the cursor by the difference.
    int spacesBefore = text.first(qMin(cursorPos, text.length())).count(' ');
    int adjustedPos  = qMin(cursorPos + (formatted.count(' ') - spacesBefore), formatted.length());
    int spacesAfter  = formatted.first(adjustedPos).count(' ');

    dataEdit->setText(formatted);
    dataEdit->setCursorPosition(cursorPos + (spacesAfter - spacesBefore));

    dataEdit->blockSignals(false);

    // Auto-update DLC based on byte count
    QString cleaned = formatted;
    cleaned.remove(' ');

    if (cleaned.length() % 2 != 0) {
        statusLabel->setText("⚠ Incomplete byte: enter 2 hex digits per byte");
        statusLabel->setStyleSheet("QLabel { color: orange; }");
    } else if (statusLabel->text().startsWith("⚠ Incomplete")) {
        statusLabel->setText("");
    }

    int byteCount = (cleaned.length() + 1) / 2;

    if (m_canType == CanType::FD) {
        // Snap to nearest valid FD DLC code (rounds up)
        int dlcCode = CANFrame::bytesToFdDlc(byteCount);
        dlcComboFD->setCurrentIndex(dlcCode);
    } else if (m_canType == CanType::XL) {
        dlcSpin->setValue(qMin(byteCount, 2048));
    } else {
        dlcSpin->setValue(qMin(byteCount, 8));
    }
}

/**
 * @brief Strip non-hex characters and reformat as "XX XX XX ..." (space every 2 chars).
 *
 * Steps:
 *  1. Uppercase the input.
 *  2. Remove any character that is not 0-9 or A-F.
 *  3. Re-insert a space before every even-indexed character (index 2, 4, 6, ...).
 *
 * This is used both by the data field (onDataChanged) and the filter bar
 * in the Analyzer tab, so they share identical formatting behaviour.
 *
 * @param input Raw user-typed string (may include spaces or garbage characters).
 * @return Cleaned, uppercase, space-separated hex string.
 */
QString FrameWidget::formatHexWithSpaces(const QString& input)
{
    QString cleaned = input.toUpper();
    cleaned.remove(QRegularExpression("[^0-9A-F]"));

    QString formatted;
    for (int i = 0; i < cleaned.length(); i++) {
        if (i > 0 && i % 2 == 0)
            formatted += ' ';
        formatted += cleaned[i];
    }

    return formatted;
}
