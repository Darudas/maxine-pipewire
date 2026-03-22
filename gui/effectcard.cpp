#include "effectcard.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFont>
#include <QGraphicsDropShadowEffect>

// ---- ToggleSwitch ----

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QWidget(parent)
    , m_checked(false)
{
    setFixedSize(48, 26);
    setCursor(Qt::PointingHandCursor);
}

bool ToggleSwitch::isChecked() const
{
    return m_checked;
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked != checked) {
        m_checked = checked;
        update();
        emit toggled(m_checked);
    }
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(48, 26);
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();
    int radius = h / 2;

    // Track
    QColor trackColor = m_checked ? QColor(0x76, 0xb9, 0x00) : QColor(0x4b, 0x55, 0x63);
    p.setPen(Qt::NoPen);
    p.setBrush(trackColor);
    p.drawRoundedRect(0, 0, w, h, radius, radius);

    // Thumb
    int thumbDiameter = h - 6;
    int thumbX = m_checked ? (w - thumbDiameter - 3) : 3;
    int thumbY = 3;

    p.setBrush(QColor(0xf9, 0xfa, 0xfb));
    p.drawEllipse(thumbX, thumbY, thumbDiameter, thumbDiameter);
}

void ToggleSwitch::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_checked = !m_checked;
        update();
        emit toggled(m_checked);
    }
}

// ---- EffectCard ----

EffectCard::EffectCard(const EffectCatalogEntry &entry, QWidget *parent)
    : QWidget(parent)
    , m_entry(entry)
    , m_effectEnabled(false)
    , m_intensity(1.0)
{
    setObjectName(QStringLiteral("EffectCard"));

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(14);

    // Toggle switch
    m_toggle = new ToggleSwitch(this);
    mainLayout->addWidget(m_toggle, 0, Qt::AlignVCenter);

    // Center: name + description + category badge
    auto *centerLayout = new QVBoxLayout();
    centerLayout->setSpacing(3);

    // Top row: name + category badge
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(10);

    m_nameLabel = new QLabel(entry.displayName, this);
    m_nameLabel->setObjectName(QStringLiteral("EffectName"));
    QFont nameFont = m_nameLabel->font();
    nameFont.setPointSize(13);
    nameFont.setBold(true);
    m_nameLabel->setFont(nameFont);
    topRow->addWidget(m_nameLabel);

    m_categoryBadge = new QLabel(categoryLabel(entry.category), this);
    m_categoryBadge->setObjectName(QStringLiteral("CategoryBadge"));
    QColor catColor = categoryColor(entry.category);
    m_categoryBadge->setStyleSheet(
        QString("QLabel#CategoryBadge {"
                "  background-color: %1;"
                "  color: #ffffff;"
                "  border-radius: 8px;"
                "  padding: 2px 10px;"
                "  font-size: 10px;"
                "  font-weight: bold;"
                "}")
        .arg(catColor.darker(130).name())
    );
    m_categoryBadge->setFixedHeight(20);
    topRow->addWidget(m_categoryBadge, 0, Qt::AlignVCenter);

    topRow->addStretch();
    centerLayout->addLayout(topRow);

    m_descLabel = new QLabel(entry.description, this);
    m_descLabel->setObjectName(QStringLiteral("EffectDesc"));
    QFont descFont = m_descLabel->font();
    descFont.setPointSize(10);
    m_descLabel->setFont(descFont);
    m_descLabel->setWordWrap(true);
    centerLayout->addWidget(m_descLabel);

    mainLayout->addLayout(centerLayout, 1);

    // Right: intensity slider (if applicable)
    m_sliderContainer = new QWidget(this);
    auto *sliderLayout = new QVBoxLayout(m_sliderContainer);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    sliderLayout->setSpacing(2);

    m_sliderValue = new QLabel(QStringLiteral("100%"), this);
    m_sliderValue->setObjectName(QStringLiteral("SliderValue"));
    m_sliderValue->setAlignment(Qt::AlignCenter);
    QFont valFont = m_sliderValue->font();
    valFont.setPointSize(10);
    valFont.setBold(true);
    m_sliderValue->setFont(valFont);
    sliderLayout->addWidget(m_sliderValue);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setObjectName(QStringLiteral("IntensitySlider"));
    m_slider->setRange(0, 100);
    m_slider->setValue(100);
    m_slider->setFixedWidth(140);
    sliderLayout->addWidget(m_slider);

    mainLayout->addWidget(m_sliderContainer, 0, Qt::AlignVCenter);

    if (!entry.hasIntensity) {
        m_sliderContainer->setVisible(false);
    }

    // Connections
    connect(m_toggle, &ToggleSwitch::toggled, this, [this](bool checked) {
        m_effectEnabled = checked;
        updateStyle();
        emit enableChanged(m_entry.id, checked);
    });

    connect(m_slider, &QSlider::valueChanged, this, [this](int value) {
        m_intensity = value / 100.0;
        m_sliderValue->setText(QString::number(value) + QStringLiteral("%"));
        emit intensityChanged(m_entry.id, m_intensity);
    });

    setMinimumHeight(72);
    updateStyle();
}

QString EffectCard::effectId() const
{
    return m_entry.id;
}

void EffectCard::setEnabled(bool enabled)
{
    m_effectEnabled = enabled;
    m_toggle->setChecked(enabled);
    updateStyle();
}

void EffectCard::setIntensity(double intensity)
{
    m_intensity = intensity;
    m_slider->blockSignals(true);
    m_slider->setValue(static_cast<int>(intensity * 100));
    m_slider->blockSignals(false);
    m_sliderValue->setText(QString::number(static_cast<int>(intensity * 100)) + QStringLiteral("%"));
}

bool EffectCard::isEffectEnabled() const
{
    return m_effectEnabled;
}

double EffectCard::intensity() const
{
    return m_intensity;
}

QColor EffectCard::categoryColor(const QString &category)
{
    if (category == QLatin1String("noise"))
        return QColor(0x3b, 0x82, 0xf6);   // Blue
    if (category == QLatin1String("echo"))
        return QColor(0xf9, 0x73, 0x16);   // Orange
    if (category == QLatin1String("enhancement"))
        return QColor(0xa8, 0x55, 0xf7);   // Purple
    if (category == QLatin1String("voice"))
        return QColor(0x14, 0xb8, 0xa6);   // Teal
    return QColor(0x6b, 0x72, 0x80);        // Grey fallback
}

QString EffectCard::categoryLabel(const QString &category)
{
    if (category == QLatin1String("noise"))
        return QStringLiteral("Noise");
    if (category == QLatin1String("echo"))
        return QStringLiteral("Echo");
    if (category == QLatin1String("enhancement"))
        return QStringLiteral("Enhancement");
    if (category == QLatin1String("voice"))
        return QStringLiteral("Voice");
    return category;
}

void EffectCard::updateStyle()
{
    QColor borderColor;
    QColor bgColor;

    if (m_effectEnabled) {
        QColor accent = categoryColor(m_entry.category);
        borderColor = accent;
        bgColor = QColor(0x1f, 0x29, 0x37);
        m_nameLabel->setStyleSheet(QStringLiteral("color: #e5e7eb;"));
        m_descLabel->setStyleSheet(QStringLiteral("color: #9ca3af;"));
        m_sliderValue->setStyleSheet(QStringLiteral("color: #76b900;"));
        m_slider->setEnabled(true);
    } else {
        borderColor = QColor(0x37, 0x41, 0x51);
        bgColor = QColor(0x1a, 0x1f, 0x2b);
        m_nameLabel->setStyleSheet(QStringLiteral("color: #6b7280;"));
        m_descLabel->setStyleSheet(QStringLiteral("color: #4b5563;"));
        m_sliderValue->setStyleSheet(QStringLiteral("color: #4b5563;"));
        m_slider->setEnabled(false);
    }

    setStyleSheet(
        QString(
            "QWidget#EffectCard {"
            "  background-color: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 10px;"
            "}"
        )
        .arg(bgColor.name(), borderColor.name())
    );
}
