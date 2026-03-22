#include "statusbar.h"

#include <QFont>
#include <QLocale>

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
    , m_connected(false)
{
    setObjectName(QStringLiteral("StatusBar"));
    setFixedHeight(36);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 4, 16, 4);
    layout->setSpacing(24);

    QFont statusFont;
    statusFont.setPointSize(9);

    // Status indicator (dot + text)
    auto *statusWidget = new QWidget(this);
    auto *statusLayout = new QHBoxLayout(statusWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(6);

    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(8, 8);
    statusLayout->addWidget(m_statusIndicator, 0, Qt::AlignVCenter);

    m_statusLabel = new QLabel(QStringLiteral("Getrennt"), this);
    m_statusLabel->setFont(statusFont);
    statusLayout->addWidget(m_statusLabel, 0, Qt::AlignVCenter);

    layout->addWidget(statusWidget);

    // GPU name
    m_gpuLabel = new QLabel(QStringLiteral("GPU: --"), this);
    m_gpuLabel->setFont(statusFont);
    m_gpuLabel->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    layout->addWidget(m_gpuLabel);

    // Latency
    m_latencyLabel = new QLabel(QStringLiteral("Latenz: --"), this);
    m_latencyLabel->setFont(statusFont);
    m_latencyLabel->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    layout->addWidget(m_latencyLabel);

    // Frames processed
    m_framesLabel = new QLabel(QStringLiteral("Frames: --"), this);
    m_framesLabel->setFont(statusFont);
    m_framesLabel->setStyleSheet(QStringLiteral("color: #9ca3af;"));
    layout->addWidget(m_framesLabel);

    layout->addStretch();

    setConnected(false);
}

void StatusBar::setConnected(bool connected)
{
    m_connected = connected;

    if (connected) {
        m_statusIndicator->setStyleSheet(
            QStringLiteral("background-color: #76b900; border-radius: 4px;")
        );
        m_statusLabel->setText(QStringLiteral("Verbunden"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #76b900;"));
    } else {
        m_statusIndicator->setStyleSheet(
            QStringLiteral("background-color: #ef4444; border-radius: 4px;")
        );
        m_statusLabel->setText(QStringLiteral("Getrennt"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #ef4444;"));
        m_gpuLabel->setText(QStringLiteral("GPU: --"));
        m_latencyLabel->setText(QStringLiteral("Latenz: --"));
        m_framesLabel->setText(QStringLiteral("Frames: --"));
    }
}

void StatusBar::updateStatus(const DaemonStatus &status)
{
    setConnected(true);

    if (!status.gpuName.isEmpty()) {
        QString gpuText = QStringLiteral("GPU: ") + status.gpuName;
        if (!status.gpuArch.isEmpty())
            gpuText += QStringLiteral(" (") + status.gpuArch + QStringLiteral(")");
        m_gpuLabel->setText(gpuText);
    }

    if (!status.latencyMs.isEmpty()) {
        m_latencyLabel->setText(
            QStringLiteral("Latenz: ~") + status.latencyMs + QStringLiteral(" ms")
        );
    }

    if (!status.framesProcessed.isEmpty()) {
        // Format large numbers with separators (German locale uses dots)
        bool ok = false;
        qulonglong frames = status.framesProcessed.toULongLong(&ok);
        if (ok) {
            QLocale locale(QLocale::German);
            m_framesLabel->setText(
                QStringLiteral("Frames: ") + locale.toString(frames)
            );
        } else {
            m_framesLabel->setText(
                QStringLiteral("Frames: ") + status.framesProcessed
            );
        }
    }
}
