#pragma once
#include <QObject>
#include <QSettings>

class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString shape READ shape WRITE setShape NOTIFY shapeChanged)

public:
    explicit Settings(QObject *parent = nullptr);

    QString theme() const { return m_theme; }
    QString shape() const { return m_shape; }

    Q_INVOKABLE void setTheme(const QString &v);
    Q_INVOKABLE void setShape(const QString &v);

signals:
    void themeChanged();
    void shapeChanged();

private:
    static bool validTheme(const QString &v);
    static bool validShape(const QString &v);

    QSettings m_store;
    QString   m_theme;
    QString   m_shape;
};
