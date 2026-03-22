#pragma once

#include <QObject>
#include <QDBusInterface>
#include <QDBusConnection>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QString>

struct EffectInfo {
    QString id;
    bool enabled;
    double intensity;
};

struct DeviceInfo {
    QString id;
    QString name;
    QString type;
};

struct DaemonStatus {
    QString gpuName;
    QString gpuArch;
    QString sampleRate;
    QString latencyMs;
    QString framesProcessed;
    QString avgProcessTimeUs;
    QString activeEffects;
    QString frameSize;
    bool running;
};

class DBusClient : public QObject
{
    Q_OBJECT

public:
    explicit DBusClient(QObject *parent = nullptr);
    ~DBusClient() override;

    bool isConnected() const;

    // Synchronous calls (used for initial load)
    QVector<EffectInfo> listEffects();
    QVector<DeviceInfo> listDevices();
    DaemonStatus getStatus();

    // Async calls
    void enableEffectAsync(const QString &name);
    void disableEffectAsync(const QString &name);
    void setIntensityAsync(const QString &name, double value);
    void reloadConfigAsync();

    void startPolling(int intervalMs = 2000);
    void stopPolling();

signals:
    void daemonConnected();
    void daemonDisconnected();
    void effectsUpdated(const QVector<EffectInfo> &effects);
    void statusUpdated(const DaemonStatus &status);
    void devicesUpdated(const QVector<DeviceInfo> &devices);
    void effectToggled(const QString &name, bool success);
    void intensityChanged(const QString &name, bool success);
    void configReloaded(bool success);

private slots:
    void pollStatus();

private:
    void ensureInterface();
    bool checkDaemon();

    QDBusInterface *m_iface;
    QTimer *m_pollTimer;
    bool m_connected;
    bool m_wasConnected;

    static const QString s_service;
    static const QString s_path;
    static const QString s_interface;
};
