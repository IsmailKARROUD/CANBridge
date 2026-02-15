#include "aboutdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("About CANBridge");
    setFixedSize(400, 300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // App name and version
    QLabel* titleLabel = new QLabel("CANBridge");
    QFont titleFont;
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    QLabel* versionLabel = new QLabel(QString("Version %1").arg(APP_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(versionLabel);

    mainLayout->addSpacing(20);

    // Description
    QLabel* descLabel = new QLabel(
        "Bridge CAN bus communication across network (TCP/IP). "
        "Send and receive frames through TCP."
        );
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(descLabel);

    mainLayout->addSpacing(20);

    // Technologies
    QLabel* techLabel = new QLabel("Built with Qt 6 C++");
    techLabel->setAlignment(Qt::AlignCenter);
    QFont techFont;
    techFont.setItalic(true);
    techLabel->setFont(techFont);
    mainLayout->addWidget(techLabel);

    mainLayout->addSpacing(20);

    // Author signature
    QLabel* authorLabel = new QLabel("Developed by Er. Ismail KARROUD");
    authorLabel->setAlignment(Qt::AlignCenter);
    QFont authorFont;
    authorFont.setBold(true);
    authorLabel->setFont(authorFont);
    mainLayout->addWidget(authorLabel);

    mainLayout->addStretch();
}
