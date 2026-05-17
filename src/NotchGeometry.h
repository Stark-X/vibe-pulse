#pragma once
#include <QObject>
#include <QVariantList>

// Per-screen HUD position data, exposed to C++ for window placement.
struct ScreenHudPos {
    QString screenName;   // QScreen::name() for matching
    qreal leftX   = 0;    // X position for left HUD (agent count)
    qreal rightX  = 0;    // X position for right HUD (usage gauge)
    qreal y       = 0;    // Y position for both HUDs
    bool  hasNotch = false;
};

class NotchGeometry : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool         available   READ available   NOTIFY geometryChanged)
    Q_PROPERTY(int          screenCount READ screenCount NOTIFY geometryChanged)
    Q_PROPERTY(QVariantList screenPositions READ screenPositions NOTIFY geometryChanged)

public:
    explicit NotchGeometry(QObject *parent = nullptr);

    bool         available()       const { return m_available; }
    int          screenCount()     const { return m_screenPositions.size(); }
    QVariantList screenPositions() const;

    Q_INVOKABLE void refresh();

    // C++ access (avoids QVariant overhead in main.cpp)
    const QVector<ScreenHudPos> &positions() const { return m_screenPositions; }

signals:
    void geometryChanged();

private:
    void compute();

    bool                   m_available = false;
    QVector<ScreenHudPos>  m_screenPositions;
};
