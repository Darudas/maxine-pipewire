#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFont>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QUrl>
#include <QVBoxLayout>

// Build the static effect catalog — German UI, 2 categories
static QVector<EffectCatalogEntry> buildCatalog()
{
    QVector<EffectCatalogEntry> cat;

    // ---- Grundlagen (Basics) ----

    cat.append({QStringLiteral("denoiser"),
                QStringLiteral("Rauschunterdr\u00fcckung"),
                QStringLiteral("Entfernt Hintergrundger\u00e4usche wie Tastaturklappern, "
                               "L\u00fcfter, Stra\u00dfenl\u00e4rm und andere "
                               "Umgebungsger\u00e4usche aus dem Mikrofonsignal."),
                QStringLiteral("basics"), true,
                QStringLiteral("Die Rauschunterdr\u00fcckung analysiert dein Audiosignal in Echtzeit "
                               "und filtert alle nicht-sprachlichen Ger\u00e4usche heraus. "
                               "Die St\u00e4rke bestimmt, wie aggressiv gefiltert wird \u2014 "
                               "h\u00f6here Werte entfernen mehr Ger\u00e4usche, k\u00f6nnen aber "
                               "die Stimmqualit\u00e4t leicht beeintr\u00e4chtigen."),
                QStringLiteral("Empfohlen f\u00fcr: Calls, Gaming, Streaming"),
                true});

    cat.append({QStringLiteral("dereverb"),
                QStringLiteral("Hall-Entfernung"),
                QStringLiteral("Reduziert den Raumhall und l\u00e4sst dein Mikrofon klingen, "
                               "als w\u00e4rst du in einem professionellen Studio ohne Echo "
                               "von den W\u00e4nden."),
                QStringLiteral("basics"), true,
                QStringLiteral("Raumhall entsteht durch Schallreflexionen an W\u00e4nden, "
                               "Decken und harten Oberfl\u00e4chen. Dieser Effekt entfernt "
                               "den Nachhall und macht die Stimme klarer und direkter."),
                QStringLiteral("Empfohlen f\u00fcr: Gro\u00dfe R\u00e4ume, Badezimmer, leere Zimmer"),
                false});

    cat.append({QStringLiteral("dereverb-denoiser"),
                QStringLiteral("Rauschen + Hall (Kombi)"),
                QStringLiteral("Kombiniert Rauschunterdr\u00fcckung und Hall-Entfernung in einem "
                               "einzigen Durchgang. Weniger Latenz als beide einzeln zu aktivieren."),
                QStringLiteral("basics"), true,
                QStringLiteral("Dieser kombinierte Effekt f\u00fchrt beide Verarbeitungsschritte "
                               "in einem einzigen GPU-Pass durch, was die Verz\u00f6gerung "
                               "gegen\u00fcber zwei einzelnen Effekten deutlich reduziert."),
                QStringLiteral("Alternative zu: Denoiser + Dereverb einzeln"),
                true});

    cat.append({QStringLiteral("aec"),
                QStringLiteral("Echo-Unterdr\u00fcckung"),
                QStringLiteral("Filtert Sounds die aus deinen Lautsprechern kommen aus dem "
                               "Mikrofon raus. So h\u00f6ren deine Gespr\u00e4chspartner nicht "
                               "was du gerade abspielst."),
                QStringLiteral("basics"), true,
                QStringLiteral("Acoustic Echo Cancellation (AEC) erkennt das Audiosignal, das "
                               "aus deinen Lautsprechern kommt, und subtrahiert es vom "
                               "Mikrofonsignal. Essentiell wenn du ohne Kopfh\u00f6rer arbeitest."),
                QStringLiteral("Empfohlen f\u00fcr: Wenn du ohne Kopfh\u00f6rer spielst/redest"),
                true});

    // ---- Erweitert (Advanced) ----

    cat.append({QStringLiteral("superres"),
                QStringLiteral("Audio Super Resolution"),
                QStringLiteral("Verbessert die Qualit\u00e4t von niedrig aufgel\u00f6sten "
                               "Audiosignalen (8/16 kHz) auf 48 kHz. N\u00fctzlich bei "
                               "schlechten Mikrofonen oder VoIP."),
                QStringLiteral("advanced"), false,
                QStringLiteral("Super Resolution verwendet ein neuronales Netzwerk, um fehlende "
                               "Frequenzen in niedrig aufgel\u00f6sten Audiosignalen zu "
                               "rekonstruieren. Das Ergebnis klingt nat\u00fcrlicher und klarer."),
                QStringLiteral("Speziell f\u00fcr: USB-Headsets mit niedriger Samplerate"),
                false});

    cat.append({QStringLiteral("studio-voice-hq"),
                QStringLiteral("Studio-Stimme (Hohe Qualit\u00e4t)"),
                QStringLiteral("L\u00e4sst jedes Mikrofon wie ein professionelles Studiomikrofon "
                               "klingen. Verbessert Klarheit und W\u00e4rme der Stimme, "
                               "h\u00f6here Latenz."),
                QStringLiteral("advanced"), false,
                QStringLiteral("Dieser Effekt wendet ein trainiertes Modell an, das die "
                               "Klangeigenschaften eines hochwertigen Studiomikrofons simuliert. "
                               "Die HQ-Variante hat h\u00f6here Qualit\u00e4t, aber mehr "
                               "Verz\u00f6gerung."),
                QStringLiteral("Speziell f\u00fcr: Podcasts, Aufnahmen, Pr\u00e4sentationen"),
                false});

    cat.append({QStringLiteral("studio-voice-ll"),
                QStringLiteral("Studio-Stimme (Geringe Latenz)"),
                QStringLiteral("Gleicher Effekt wie Studio-Stimme, aber mit weniger "
                               "Verz\u00f6gerung. Leicht geringere Qualit\u00e4t, daf\u00fcr "
                               "besser f\u00fcr Live-Gespr\u00e4che."),
                QStringLiteral("advanced"), false,
                QStringLiteral("Die Low-Latency-Variante des Studio-Voice-Effekts. "
                               "Geeignet f\u00fcr Echtzeit-Kommunikation, wo die HQ-Variante "
                               "zu viel Verz\u00f6gerung verursacht."),
                QStringLiteral("Speziell f\u00fcr: Live-Calls wenn Studio-Voice zu langsam ist"),
                false});

    cat.append({QStringLiteral("speaker-focus"),
                QStringLiteral("Sprecher-Fokus"),
                QStringLiteral("Isoliert deine Stimme und entfernt alle anderen Sprecher im "
                               "Raum. Nur die lauteste/n\u00e4chste Stimme wird durchgelassen."),
                QStringLiteral("advanced"), true,
                QStringLiteral("Speaker Focus verwendet Richtungserkennung und Stimmseparation, "
                               "um nur den Hauptsprecher durchzulassen. Alle anderen Stimmen "
                               "im Raum werden unterdr\u00fcckt."),
                QStringLiteral("Speziell f\u00fcr: B\u00fcro, Co-Working, wenn andere im Raum reden"),
                false});

    cat.append({QStringLiteral("voice-font-hq"),
                QStringLiteral("Stimmenver\u00e4nderung (HQ)"),
                QStringLiteral("Wandelt deine Stimme um, sodass sie wie eine Referenzstimme "
                               "klingt. Ben\u00f6tigt eine WAV-Datei als Vorlage."),
                QStringLiteral("advanced"), false,
                QStringLiteral("Voice Font konvertiert die Klangeigenschaften deiner Stimme "
                               "in die einer Referenzaufnahme. Tonlage, Timbre und "
                               "Sprechcharakteristik werden \u00fcbernommen."),
                QStringLiteral("Speziell f\u00fcr: Content Creation, Unterhaltung"),
                false});

    cat.append({QStringLiteral("voice-font-ll"),
                QStringLiteral("Stimmenver\u00e4nderung (Geringe Latenz)"),
                QStringLiteral("Gleiche Stimmver\u00e4nderung, aber schneller. Geringere "
                               "Qualit\u00e4t, daf\u00fcr nutzbar in Echtzeit-Gespr\u00e4chen."),
                QStringLiteral("advanced"), false,
                QStringLiteral("Die Low-Latency-Variante der Stimmver\u00e4nderung. "
                               "Weniger pr\u00e4zise Konvertierung, aber nutzbar ohne "
                               "sp\u00fcrbare Verz\u00f6gerung in Live-Situationen."),
                QStringLiteral("Speziell f\u00fcr: Live-Nutzung der Stimmver\u00e4nderung"),
                false});

    return cat;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dbus(new DBusClient(this))
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
{
    m_catalog = buildCatalog();

    setWindowTitle(QStringLiteral("Maxine PipeWire \u2014 NVIDIA Audio-Effekte f\u00fcr Linux"));
    resize(960, 720);
    setMinimumSize(740, 520);

    setupUi();
    setupSystemTray();

    // Connect D-Bus signals
    connect(m_dbus, &DBusClient::daemonConnected,
            this, &MainWindow::onDaemonConnected);
    connect(m_dbus, &DBusClient::daemonDisconnected,
            this, &MainWindow::onDaemonDisconnected);
    connect(m_dbus, &DBusClient::effectsUpdated,
            this, &MainWindow::onEffectsUpdated);
    connect(m_dbus, &DBusClient::statusUpdated,
            this, &MainWindow::onStatusUpdated);
    connect(m_dbus, &DBusClient::devicesUpdated,
            this, &MainWindow::onDevicesUpdated);

    // Start polling
    m_dbus->startPolling(2000);
}

MainWindow::~MainWindow()
{
    m_dbus->stopPolling();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_disconnectedOverlay && m_centralWidget) {
        m_disconnectedOverlay->setGeometry(m_centralWidget->rect());
    }
}

void MainWindow::setupUi()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    auto *rootLayout = new QVBoxLayout(m_centralWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Title bar area
    auto *titleBar = new QWidget(this);
    titleBar->setObjectName(QStringLiteral("TitleBar"));
    titleBar->setFixedHeight(52);
    titleBar->setStyleSheet(
        QStringLiteral("QWidget#TitleBar {"
                        "  background-color: #0f1623;"
                        "  border-bottom: 1px solid #1e2a3a;"
                        "}")
    );

    auto *titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(20, 0, 20, 0);

    // NVIDIA-style green bar accent
    auto *accentBar = new QWidget(this);
    accentBar->setFixedSize(4, 24);
    accentBar->setStyleSheet(QStringLiteral("background-color: #76b900; border-radius: 2px;"));
    titleLayout->addWidget(accentBar);

    auto *titleLabel = new QLabel(QStringLiteral("Maxine PipeWire"), this);
    titleLabel->setObjectName(QStringLiteral("TitleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QStringLiteral("color: #e5e7eb; margin-left: 10px;"));
    titleLayout->addWidget(titleLabel);

    auto *subtitleLabel = new QLabel(QStringLiteral("NVIDIA Audio-Effekte f\u00fcr Linux"), this);
    QFont subFont = subtitleLabel->font();
    subFont.setPointSize(9);
    subtitleLabel->setFont(subFont);
    subtitleLabel->setStyleSheet(QStringLiteral("color: #6b7280; margin-left: 8px;"));
    titleLayout->addWidget(subtitleLabel);

    titleLayout->addStretch();

    rootLayout->addWidget(titleBar);

    // Main content area: sidebar + pages
    auto *contentWidget = new QWidget(this);
    auto *contentLayout = new QHBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    setupSidebar();
    contentLayout->addWidget(m_sidebar);

    // Separator line
    auto *sepLine = new QFrame(this);
    sepLine->setFrameShape(QFrame::VLine);
    sepLine->setStyleSheet(QStringLiteral("color: #1e2a3a;"));
    contentLayout->addWidget(sepLine);

    // Pages
    m_pages = new QStackedWidget(this);
    setupAudioPage();
    setupSettingsPage();
    setupDisconnectedPage();

    m_pages->addWidget(m_audioPage);
    m_pages->addWidget(m_settingsPage);

    contentLayout->addWidget(m_pages, 1);

    rootLayout->addWidget(contentWidget, 1);

    // Status bar at bottom
    m_statusBar = new StatusBar(this);
    m_statusBar->setStyleSheet(
        QStringLiteral("QWidget#StatusBar {"
                        "  background-color: #0f1623;"
                        "  border-top: 1px solid #1e2a3a;"
                        "}")
    );
    rootLayout->addWidget(m_statusBar);

    // Disconnected overlay (on top of content)
    m_disconnectedOverlay = new QWidget(m_centralWidget);
    m_disconnectedOverlay->setObjectName(QStringLiteral("DisconnectedOverlay"));
    m_disconnectedOverlay->setStyleSheet(
        QStringLiteral("QWidget#DisconnectedOverlay {"
                        "  background-color: rgba(15, 22, 35, 220);"
                        "}")
    );

    auto *overlayLayout = new QVBoxLayout(m_disconnectedOverlay);
    overlayLayout->setAlignment(Qt::AlignCenter);

    auto *disconnIcon = new QLabel(m_disconnectedOverlay);
    disconnIcon->setText(QStringLiteral("!"));
    disconnIcon->setAlignment(Qt::AlignCenter);
    disconnIcon->setFixedSize(64, 64);
    disconnIcon->setStyleSheet(
        QStringLiteral("background-color: #374151;"
                        " border-radius: 32px;"
                        " color: #ef4444;"
                        " font-size: 32px;"
                        " font-weight: bold;")
    );
    overlayLayout->addWidget(disconnIcon, 0, Qt::AlignCenter);

    auto *disconnTitle = new QLabel(
        QStringLiteral("Daemon nicht erreichbar"), m_disconnectedOverlay
    );
    QFont disconnFont = disconnTitle->font();
    disconnFont.setPointSize(18);
    disconnFont.setBold(true);
    disconnTitle->setFont(disconnFont);
    disconnTitle->setStyleSheet(QStringLiteral("color: #e5e7eb; margin-top: 16px;"));
    disconnTitle->setAlignment(Qt::AlignCenter);
    overlayLayout->addWidget(disconnTitle, 0, Qt::AlignCenter);

    auto *disconnDesc = new QLabel(
        QStringLiteral("Der maxined-Daemon l\u00e4uft nicht oder ist nicht \u00fcber D-Bus erreichbar.\n"
                        "Starte ihn mit folgendem Befehl:"),
        m_disconnectedOverlay
    );
    disconnDesc->setStyleSheet(QStringLiteral("color: #9ca3af; margin-top: 8px;"));
    disconnDesc->setAlignment(Qt::AlignCenter);
    overlayLayout->addWidget(disconnDesc, 0, Qt::AlignCenter);

    auto *cmdLabel = new QLabel(
        QStringLiteral("systemctl --user start maxined"), m_disconnectedOverlay
    );
    cmdLabel->setStyleSheet(
        QStringLiteral("background-color: #1f2937;"
                        " border: 1px solid #374151;"
                        " border-radius: 6px;"
                        " padding: 10px 20px;"
                        " color: #76b900;"
                        " font-family: monospace;"
                        " font-size: 13px;"
                        " margin-top: 12px;")
    );
    cmdLabel->setAlignment(Qt::AlignCenter);
    cmdLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    overlayLayout->addWidget(cmdLabel, 0, Qt::AlignCenter);

    auto *hintLabel = new QLabel(
        QStringLiteral("Die GUI verbindet sich automatisch, sobald der Daemon l\u00e4uft."),
        m_disconnectedOverlay
    );
    hintLabel->setStyleSheet(QStringLiteral("color: #6b7280; margin-top: 16px; font-size: 10px;"));
    hintLabel->setAlignment(Qt::AlignCenter);
    overlayLayout->addWidget(hintLabel, 0, Qt::AlignCenter);

    m_disconnectedOverlay->setGeometry(m_centralWidget->rect());
    m_disconnectedOverlay->raise();
    m_disconnectedOverlay->show();

    // Connect sidebar
    connect(m_sidebar, &QListWidget::currentRowChanged,
            this, &MainWindow::onSidebarChanged);
    m_sidebar->setCurrentRow(0);
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QListWidget(this);
    m_sidebar->setObjectName(QStringLiteral("Sidebar"));
    m_sidebar->setFixedWidth(180);
    m_sidebar->setIconSize(QSize(20, 20));
    m_sidebar->setSpacing(2);

    // Audio section
    auto *audioItem = new QListWidgetItem(QStringLiteral("  Audio"));
    audioItem->setSizeHint(QSize(180, 44));
    m_sidebar->addItem(audioItem);

    // Video section (greyed out)
    auto *videoItem = new QListWidgetItem(QStringLiteral("  Video (bald)"));
    videoItem->setSizeHint(QSize(180, 44));
    videoItem->setFlags(videoItem->flags() & ~Qt::ItemIsEnabled);
    videoItem->setToolTip(QStringLiteral("Kommt bald"));
    m_sidebar->addItem(videoItem);

    // Settings section
    auto *settingsItem = new QListWidgetItem(QStringLiteral("  Einstellungen"));
    settingsItem->setSizeHint(QSize(180, 44));
    m_sidebar->addItem(settingsItem);

    m_sidebar->setStyleSheet(
        QStringLiteral(
            "QListWidget#Sidebar {"
            "  background-color: #0f1623;"
            "  border: none;"
            "  padding-top: 12px;"
            "  outline: none;"
            "}"
            "QListWidget#Sidebar::item {"
            "  color: #9ca3af;"
            "  padding: 10px 16px;"
            "  border-radius: 8px;"
            "  margin: 2px 8px;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "}"
            "QListWidget#Sidebar::item:selected {"
            "  background-color: #1f2937;"
            "  color: #76b900;"
            "  font-weight: bold;"
            "}"
            "QListWidget#Sidebar::item:hover:!selected {"
            "  background-color: #161f2e;"
            "  color: #d1d5db;"
            "}"
            "QListWidget#Sidebar::item:disabled {"
            "  color: #374151;"
            "}"
        )
    );
}

void MainWindow::setupAudioPage()
{
    m_audioPage = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(m_audioPage);
    pageLayout->setContentsMargins(24, 20, 24, 12);
    pageLayout->setSpacing(16);

    // Page header
    auto *headerLabel = new QLabel(QStringLiteral("Audio-Effekte"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(18);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
    pageLayout->addWidget(headerLabel);

    auto *headerSubLabel = new QLabel(
        QStringLiteral("GPU-beschleunigte Audioeffekte f\u00fcr dein Mikrofon. "
                        "Aktiviere Effekte per Schalter."),
        this
    );
    headerSubLabel->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 11px; margin-bottom: 4px;"));
    headerSubLabel->setWordWrap(true);
    pageLayout->addWidget(headerSubLabel);

    // Device selector
    auto *deviceWidget = new QWidget(this);
    deviceWidget->setObjectName(QStringLiteral("DeviceSelector"));
    deviceWidget->setStyleSheet(
        QStringLiteral("QWidget#DeviceSelector {"
                        "  background-color: #1f2937;"
                        "  border: 1px solid #374151;"
                        "  border-radius: 10px;"
                        "  padding: 6px;"
                        "}")
    );
    auto *deviceLayout = new QHBoxLayout(deviceWidget);
    deviceLayout->setContentsMargins(14, 10, 14, 10);

    auto *deviceLabel = new QLabel(QStringLiteral("Eingabeger\u00e4t"), this);
    QFont devFont = deviceLabel->font();
    devFont.setPointSize(11);
    devFont.setBold(true);
    deviceLabel->setFont(devFont);
    deviceLabel->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    deviceLayout->addWidget(deviceLabel);

    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(280);
    m_deviceCombo->setFixedHeight(32);
    m_deviceCombo->addItem(QStringLiteral("Standard (warte auf Daemon...)"));
    m_deviceCombo->setStyleSheet(
        QStringLiteral(
            "QComboBox {"
            "  background-color: #111827;"
            "  color: #e5e7eb;"
            "  border: 1px solid #374151;"
            "  border-radius: 6px;"
            "  padding: 4px 12px;"
            "  font-size: 11px;"
            "}"
            "QComboBox::drop-down {"
            "  border: none;"
            "  width: 24px;"
            "}"
            "QComboBox::down-arrow {"
            "  image: none;"
            "  border-left: 5px solid transparent;"
            "  border-right: 5px solid transparent;"
            "  border-top: 6px solid #9ca3af;"
            "  margin-right: 8px;"
            "}"
            "QComboBox QAbstractItemView {"
            "  background-color: #1f2937;"
            "  color: #e5e7eb;"
            "  border: 1px solid #374151;"
            "  selection-background-color: #374151;"
            "  outline: none;"
            "}"
        )
    );
    deviceLayout->addWidget(m_deviceCombo, 1);

    pageLayout->addWidget(deviceWidget);

    // Quick Setup section
    auto *quickWidget = new QWidget(this);
    quickWidget->setObjectName(QStringLiteral("QuickSetup"));
    quickWidget->setStyleSheet(
        QStringLiteral("QWidget#QuickSetup {"
                        "  background-color: #1a2a1a;"
                        "  border: 1px solid #2a4a1a;"
                        "  border-radius: 10px;"
                        "  padding: 6px;"
                        "}")
    );
    auto *quickLayout = new QVBoxLayout(quickWidget);
    quickLayout->setContentsMargins(14, 10, 14, 10);
    quickLayout->setSpacing(8);

    auto *quickTitle = new QLabel(QStringLiteral("Schnellstart"), this);
    QFont quickTitleFont = quickTitle->font();
    quickTitleFont.setPointSize(11);
    quickTitleFont.setBold(true);
    quickTitle->setFont(quickTitleFont);
    quickTitle->setStyleSheet(QStringLiteral("color: #76b900;"));
    quickLayout->addWidget(quickTitle);

    auto *quickDesc = new QLabel(
        QStringLiteral("F\u00fcr die meisten Nutzer reicht der Denoiser. "
                        "Aktiviere weitere Effekte nur wenn n\u00f6tig."),
        this
    );
    quickDesc->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 10px;"));
    quickDesc->setWordWrap(true);
    quickLayout->addWidget(quickDesc);

    auto *quickBtnRow = new QHBoxLayout();
    quickBtnRow->setSpacing(10);

    m_quickDenoiserBtn = new QPushButton(
        QStringLiteral("\u2713 Denoiser aktivieren"), this
    );
    m_quickDenoiserBtn->setFixedHeight(32);
    m_quickDenoiserBtn->setCursor(Qt::PointingHandCursor);
    m_quickDenoiserBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: #1a3a0a;"
            "  color: #76b900;"
            "  border: 1px solid #76b900;"
            "  border-radius: 6px;"
            "  padding: 4px 16px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "  background-color: #2a4a1a;"
            "}"
        )
    );
    connect(m_quickDenoiserBtn, &QPushButton::clicked,
            this, &MainWindow::onQuickEnableDenoiser);
    quickBtnRow->addWidget(m_quickDenoiserBtn);

    m_quickDisableAllBtn = new QPushButton(
        QStringLiteral("Alle deaktivieren"), this
    );
    m_quickDisableAllBtn->setFixedHeight(32);
    m_quickDisableAllBtn->setCursor(Qt::PointingHandCursor);
    m_quickDisableAllBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: #374151;"
            "  color: #e5e7eb;"
            "  border: 1px solid #4b5563;"
            "  border-radius: 6px;"
            "  padding: 4px 16px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #4b5563;"
            "}"
        )
    );
    connect(m_quickDisableAllBtn, &QPushButton::clicked,
            this, &MainWindow::onDisableAllEffects);
    quickBtnRow->addWidget(m_quickDisableAllBtn);

    quickBtnRow->addStretch();
    quickLayout->addLayout(quickBtnRow);

    pageLayout->addWidget(quickWidget);

    // Scrollable effects area
    m_effectsScroll = new QScrollArea(this);
    m_effectsScroll->setWidgetResizable(true);
    m_effectsScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_effectsScroll->setFrameShape(QFrame::NoFrame);
    m_effectsScroll->setStyleSheet(
        QStringLiteral(
            "QScrollArea { background: transparent; border: none; }"
            "QScrollBar:vertical {"
            "  background: #0f1623;"
            "  width: 8px;"
            "  margin: 0;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:vertical {"
            "  background: #374151;"
            "  min-height: 30px;"
            "  border-radius: 4px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
            "  background: #4b5563;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "  height: 0px;"
            "}"
        )
    );

    m_effectsContainer = new QWidget(this);
    m_effectsContainer->setStyleSheet(QStringLiteral("background: transparent;"));
    m_effectsLayout = new QVBoxLayout(m_effectsContainer);
    m_effectsLayout->setContentsMargins(0, 0, 4, 0);
    m_effectsLayout->setSpacing(8);

    buildEffectCards();

    m_effectsLayout->addStretch();
    m_effectsScroll->setWidget(m_effectsContainer);

    pageLayout->addWidget(m_effectsScroll, 1);
}

void MainWindow::setupSettingsPage()
{
    m_settingsPage = new QWidget(this);
    auto *pageLayout = new QVBoxLayout(m_settingsPage);
    pageLayout->setContentsMargins(24, 20, 24, 12);
    pageLayout->setSpacing(20);

    // Page header
    auto *headerLabel = new QLabel(QStringLiteral("Einstellungen"), this);
    QFont headerFont = headerLabel->font();
    headerFont.setPointSize(18);
    headerFont.setBold(true);
    headerLabel->setFont(headerFont);
    headerLabel->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
    pageLayout->addWidget(headerLabel);

    // Settings card: Model Path
    auto *modelCard = new QWidget(this);
    modelCard->setObjectName(QStringLiteral("SettingsCard"));
    modelCard->setStyleSheet(
        QStringLiteral("QWidget#SettingsCard {"
                        "  background-color: #1f2937;"
                        "  border: 1px solid #374151;"
                        "  border-radius: 10px;"
                        "}")
    );
    auto *modelLayout = new QVBoxLayout(modelCard);
    modelLayout->setContentsMargins(18, 14, 18, 14);
    modelLayout->setSpacing(8);

    auto *modelTitle = new QLabel(QStringLiteral("Modell-Pfad"), this);
    QFont mFont = modelTitle->font();
    mFont.setPointSize(12);
    mFont.setBold(true);
    modelTitle->setFont(mFont);
    modelTitle->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    modelLayout->addWidget(modelTitle);

    m_modelPathLabel = new QLabel(QStringLiteral("/opt/maxine-afx/features"), this);
    m_modelPathLabel->setStyleSheet(
        QStringLiteral("color: #9ca3af; font-family: monospace; font-size: 11px;")
    );
    m_modelPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    modelLayout->addWidget(m_modelPathLabel);

    pageLayout->addWidget(modelCard);

    // Settings card: Sample Rate
    auto *srCard = new QWidget(this);
    srCard->setObjectName(QStringLiteral("SettingsCard2"));
    srCard->setStyleSheet(
        QStringLiteral("QWidget#SettingsCard2 {"
                        "  background-color: #1f2937;"
                        "  border: 1px solid #374151;"
                        "  border-radius: 10px;"
                        "}")
    );
    auto *srLayout = new QVBoxLayout(srCard);
    srLayout->setContentsMargins(18, 14, 18, 14);
    srLayout->setSpacing(8);

    auto *srTitle = new QLabel(QStringLiteral("Abtastrate"), this);
    srTitle->setFont(mFont);
    srTitle->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    srLayout->addWidget(srTitle);

    m_sampleRateLabel = new QLabel(QStringLiteral("48000 Hz"), this);
    m_sampleRateLabel->setStyleSheet(QStringLiteral("color: #76b900; font-size: 13px; font-weight: bold;"));
    srLayout->addWidget(m_sampleRateLabel);

    auto *srHint = new QLabel(
        QStringLiteral("Unterst\u00fctzt: 16000 Hz, 48000 Hz. \u00c4nderbar in config.toml, danach neu laden."),
        this
    );
    srHint->setStyleSheet(QStringLiteral("color: #6b7280; font-size: 10px;"));
    srHint->setWordWrap(true);
    srLayout->addWidget(srHint);

    pageLayout->addWidget(srCard);

    // Settings card: Config File
    auto *configCard = new QWidget(this);
    configCard->setObjectName(QStringLiteral("SettingsCard3"));
    configCard->setStyleSheet(
        QStringLiteral("QWidget#SettingsCard3 {"
                        "  background-color: #1f2937;"
                        "  border: 1px solid #374151;"
                        "  border-radius: 10px;"
                        "}")
    );
    auto *configLayout = new QVBoxLayout(configCard);
    configLayout->setContentsMargins(18, 14, 18, 14);
    configLayout->setSpacing(8);

    auto *configTitle = new QLabel(QStringLiteral("Konfiguration"), this);
    configTitle->setFont(mFont);
    configTitle->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    configLayout->addWidget(configTitle);

    auto *configRow = new QHBoxLayout();

    QString configPath = QDir::homePath() +
        QStringLiteral("/.config/maxine-pipewire/config.toml");
    auto *configPathLabel = new QLabel(configPath, this);
    configPathLabel->setStyleSheet(
        QStringLiteral("color: #9ca3af; font-family: monospace; font-size: 11px;")
    );
    configPathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    configRow->addWidget(configPathLabel, 1);

    auto *openConfigBtn = new QPushButton(QStringLiteral("Im Editor \u00f6ffnen"), this);
    openConfigBtn->setFixedHeight(30);
    openConfigBtn->setCursor(Qt::PointingHandCursor);
    openConfigBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: #374151;"
            "  color: #e5e7eb;"
            "  border: 1px solid #4b5563;"
            "  border-radius: 6px;"
            "  padding: 4px 16px;"
            "  font-size: 11px;"
            "}"
            "QPushButton:hover {"
            "  background-color: #4b5563;"
            "}"
        )
    );
    connect(openConfigBtn, &QPushButton::clicked, this, [configPath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(configPath));
    });
    configRow->addWidget(openConfigBtn);

    auto *reloadBtn = new QPushButton(QStringLiteral("Konfig neu laden"), this);
    reloadBtn->setFixedHeight(30);
    reloadBtn->setCursor(Qt::PointingHandCursor);
    reloadBtn->setStyleSheet(
        QStringLiteral(
            "QPushButton {"
            "  background-color: #1a3a0a;"
            "  color: #76b900;"
            "  border: 1px solid #76b900;"
            "  border-radius: 6px;"
            "  padding: 4px 16px;"
            "  font-size: 11px;"
            "  font-weight: bold;"
            "}"
            "QPushButton:hover {"
            "  background-color: #2a4a1a;"
            "}"
        )
    );
    connect(reloadBtn, &QPushButton::clicked, this, [this]() {
        m_dbus->reloadConfigAsync();
    });
    configRow->addWidget(reloadBtn);

    configLayout->addLayout(configRow);

    pageLayout->addWidget(configCard);

    // About section
    auto *aboutCard = new QWidget(this);
    aboutCard->setObjectName(QStringLiteral("SettingsCard4"));
    aboutCard->setStyleSheet(
        QStringLiteral("QWidget#SettingsCard4 {"
                        "  background-color: #1f2937;"
                        "  border: 1px solid #374151;"
                        "  border-radius: 10px;"
                        "}")
    );
    auto *aboutLayout = new QVBoxLayout(aboutCard);
    aboutLayout->setContentsMargins(18, 14, 18, 14);
    aboutLayout->setSpacing(6);

    auto *aboutTitle = new QLabel(QStringLiteral("\u00dcber"), this);
    aboutTitle->setFont(mFont);
    aboutTitle->setStyleSheet(QStringLiteral("color: #d1d5db;"));
    aboutLayout->addWidget(aboutTitle);

    auto *aboutText = new QLabel(
        QStringLiteral("Maxine PipeWire v0.1.0\n"
                        "NVIDIA Broadcast f\u00fcr Linux\n\n"
                        "GPU-beschleunigte Audioeffekte \u00fcber das NVIDIA Maxine AFX SDK\n"
                        "mit nativer PipeWire-Integration.\n\n"
                        "Lizenz: MIT"),
        this
    );
    aboutText->setStyleSheet(QStringLiteral("color: #9ca3af; font-size: 11px;"));
    aboutText->setWordWrap(true);
    aboutLayout->addWidget(aboutText);

    pageLayout->addWidget(aboutCard);

    pageLayout->addStretch();
}

void MainWindow::setupDisconnectedPage()
{
    // The overlay is set up in setupUi already
}

void MainWindow::buildEffectCards()
{
    // Two groups: Grundlagen (basics) and Erweitert (advanced)
    struct CategoryGroup {
        QString category;
        QString label;
        QString description;
    };
    QVector<CategoryGroup> groups = {
        {QStringLiteral("basics"),
         QStringLiteral("Grundlagen"),
         QStringLiteral("Diese Effekte verbessern die Audioqualit\u00e4t f\u00fcr die meisten Anwendungsf\u00e4lle.")},
        {QStringLiteral("advanced"),
         QStringLiteral("Erweitert"),
         QStringLiteral("Spezialeffekte f\u00fcr professionelle Nutzung. K\u00f6nnen die Latenz erh\u00f6hen.")},
    };

    for (const auto &group : groups) {
        // Category header
        auto *catLabel = new QLabel(group.label, this);
        QFont catFont = catLabel->font();
        catFont.setPointSize(12);
        catFont.setBold(true);
        catLabel->setFont(catFont);
        QColor catColor = EffectCard::categoryColor(group.category);
        catLabel->setStyleSheet(
            QString("color: %1; margin-top: 12px; margin-bottom: 0px;")
            .arg(catColor.name())
        );
        m_effectsLayout->addWidget(catLabel);

        // Category description
        auto *catDesc = new QLabel(group.description, this);
        QFont catDescFont = catDesc->font();
        catDescFont.setPointSize(9);
        catDesc->setFont(catDescFont);
        catDesc->setStyleSheet(QStringLiteral("color: #6b7280; margin-bottom: 4px;"));
        catDesc->setWordWrap(true);
        m_effectsLayout->addWidget(catDesc);

        // Add cards for this category
        for (const auto &entry : m_catalog) {
            if (entry.category != group.category)
                continue;

            auto *card = new EffectCard(entry, this);
            m_effectCards.insert(entry.id, card);
            m_effectsLayout->addWidget(card);

            connect(card, &EffectCard::enableChanged,
                    this, &MainWindow::onEffectEnableChanged);
            connect(card, &EffectCard::intensityChanged,
                    this, &MainWindow::onEffectIntensityChanged);
        }
    }
}

void MainWindow::setupSystemTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_trayMenu = new QMenu(this);

    auto *showAction = m_trayMenu->addAction(QStringLiteral("Fenster anzeigen"));
    connect(showAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });

    m_trayMenu->addSeparator();

    // Quick toggles for common effects
    auto *denoiserAction = m_trayMenu->addAction(QStringLiteral("Rauschunterdr\u00fcckung umschalten"));
    connect(denoiserAction, &QAction::triggered, this, [this]() {
        auto it = m_effectCards.find(QStringLiteral("denoiser"));
        if (it != m_effectCards.end()) {
            bool newState = !it.value()->isEffectEnabled();
            if (newState)
                m_dbus->enableEffectAsync(QStringLiteral("denoiser"));
            else
                m_dbus->disableEffectAsync(QStringLiteral("denoiser"));
        }
    });

    auto *dereverbAction = m_trayMenu->addAction(QStringLiteral("Hall-Entfernung umschalten"));
    connect(dereverbAction, &QAction::triggered, this, [this]() {
        auto it = m_effectCards.find(QStringLiteral("dereverb"));
        if (it != m_effectCards.end()) {
            bool newState = !it.value()->isEffectEnabled();
            if (newState)
                m_dbus->enableEffectAsync(QStringLiteral("dereverb"));
            else
                m_dbus->disableEffectAsync(QStringLiteral("dereverb"));
        }
    });

    m_trayMenu->addSeparator();

    auto *quitAction = m_trayMenu->addAction(QStringLiteral("Beenden"));
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip(QStringLiteral("Maxine PipeWire \u2014 NVIDIA Audio-Effekte"));

    // Use a simple generated pixmap as icon since we don't have an icon file yet
    QPixmap iconPix(32, 32);
    iconPix.fill(Qt::transparent);
    {
        QPainter p(&iconPix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x76, 0xb9, 0x00));
        p.drawRoundedRect(2, 2, 28, 28, 6, 6);
        p.setPen(QPen(Qt::white, 2));
        QFont f;
        f.setPixelSize(18);
        f.setBold(true);
        p.setFont(f);
        p.drawText(iconPix.rect(), Qt::AlignCenter, QStringLiteral("M"));
    }
    m_trayIcon->setIcon(QIcon(iconPix));

    connect(m_trayIcon, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger ||
                reason == QSystemTrayIcon::DoubleClick) {
                if (isVisible()) {
                    hide();
                } else {
                    show();
                    raise();
                    activateWindow();
                }
            }
        }
    );

    m_trayIcon->show();
}

void MainWindow::showDisconnectedOverlay(bool show)
{
    if (show) {
        m_disconnectedOverlay->setGeometry(m_centralWidget->rect());
        m_disconnectedOverlay->raise();
        m_disconnectedOverlay->show();
    } else {
        m_disconnectedOverlay->hide();
    }
}

void MainWindow::onSidebarChanged(int currentRow)
{
    // row 0 = Audio, row 1 = Video (disabled), row 2 = Settings
    if (currentRow == 0) {
        m_pages->setCurrentWidget(m_audioPage);
    } else if (currentRow == 2) {
        m_pages->setCurrentWidget(m_settingsPage);
    }
    // Video (row 1) is disabled and can't be selected
}

void MainWindow::onDaemonConnected()
{
    showDisconnectedOverlay(false);
    m_statusBar->setConnected(true);

    // Fetch devices
    QVector<DeviceInfo> devices = m_dbus->listDevices();
    onDevicesUpdated(devices);
}

void MainWindow::onDaemonDisconnected()
{
    showDisconnectedOverlay(true);
    m_statusBar->setConnected(false);
}

void MainWindow::onEffectsUpdated(const QVector<EffectInfo> &effects)
{
    // Update existing cards with daemon state
    // Build a map for quick lookup
    QMap<QString, EffectInfo> effectMap;
    for (const auto &e : effects) {
        effectMap.insert(e.id, e);
    }

    for (auto it = m_effectCards.begin(); it != m_effectCards.end(); ++it) {
        EffectCard *card = it.value();
        auto eit = effectMap.find(it.key());
        if (eit != effectMap.end()) {
            // Block signals to avoid feedback loops
            card->blockSignals(true);
            card->setEnabled(eit->enabled);
            card->setIntensity(eit->intensity);
            card->blockSignals(false);
        }
    }
}

void MainWindow::onStatusUpdated(const DaemonStatus &status)
{
    m_statusBar->updateStatus(status);

    // Update settings page
    if (!status.sampleRate.isEmpty()) {
        m_sampleRateLabel->setText(status.sampleRate + QStringLiteral(" Hz"));
    }
}

void MainWindow::onDevicesUpdated(const QVector<DeviceInfo> &devices)
{
    m_deviceCombo->clear();
    if (devices.isEmpty()) {
        m_deviceCombo->addItem(QStringLiteral("Keine Ger\u00e4te gefunden"));
        return;
    }
    for (const auto &dev : devices) {
        QString display = dev.name;
        if (!dev.type.isEmpty())
            display += QStringLiteral(" [") + dev.type + QStringLiteral("]");
        m_deviceCombo->addItem(display, dev.id);
    }
}

void MainWindow::onEffectEnableChanged(const QString &id, bool enabled)
{
    if (enabled)
        m_dbus->enableEffectAsync(id);
    else
        m_dbus->disableEffectAsync(id);
}

void MainWindow::onEffectIntensityChanged(const QString &id, double value)
{
    m_dbus->setIntensityAsync(id, value);
}

void MainWindow::onQuickEnableDenoiser()
{
    m_dbus->enableEffectAsync(QStringLiteral("denoiser"));
}

void MainWindow::onDisableAllEffects()
{
    for (auto it = m_effectCards.begin(); it != m_effectCards.end(); ++it) {
        if (it.value()->isEffectEnabled()) {
            m_dbus->disableEffectAsync(it.key());
        }
    }
}
