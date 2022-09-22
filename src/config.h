#ifndef CONFIG_H
#define CONFIG_H

#include <QSettings>
#include <QObject>

class Config : public QObject
{
    Q_OBJECT

public:
    explicit Config(QObject *parent = nullptr);

    QString port;
    QString address;
    QString url;
    QString host;
    QStringList sources;
    bool strict;
    bool startup;
    QString theme;

    void readSettings();
    void writeSettings();

private:
    QSettings *settings;
};

#endif // CONFIG_H
