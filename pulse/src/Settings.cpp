#include "Settings.h"

static const QStringList kThemes = {QStringLiteral("midnight"),
                                     QStringLiteral("aurora"),
                                     QStringLiteral("carbon")};
static const QStringList kShapes = {QStringLiteral("round"),
                                     QStringLiteral("sharp"),
                                     QStringLiteral("pill")};

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_store(QStringLiteral("Pulse"), QStringLiteral("Pulse"))
{
    m_theme = m_store.value(QStringLiteral("theme"), QStringLiteral("midnight")).toString();
    m_shape = m_store.value(QStringLiteral("shape"), QStringLiteral("round")).toString();
    if (!validTheme(m_theme)) m_theme = QStringLiteral("midnight");
    if (!validShape(m_shape)) m_shape = QStringLiteral("round");
}

void Settings::setTheme(const QString &v)
{
    if (!validTheme(v) || v == m_theme) return;
    m_theme = v;
    m_store.setValue(QStringLiteral("theme"), v);
    emit themeChanged();
}

void Settings::setShape(const QString &v)
{
    if (!validShape(v) || v == m_shape) return;
    m_shape = v;
    m_store.setValue(QStringLiteral("shape"), v);
    emit shapeChanged();
}

bool Settings::validTheme(const QString &v) { return kThemes.contains(v); }
bool Settings::validShape(const QString &v) { return kShapes.contains(v); }
