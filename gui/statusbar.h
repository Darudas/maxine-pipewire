#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>

#include "dbusclient.h"

class StatusBar : public QWidget
{
    Q_OBJECT

public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void updateStatus(const DaemonStatus &status);

private:
    QLabel *m_gpuLabel;
    QLabel *m_latencyLabel;
    QLabel *m_framesLabel;
    QLabel *m_statusIndicator;
    QLabel *m_statusLabel;
    bool m_connected;
};
