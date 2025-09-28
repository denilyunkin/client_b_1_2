//client.cpp
#include "client.h"

Client::Client(const QUrl &serverUrl, bool debug, QObject *parent) :
    QObject(parent), m_serverUrl(serverUrl), m_debug(debug)
{
    if (m_debug)
        qDebug() << "\nПодключение к серверу:" << m_serverUrl.toString();

    if (!m_serverUrl.isValid() || m_serverUrl.scheme() != "ws") {
        qWarning() << "Неверный URL сервера:" << m_serverUrl.toString();
        return;
    }

    connect(&m_webSocket, &QWebSocket::connected, this, &Client::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &Client::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &Client::onTextMessageReceived);
    connect(&m_webSocket, &QWebSocket::errorOccurred, this, &Client::onErrorOccurred);

    connect(&m_reconnectTimer, &QTimer::timeout, this, &Client::tryReconnect);
    m_reconnectTimer.setInterval(WAITING_TIME);
    m_reconnectTimer.setSingleShot(false);

    m_webSocket.open(m_serverUrl);
}

Client::~Client()
{
    if (m_webSocket.state() == QAbstractSocket::ConnectedState)
        m_webSocket.close();
}

void Client::onConnected()
{
    if (m_debug)
    {
        qDebug() << "Успешное подключение к серверу!";
        qDebug() << "Клиентский порт:" << m_webSocket.localPort();
    }

    emit connected();

    //if (m_debug) m_webSocket.sendTextMessage("Hello, EchoServer!");

    // ОТПРАВКА ДАННЫХ
    sendDirectoryStructure("C:/logs");
    addPathRecursive("C:/logs");

    // connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
    //         this, [this](const QString &path) {
    //             qDebug() << "Изменилась директория:" << path;
    //             sendDirectoryStructure(path); // отправляем только изменившуюся часть
    //         });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &path) {
                Q_UNUSED(path)
                sendDirectoryStructure("C:/logs"); // всегда полный снимок
            });

    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
                qDebug() << "Изменился файл:" << path;
                QJsonObject fileInfo;
                QFileInfo fi(path);
                fileInfo["name"] = fi.fileName();
                fileInfo["size"] = fi.size();
                fileInfo["lastModified"] = fi.lastModified().toString(Qt::ISODate);

                QJsonDocument doc(fileInfo);
                m_webSocket.sendTextMessage(doc.toJson(QJsonDocument::Compact));
            });
}

void Client::tryReconnect()
{
    m_reconnectTimer.stop();
    if (m_webSocket.state() != QAbstractSocket::ConnectedState)
    {
        if (MAX_RECONNECT_ATTEMPTS == -1 || m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS)
        {
            m_reconnectAttempts++;
            qDebug() << "\nПопытка переподключения" << m_reconnectAttempts << "/" << MAX_RECONNECT_ATTEMPTS;
            m_webSocket.abort();
            m_webSocket.open(m_serverUrl);
        } else {
            qCritical() << "\nПревышено максимальное число попыток подключения";
            m_reconnectTimer.stop();
            //QCoreApplication::quit();
        }
    }
}

void Client::onDisconnected()
{
    if (m_debug)
        qDebug() << "Соединение разорвано!";

    m_reconnectTimer.start();
}





void Client::onTextMessageReceived(const QString &message)
{
    if (m_debug)
        qDebug() << "Получено сообщение:" << message;

    emit messageReceived(message);
}

void Client::onErrorOccurred(QAbstractSocket::SocketError error)
{
    qWarning() << "Ошибка сокета:" << error << "-" << m_webSocket.errorString();
}

void Client::sendMessage(const QString &message) // Ненужный слот?
{
    if (m_webSocket.state() == QAbstractSocket::ConnectedState)
        m_webSocket.sendTextMessage(message);
    else
        qWarning() << "Не удалось отправить сообщение: соединение не установлено";
}




QJsonObject Client::scanDirectory(const QString &path) {
    QJsonObject dirObject;
    QDir dir(path);

    if (!dir.exists()) {
        qWarning() << "Директория не существует:" << path;
        return dirObject;
    }

    dirObject["name"] = dir.dirName();
    dirObject["path"] = dir.absolutePath();

    QJsonArray filesArray;
    QJsonArray foldersArray;

    // Настройка фильтра и сортировки
    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot;
    QDir::SortFlags sorting = QDir::DirsFirst | QDir::Name;

    for (const QFileInfo &entry : dir.entryInfoList(filters, sorting)) {
        if (entry.isDir()) {
            foldersArray.append(scanDirectory(entry.absoluteFilePath()));
        } else {
            QJsonObject fileObject;
            fileObject["name"] = entry.fileName();
            fileObject["size"] = entry.size();
            fileObject["lastModified"] = entry.lastModified().toString(Qt::ISODate);
            filesArray.append(fileObject);
        }
    }

    if (!foldersArray.isEmpty())
        dirObject["folders"] = foldersArray;

    if (!filesArray.isEmpty())
        dirObject["files"] = filesArray;

    if (m_debug) {
        //qDebug() << "scanDirectory возвращает:" << dirObject;
        qDebug() << "Вложенная структура директории скопирована.";
    }

    return dirObject;
}

void Client::sendDirectoryStructure(const QString &path) {
    if (m_webSocket.state() != QAbstractSocket::ConnectedState) {
        qWarning() << "Не подключен к серверу";
        return;
    }

    QJsonObject dirStructure = scanDirectory(path);
    QJsonDocument doc(dirStructure);
    m_webSocket.sendTextMessage(doc.toJson(QJsonDocument::Compact));

    // if (m_debug) {
    //     //qDebug() << "Отправляемая структура:\n" << doc.toJson(QJsonDocument::Indented);
    //     qDebug() << "Структура директории отправлена.";
    // }

    if (m_debug) {
        qDebug().noquote() << "Отправляемая структура:\n"
                           << doc.toJson(QJsonDocument::Indented);
    }
}


// Контроль за директорией
void Client::addPathRecursive(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) return;

    m_watcher.addPath(path); // следим за директорией

    for (const QFileInfo &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        addPathRecursive(entry.absoluteFilePath()); // рекурсивно
    }
    for (const QFileInfo &entry : dir.entryInfoList(QDir::Files)) {
        m_watcher.addPath(entry.absoluteFilePath()); // следим за файлами
    }
}


