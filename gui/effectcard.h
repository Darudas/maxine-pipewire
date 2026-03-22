#pragma once

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QString>

class ToggleSwitch : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool checked READ isChecked WRITE setChecked NOTIFY toggled)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    bool isChecked() const;
    void setChecked(bool checked);

    QSize sizeHint() const override;

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool m_checked;
};

struct EffectCatalogEntry {
    QString id;
    QString displayName;
    QString description;
    QString category;       // "basics", "advanced"
    bool hasIntensity;
    QString detailedHelp;   // longer explanation shown in tooltip
    QString usageHint;      // "Empfohlen fuer: ..." one-liner
    bool recommended;       // show a "Empfohlen" badge
};

class EffectCard : public QWidget
{
    Q_OBJECT

public:
    explicit EffectCard(const EffectCatalogEntry &entry, QWidget *parent = nullptr);

    QString effectId() const;
    void setEnabled(bool enabled);
    void setIntensity(double intensity);
    bool isEffectEnabled() const;
    double intensity() const;

    static QColor categoryColor(const QString &category);
    static QString categoryLabel(const QString &category);

signals:
    void enableChanged(const QString &id, bool enabled);
    void intensityChanged(const QString &id, double value);

private:
    void updateStyle();

    EffectCatalogEntry m_entry;
    ToggleSwitch *m_toggle;
    QLabel *m_nameLabel;
    QLabel *m_descLabel;
    QLabel *m_hintLabel;
    QLabel *m_recommendedBadge;
    QSlider *m_slider;
    QLabel *m_sliderLabel;
    QLabel *m_sliderValue;
    QLabel *m_sliderRangeLabel;
    QWidget *m_sliderContainer;
    bool m_effectEnabled;
    double m_intensity;
};
