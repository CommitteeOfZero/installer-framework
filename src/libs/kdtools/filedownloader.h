/* Copyright C 2013 Klaralvdalens Datakonsult AB KDAB
 * Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef FILEDOWNLOADER_H
#define FILEDOWNLOADER_H

#include "kdtoolsglobal.h"
#include "filedownloaderfactory.h"
#include "abstractfiletask.h"
#include "packagemanagercore.h"
#include "filedownloadrequest.h"
#include "downloadfiletask.h"
#include "copyfiletask.h"

#include <memory>
#include <QtCore/QObject>
#include <QtCore/QUrl>

#include <QtNetwork/QAuthenticator>
#include <QFutureWatcher>

using namespace QInstaller;

namespace KDUpdater {

class FileDownloaderProxyFactory;

class KDTOOLS_EXPORT FileDownloader : public QObject
{
    Q_OBJECT

public:
    explicit FileDownloader(QObject *parent = nullptr);

    void resetRequests();
    void addRequest(const FileDownloadRequest &request);
    void addRequests(const QList<FileDownloadRequest> &requests);
    QList<FileDownloadRequest> requests() const;
    QList<FileDownloadRequest> takeNextChunk(FileDownloadRequest::Role role,
                                             FileDownloadRequest::TransferMethod transferMethod);

    void setPackageManagerCore(PackageManagerCore *core);

    void download();
    void setupFileTask(AbstractFileTask *const task,
                       FileDownloadRequest::Role role,
                       FileDownloadRequest::TransferMethod transferMethod);

    void resetTasks();

    FileDownloaderProxyFactory *proxyFactory() const;
    void setProxyFactory(FileDownloaderProxyFactory *factory);

    quint64 bytesReceived() const;

    bool dataDownloaded() const;
    void setDataDownloaded(bool downloaded);
    bool sha1Downloaded() const;
    void setSha1Downloaded(bool downloaded);

    void reset();

Q_SIGNALS:
    void downloadCompleted();
    void downloadAborted(const JobError error, const QString &errorStr);
    void registerFile(const QInstaller::FileTaskItem &item);
    void setProcessedAmount();
    void fileDownloaded(const QString &fileName, const QString &componentName);
    void networkDisconnected();
    void sha1DownloadFinished();
    void retryFileDownload(const QString &fileName);

public Q_SLOT:
    void setProgress(quint64 bytesReceived);

protected:
    void setDownloadCompleted();
    void setDownloadAborted(const JobError error, const QString &errorStr);
    bool isDownloadAborted() const;

    DownloadFileTask* createNetworkTask(const QList<FileDownloadRequest>& requests);
    CopyFileTask* createLocalTask(const QList<FileDownloadRequest>& requests);

private Q_SLOTS:
    bool startStage(FileDownloadRequest::Role role);
    void onTaskFinished(QFutureWatcher<FileTaskResult> *watcher,
                        FileDownloadRequest::Role role);
    void shaDownloadFinished();
    void archiveDownloadFinished();

private:
    struct Private;
    struct PrivatePtr: public std::unique_ptr<Private> 
    {
        using std::unique_ptr<Private>::unique_ptr;
        ~PrivatePtr();
    } d;
};

} // namespace KDUpdater

#endif // FILEDOWNLOADER_H