#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QScrollArea>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMap>
#include <QPushButton>

#include "dbusclient.h"
#include "effectcard.h"
#include "statusbar.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onSidebarChanged(int currentRow);
    void onDaemonConnected();
    void onDaemonDisconnected();
    void onEffectsUpdated(const QVector<EffectInfo> &effects);
    void onStatusUpdated(const DaemonStatus &status);
    void onDevicesUpdated(const QVector<DeviceInfo> &devices);
    void onEffectEnableChanged(const QString &id, bool enabled);
    void onEffectIntensityChanged(const QString &id, double value);
    void onQuickEnableDenoiser();
    void onDisableAllEffects();

private:
    void setupUi();
    void setupSidebar();
    void setupAudioPage();
    void setupSettingsPage();
    void setupDisconnectedPage();
    void setupSystemTray();
    void buildEffectCards();
    void showDisconnectedOverlay(bool show);

    // D-Bus
    DBusClient *m_dbus;

    // Layout
    QWidget *m_centralWidget;
    QListWidget *m_sidebar;
    QStackedWidget *m_pages;

    // Audio page
    QWidget *m_audioPage;
    QComboBox *m_deviceCombo;
    QScrollArea *m_effectsScroll;
    QWidget *m_effectsContainer;
    QVBoxLayout *m_effectsLayout;
    QMap<QString, EffectCard *> m_effectCards;

    // Quick setup
    QPushButton *m_quickDenoiserBtn;
    QPushButton *m_quickDisableAllBtn;

    // Settings page
    QWidget *m_settingsPage;
    QLabel *m_modelPathLabel;
    QLabel *m_sampleRateLabel;

    // Disconnected overlay
    QWidget *m_disconnectedOverlay;

    // Status bar
    StatusBar *m_statusBar;

    // System tray
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;

    // Catalog of effects (static metadata)
    QVector<EffectCatalogEntry> m_catalog;
};
