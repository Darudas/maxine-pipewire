#include "dbusclient.h"

#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDebug>

const QString DBusClient::s_service   = QStringLiteral("org.maxine.Effects");
const QString DBusClient::s_path      = QStringLiteral("/org/maxine/Effects");
const QString DBusClient::s_interface = QStringLiteral("org.maxine.Effects");

DBusClient::DBusClient(QObject *parent)
    : QObject(parent)
    , m_iface(nullptr)
    , m_pollTimer(new QTimer(this))
    , m_connected(false)
    , m_wasConnected(false)
{
    connect(m_pollTimer, &QTimer::timeout, this, &DBusClient::pollStatus);
    ensureInterface();
}

DBusClient::~DBusClient()
{
    stopPolling();
    delete m_iface;
}

void DBusClient::ensureInterface()
{
    if (m_iface) {
        delete m_iface;
        m_iface = nullptr;
    }

    m_iface = new QDBusInterface(
        s_service, s_path, s_interface,
        QDBusConnection::sessionBus(), this
    );
}

bool DBusClient::checkDaemon()
{
    if (!m_iface || !m_iface->isValid()) {
        ensureInterface();
    }

    // Try a lightweight call to check if daemon is alive
    QDBusReply<QDBusArgument> reply = m_iface->call(
        QDBus::Block, QStringLiteral("GetStatus")
    );

    bool nowConnected = reply.isValid();

    if (nowConnected && !m_wasConnected) {
        m_connected = true;
        m_wasConnected = true;
        emit daemonConnected();
    } else if (!nowConnected && m_wasConnected) {
        m_connected = false;
        m_wasConnected = false;
        emit daemonDisconnected();
    } else {
        m_connected = nowConnected;
    }

    return m_connected;
}

bool DBusClient::isConnected() const
{
    return m_connected;
}

QVector<EffectInfo> DBusClient::listEffects()
{
    QVector<EffectInfo> result;

    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusMessage reply = m_iface->call(QDBus::Block, QStringLiteral("ListEffects"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return result;
    }

    if (reply.arguments().isEmpty())
        return result;

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();
        EffectInfo info;
        arg >> info.id;
        arg >> info.enabled;
        arg >> info.intensity;
        arg.endStructure();
        result.append(info);
    }
    arg.endArray();

    return result;
}

QVector<DeviceInfo> DBusClient::listDevices()
{
    QVector<DeviceInfo> result;

    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusMessage reply = m_iface->call(QDBus::Block, QStringLiteral("ListDevices"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        return result;
    }

    if (reply.arguments().isEmpty())
        return result;

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd()) {
        arg.beginStructure();
        DeviceInfo info;
        arg >> info.id;
        arg >> info.name;
        arg >> info.type;
        arg.endStructure();
        result.append(info);
    }
    arg.endArray();

    return result;
}

DaemonStatus DBusClient::getStatus()
{
    DaemonStatus status;
    status.running = false;

    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusMessage reply = m_iface->call(QDBus::Block, QStringLiteral("GetStatus"));
    if (reply.type() == QDBusMessage::ErrorMessage) {
        m_connected = false;
        return status;
    }

    if (reply.arguments().isEmpty())
        return status;

    const QDBusArgument arg = reply.arguments().at(0).value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd()) {
        arg.beginMapEntry();
        QString key, value;
        arg >> key >> value;
        arg.endMapEntry();

        if (key == QLatin1String("gpu_name"))
            status.gpuName = value;
        else if (key == QLatin1String("gpu_arch"))
            status.gpuArch = value;
        else if (key == QLatin1String("running"))
            status.running = (value == QLatin1String("true"));
        else if (key == QLatin1String("sample_rate"))
            status.sampleRate = value;
        else if (key == QLatin1String("latency_ms"))
            status.latencyMs = value;
        else if (key == QLatin1String("frames_processed"))
            status.framesProcessed = value;
        else if (key == QLatin1String("avg_process_time_us"))
            status.avgProcessTimeUs = value;
        else if (key == QLatin1String("active_effects"))
            status.activeEffects = value;
        else if (key == QLatin1String("frame_size"))
            status.frameSize = value;
    }
    arg.endMap();

    m_connected = true;
    if (!m_wasConnected) {
        m_wasConnected = true;
        emit daemonConnected();
    }

    return status;
}

void DBusClient::enableEffectAsync(const QString &name)
{
    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusPendingCall pending = m_iface->asyncCall(
        QStringLiteral("EnableEffect"), name
    );

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, name](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<bool> reply = *w;
            emit effectToggled(name, !reply.isError());
            w->deleteLater();
            // Trigger an immediate refresh
            pollStatus();
        }
    );
}

void DBusClient::disableEffectAsync(const QString &name)
{
    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusPendingCall pending = m_iface->asyncCall(
        QStringLiteral("DisableEffect"), name
    );

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, name](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<bool> reply = *w;
            emit effectToggled(name, !reply.isError());
            w->deleteLater();
            pollStatus();
        }
    );
}

void DBusClient::setIntensityAsync(const QString &name, double value)
{
    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusPendingCall pending = m_iface->asyncCall(
        QStringLiteral("SetIntensity"), name, value
    );

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, name](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<bool> reply = *w;
            emit intensityChanged(name, !reply.isError());
            w->deleteLater();
        }
    );
}

void DBusClient::reloadConfigAsync()
{
    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    QDBusPendingCall pending = m_iface->asyncCall(
        QStringLiteral("ReloadConfig")
    );

    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<bool> reply = *w;
            emit configReloaded(!reply.isError());
            w->deleteLater();
        }
    );
}

void DBusClient::startPolling(int intervalMs)
{
    m_pollTimer->start(intervalMs);
    // Do an immediate poll
    pollStatus();
}

void DBusClient::stopPolling()
{
    m_pollTimer->stop();
}

void DBusClient::pollStatus()
{
    if (!checkDaemon()) {
        return;
    }

    DaemonStatus status = getStatus();
    emit statusUpdated(status);

    QVector<EffectInfo> effects = listEffects();
    emit effectsUpdated(effects);
}
