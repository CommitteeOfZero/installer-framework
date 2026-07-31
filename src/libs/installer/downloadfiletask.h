/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef DOWNLOADFILETASK_H
#define DOWNLOADFILETASK_H

#include "abstractfiletask.h"
#include "filedownloaderfactory.h"

#include <QAuthenticator>
Q_DECLARE_METATYPE(QAuthenticator)

namespace QInstaller {

namespace TaskRole {
enum
{
    Authenticator = TaskRole::TargetFile + 10
};
}

class MetaDownloadException : public QException
{
public:
    MetaDownloadException() {}
    ~MetaDownloadException() throw() {}
    explicit MetaDownloadException(const QString &message)
        : m_message(message)
    {}

    void raise() const override { throw *this; }
    QString message() const { return m_message; }
    MetaDownloadException *clone() const override{ return new MetaDownloadException(*this); }

private:
    QString m_message;
};

class AuthenticationRequiredException : public TaskException
{
public:
    enum struct Type {
        Proxy,
        Server
    };

    explicit AuthenticationRequiredException(Type type, const QString &message);
    ~AuthenticationRequiredException() throw() {}

    Type type() const { return m_type; }

    QNetworkProxy proxy() const { return m_proxy; }
    void setProxy(const QNetworkProxy &proxy) { m_proxy = proxy; }

    FileTaskItem taskItem() const { return m_fileTaskItem; }
    void setFileTaskItem(const FileTaskItem &item) { m_fileTaskItem = item; }

    void raise() const override { throw *this; }
    AuthenticationRequiredException *clone() const override {
        return new AuthenticationRequiredException(*this); }

private:
    Type m_type;
    QNetworkProxy m_proxy;
    FileTaskItem m_fileTaskItem;
};

class INSTALLER_EXPORT DownloadFileTask : public AbstractFileTask
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadFileTask)

public:
    DownloadFileTask() {}
    explicit DownloadFileTask(const FileTaskItem &item)
        : AbstractFileTask(item) {}
    explicit DownloadFileTask(const QList<FileTaskItem> &items);

    explicit DownloadFileTask(const QString &source)
        : AbstractFileTask(source) {}
    DownloadFileTask(const QString &source, const QString &target)
        : AbstractFileTask(source, target) {}

    void addTaskItem(const FileTaskItem &items);
    void addTaskItems(const QList<FileTaskItem> &items);

    void setTaskItem(const FileTaskItem &items);
    void setTaskItems(const QList<FileTaskItem> &items);

    void setAuthenticator(const QAuthenticator &authenticator);
    void setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory);

    void doTask(QFutureInterface<FileTaskResult> &fi) override;

Q_SIGNALS:
    void networkDisconnected();

private:
    friend class Downloader;
    QAuthenticator m_authenticator;
    QScopedPointer<KDUpdater::FileDownloaderProxyFactory> m_proxyFactory;
};

}   // namespace QInstaller

#endif // DOWNLOADFILETASK_H
