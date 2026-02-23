#include "framewidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QRegularExpression>
#include <limits>

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

    // DLC spinner
    controlsLayout->addWidget(new QLabel("DLC:"));
    dlcSpin = new QSpinBox();
    dlcSpin->setRange(0, 8);
    dlcSpin->setValue(8);
    dlcSpin->setMaximumWidth(60);
    controlsLayout->addWidget(dlcSpin);

    // Data bytes input
    controlsLayout->addWidget(new QLabel("Data:"));
    dataEdit = new QLineEdit("00 00 00 00 00 00 00 00");
    dataEdit->setMinimumWidth(200);
    controlsLayout->addWidget(dataEdit);

    // Periodic checkbox and interval
    periodicCheckBox = new QCheckBox("Periodic");
    controlsLayout->addWidget(periodicCheckBox);

    intervalSpin = new QSpinBox();
    // Range and default value are set by MainWindow via setMinInterval() /
    // setMaxInterval() / setIntervalValue() immediately after construction.
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
    connect(sendBtn, &QPushButton::clicked, this, &FrameWidget::onSend);
    connect(stopBtn, &QPushButton::clicked, this, &FrameWidget::onStopFramePeriodic);
    connect(hideBtn, &QPushButton::clicked, this, &FrameWidget::onHide);
    connect(removeBtn, &QPushButton::clicked, this, &FrameWidget::onRemove);
    connect(periodicCheckBox, &QCheckBox::toggled, this, &FrameWidget::onPeriodicToggled);
    connect(canIdEdit, &QLineEdit::editingFinished, this, &FrameWidget::onCanIdChanged);
    connect(dataEdit, &QLineEdit::textChanged, this, &FrameWidget::onDataChanged);

    // Enable/disable interval spinner based on checkbox
    connect(periodicCheckBox, &QCheckBox::toggled, intervalSpin, &QSpinBox::setEnabled);
}

CANFrame FrameWidget::getFrame() const
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok) id = 0x100;

    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    quint8 data[8] = {0};
    for (int i = 0; i < qMin(dataList.size(), 8); ++i) {
        quint32 byte = dataList[i].toUInt(&ok, 16);
        if (ok && byte <= 0xFF) {
            data[i] = static_cast<quint8>(byte);
        }
    }

    return CANFrame(id, dlcSpin->value(), data);
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

void FrameWidget::setFrame(const CANFrame& frame)
{
    canIdEdit->setText(QString("0x%1").arg(frame.getId(), 0, 16));
    dlcSpin->setValue(frame.getDlc());

    QStringList dataBytes;
    for (int i = 0; i < frame.getDlc(); ++i) {
        // CANFrame exposes individual bytes via getDataIndex(), not a raw pointer
        dataBytes.append(QString("%1").arg(frame.getDataIndex(i), 2, 16, QChar('0')).toUpper());
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

void FrameWidget::setConnectionChecker(std::function<bool()> checker)
{
    // Store the callable supplied by MainWindow. It will be invoked live on
    // every send attempt so it always reflects the current network state.
    m_connectionChecker = std::move(checker);
}

bool FrameWidget::checkConnection()
{
    // If no checker was injected yet, treat it as not connected
    if (!m_connectionChecker || !m_connectionChecker()) {
        statusLabel->setText("✗ Error: Not connected (start server or connect client)");
        statusLabel->setStyleSheet("QLabel { color: red; }");
        return false;
    }
    return true;
}

void FrameWidget::onSend()
{
    // --- Check connection before anything else ---
    // Queries the live checker lambda provided by MainWindow; bails out
    // with an error on the status label if no server/client is active.
    if (!checkConnection()) return;

    sendBtn->setEnabled(false);
    stopBtn->setEnabled(false);

    // --- Validate CAN ID ---
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {   // 29-bit extended CAN ID max
        statusLabel->setText("✗ Error: Invalid CAN ID (max 0x1FFFFFFF)");
        statusLabel->setStyleSheet("QLabel { color: red; }");
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        return;
    }

    // --- Validate data bytes ---
    QStringList dataList = dataEdit->text().split(' ', Qt::SkipEmptyParts);
    if (dataList.size() > 8) {
        statusLabel->setText("✗ Error: Max 8 data bytes");
        statusLabel->setStyleSheet("QLabel { color: red; }");
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        return;
    }

    quint8 data[8] = {0};
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
        data[i] = static_cast<quint8>(byte);
    }

    // --- All inputs valid: build frame and emit ---
    // Connection check (server running / client connected) is handled

    CANFrame frame(id, dlcSpin->value(), data);
    if(periodicCheckBox->isChecked()) {
        // by MainWindow::onSendFramePeriodic() after receiving this signal.
        emit onSendFramePeriodicClicked(frame, intervalSpin->value());
        sendBtn->setVisible(false);
        stopBtn->setVisible(true);
        stopBtn->setEnabled(true);
        statusLabel->setText(QString("✓ Sending ID: 0x%1").arg(id, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: green; }");
    }else {
        // by MainWindow::onFrameSendOnce() after receiving this signal.
        emit sendOnceClicked(frame);
        sendBtn->setVisible(true);
        stopBtn->setVisible(false);
        sendBtn->setEnabled(true);
        statusLabel->setText(QString("✓ Sent ID: 0x%1").arg(id, 0, 16));
        statusLabel->setStyleSheet("QLabel { color: green; }");
    }

}
void FrameWidget::onStopFramePeriodic(){

    sendBtn->setEnabled(false);
    stopBtn->setEnabled(false);
    // --- Validate CAN ID ---
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 id = idText.toUInt(&ok, 16);
    if (!ok || id > 0x1FFFFFFF) {   // 29-bit extended CAN ID max
        statusLabel->setText("✗ Error: Invalid CAN ID (max 0x1FFFFFFF)");
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

void FrameWidget::onRemove()
{
    emit removeClicked(currentCanId);
}

void FrameWidget::onHide()
{
    setVisible(false);
    emit hideClicked();
}

void FrameWidget::onCanIdChanged()
{
    bool ok;
    QString idText = canIdEdit->text().remove("0x", Qt::CaseInsensitive);
    quint32 newId = idText.toUInt(&ok, 16);

    if (!ok || newId > 0x1FFFFFFF) {
        // Invalid ID - revert to old
        canIdEdit->setText(QString("0x%1").arg(currentCanId, 0, 16));
        return;
    }

    if (newId != currentCanId) {
        uint32_t oldId = currentCanId;
        currentCanId = newId;
        emit canIdChanged(oldId, newId);

        // Update group box title
        QGroupBox* box = findChild<QGroupBox*>();
        if (box) {
            box->setTitle(QString("Frame ID: 0x%1").arg(newId, 0, 16));
        }
    }
}

void FrameWidget::onDataChanged(const QString& text)
{
    // Block signals to prevent infinite loop
    dataEdit->blockSignals(true);

    int cursorPos = dataEdit->cursorPosition();
    QString formatted = formatHexWithSpaces(text);

    // Calculate cursor adjustment
    int spacesBefore = text.first(qMin(cursorPos, text.length())).count(' ');
    int adjustedPos = qMin(cursorPos + (formatted.count(' ') - spacesBefore), formatted.length());
    int spacesAfter = formatted.first(adjustedPos).count(' ');

    dataEdit->setText(formatted);
    dataEdit->setCursorPosition(cursorPos + (spacesAfter - spacesBefore));

    dataEdit->blockSignals(false);

    // Auto-update DLC
    QString cleaned = formatted;
    cleaned.remove(' ');
    int byteCount = (cleaned.length() + 1) / 2;
    dlcSpin->setValue(qMin(byteCount, 8));
}

QString FrameWidget::formatHexWithSpaces(const QString& input)
{
    QString cleaned = input.toUpper();
    cleaned.remove(QRegularExpression("[^0-9A-F]"));

    QString formatted;
    for (int i = 0; i < cleaned.length(); i++) {
        if (i > 0 && i % 2 == 0) {
            formatted += ' ';
        }
        formatted += cleaned[i];
    }

    return formatted;
}
