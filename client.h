//client.h
#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>
#include <QDebug>

#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QJsonArray>

#include <QFileSystemWatcher>

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(const QUrl &serverUrl, bool debug = false, QObject *parent = nullptr);
    ~Client();
    void sendDirectoryStructure(const QString &path);

private:
    QTimer m_reconnectTimer;
    int m_reconnectAttempts = 0;
    const int MAX_RECONNECT_ATTEMPTS = -1; // Переподключаться - бесконечно [-1] / никогда [-2] / несколько раз [1 ... n]
    const int WAITING_TIME = 2000; // Мс между попытками
    void tryReconnect();
    QJsonObject scanDirectory(const QString &path);

signals:
    void connected();
    void messageReceived(const QString &message);

public slots:
    void sendMessage(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString &message);
    void onErrorOccurred(QAbstractSocket::SocketError error);

private:
    QWebSocket m_webSocket;
    QUrl m_serverUrl;
    bool m_debug;
    QFileSystemWatcher m_watcher;
    void addPathRecursive(const QString &path);
};

#endif // CLIENT_H
