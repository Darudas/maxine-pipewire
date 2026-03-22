#include "dbusclient.h"

#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusPendingCallWatcher>
#include <QDebug>
#include <cmath>

const QString DBusClient::s_service   = QStringLiteral("org.maxine.Effects");
const QString DBusClient::s_path      = QStringLiteral("/org/maxine/Effects");
const QString DBusClient::s_interface = QStringLiteral("org.maxine.Effects");

DBusClient::DBusClient(QObject *parent)
    : QObject(parent)
    , m_iface(nullptr)
    , m_pollTimer(new QTimer(this))
    , m_connected(false)
    , m_wasConnected(false)
    , m_destroying(false)
{
    connect(m_pollTimer, &QTimer::timeout, this, &DBusClient::pollStatus);
    ensureInterface();
}

DBusClient::~DBusClient()
{
    m_destroying = true;
    stopPolling();
    delete m_iface;
    m_iface = nullptr;
}

void DBusClient::ensureInterface()
{
    if (m_iface) {
        delete m_iface;
        m_iface = nullptr;
    }

    m_iface = new QDBusInterface(
        s_service, s_path, s_interface,
        QDBusConnection::sessionBus()
    );
}

bool DBusClient::checkDaemon()
{
    if (!m_iface || !m_iface->isValid()) {
        ensureInterface();
    }

    if (!m_iface || !m_iface->isValid()) {
        updateConnectionState(false);
        return false;
    }

    return m_connected;
}

void DBusClient::updateConnectionState(bool nowConnected)
{
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

    const QVariant firstArg = reply.arguments().at(0);
    if (!firstArg.canConvert<QDBusArgument>())
        return result;

    const QDBusArgument arg = firstArg.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::ArrayType)
        return result;

    arg.beginArray();
    while (!arg.atEnd()) {
        if (arg.currentType() != QDBusArgument::StructureType)
            break;
        arg.beginStructure();
        EffectInfo info;
        arg >> info.id;
        arg >> info.enabled;
        arg >> info.intensity;
        // Clamp intensity to valid range
        if (std::isnan(info.intensity) || std::isinf(info.intensity))
            info.intensity = 1.0;
        info.intensity = qBound(0.0, info.intensity, 1.0);
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

    const QVariant firstArg = reply.arguments().at(0);
    if (!firstArg.canConvert<QDBusArgument>())
        return result;

    const QDBusArgument arg = firstArg.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::ArrayType)
        return result;

    arg.beginArray();
    while (!arg.atEnd()) {
        if (arg.currentType() != QDBusArgument::StructureType)
            break;
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

    const QVariant firstArg = reply.arguments().at(0);
    if (!firstArg.canConvert<QDBusArgument>())
        return status;

    const QDBusArgument arg = firstArg.value<QDBusArgument>();
    if (arg.currentType() != QDBusArgument::MapType)
        return status;

    arg.beginMap();
    while (!arg.atEnd()) {
        if (arg.currentType() != QDBusArgument::MapEntryType)
            break;
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

    // Mark as connected — caller (pollStatus) handles state transitions via updateConnectionState()
    m_connected = true;

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
            w->deleteLater();
            if (m_destroying) return;
            QDBusPendingReply<bool> reply = *w;
            emit effectToggled(name, !reply.isError());
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
            w->deleteLater();
            if (m_destroying) return;
            QDBusPendingReply<bool> reply = *w;
            emit effectToggled(name, !reply.isError());
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
            w->deleteLater();
            if (m_destroying) return;
            QDBusPendingReply<bool> reply = *w;
            emit intensityChanged(name, !reply.isError());
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
            w->deleteLater();
            if (m_destroying) return;
            QDBusPendingReply<bool> reply = *w;
            emit configReloaded(!reply.isError());
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
    if (!m_iface || !m_iface->isValid())
        ensureInterface();

    // getStatus() already updates m_connected / m_wasConnected internally
    DaemonStatus status = getStatus();

    if (!status.running && !m_connected) {
        // getStatus failed — daemon not reachable
        updateConnectionState(false);
        return;
    }

    updateConnectionState(true);
    emit statusUpdated(status);

    QVector<EffectInfo> effects = listEffects();
    emit effectsUpdated(effects);
}
