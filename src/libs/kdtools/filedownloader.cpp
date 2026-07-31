/* Copyright C 2013 Klaralvdalens Datakonsult AB KDAB
 * Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#include "filedownloader.h"
#include "filedownloaderfactory.h"
#include "filedownloadrequest.h"

#include "downloadfiletask.h"
#include "copyfiletask.h"
#include "qinstallerglobal.h"
#include "packagemanagerproxyfactory.h"

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxyFactory>
#include <QPointer>
#include <QUrl>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QThreadPool>
#include <QDebug>
#include <QSslError>
#include <QBasicTimer>
#include <QTimerEvent>
#include <QLoggingCategory>
#include <globals.h>
#include <QHostInfo>
#include <QFutureWatcher>
#include <QtCore/QHash>
#include <QtConcurrent/QtConcurrentRun>

using namespace KDUpdater;
using namespace QInstaller;

static constexpr uint scMaxRetries = 3;

// -- KDUpdater::FileDownloader

/*!
    \inmodule kdupdater
    \class KDUpdater::FileDownloader
    \internal
*/
struct KDUpdater::FileDownloader::Private
{
    Private()
        : m_core(nullptr)
        , m_factory(nullptr)
        , m_bytesReceived(0)
        , m_allBytesReceived(0)
        , m_dataDownloaded(false)
        , m_sha1Downloaded(false)
        , m_aborted(false)
        , m_retryCount(scMaxRetries)
        , m_downloadableChunkSize(100)
    {
        const QByteArray chunkSizeEnv = qgetenv("IFW_DOWNLOAD_SIZE");
        if (!chunkSizeEnv.isEmpty()) {
            const int chunkSize = QString::fromLocal8Bit(chunkSizeEnv).toInt();
            if (chunkSize > 0)
                m_downloadableChunkSize = chunkSize;
        }
    }

    ~Private()
    {
        delete m_factory;
        qDeleteAll(m_activeWatchers);
    }

    QString scheme;

    PackageManagerCore *m_core;
    FileDownloaderProxyFactory *m_factory;
    QList<FileDownloadRequest> m_pendingRequests;
    QHash<FileDownloadRequest::Role, QList<FileTaskResult>> m_results;
    QHash<FileDownloadRequest::Role, int> m_pendingTasks;
    QList<QFutureWatcher<FileTaskResult> *> m_activeWatchers;
    QHash<QString, QPair<QString, QString>> m_checksumSourceByArchiveTarget;


    quint64 m_bytesReceived;
    quint64 m_allBytesReceived;
    bool m_dataDownloaded;
    bool m_sha1Downloaded;
    bool m_aborted;
    int m_retryCount;
    int m_downloadableChunkSize;
};

KDUpdater::FileDownloader::PrivatePtr::~PrivatePtr() {}

KDUpdater::FileDownloader::FileDownloader(QObject *parent)
    : QObject(parent)
{
    d.reset(new Private);
}

void FileDownloader::setDownloadAborted(const JobError error, const QString &errorStr)
{
    d->m_aborted = true;
    emit downloadAborted(error, errorStr);
}

void KDUpdater::FileDownloader::setDownloadCompleted()
{
    setDataDownloaded(true);
    emit downloadCompleted();
}

void KDUpdater::FileDownloader::setPackageManagerCore(PackageManagerCore *core)
{
    d->m_core = core;
}

void FileDownloader::resetRequests()
{
    d->m_pendingRequests.clear();
}

void FileDownloader::addRequest(const FileDownloadRequest &request)
{
    d->m_pendingRequests.append(request);
}

void FileDownloader::addRequests(const QList<FileDownloadRequest> &requests)
{
    d->m_pendingRequests.append(requests);
}

QList<FileDownloadRequest> FileDownloader::requests() const
{
    return d->m_pendingRequests;
}

QList<FileDownloadRequest> FileDownloader::takeNextChunk(
    FileDownloadRequest::Role role,
    FileDownloadRequest::TransferMethod transferMethod)
{
    QList<FileDownloadRequest> chunk;

    auto it = d->m_pendingRequests.begin();
    while (it != d->m_pendingRequests.end() && chunk.size() < d->m_downloadableChunkSize) {
        if (it->role == role && it->transferMethod == transferMethod) {
            chunk.append(*it);
            it = d->m_pendingRequests.erase(it);
        } else {
            ++it;
        }
    }

    return chunk;
}

void FileDownloader::download()
{
    QMetaObject::invokeMethod(this, [this]() {
        setDataDownloaded(false);
        setSha1Downloaded(false);
        d->m_results.clear();
        d->m_pendingTasks.clear();
        d->m_checksumSourceByArchiveTarget.clear();
        d->m_retryCount = scMaxRetries;
        startStage(FileDownloadRequest::Role::Checksum);
    }, Qt::QueuedConnection);
}

bool FileDownloader::startStage(FileDownloadRequest::Role role)
{
    if (isDownloadAborted())
        return false;

    QHash<FileDownloadRequest::TransferMethod, QList<FileDownloadRequest>> chunksByMethod;

    auto it = d->m_pendingRequests.begin();
    while (it != d->m_pendingRequests.end()) {
        if (it->role != role) {
            ++it;
            continue;
        }
        chunksByMethod[it->transferMethod].append(*it);
        it = d->m_pendingRequests.erase(it);
    }

    bool started = false;
    for(const auto &[method, chunk] : std::as_const(chunksByMethod).asKeyValueRange()) {
        if (chunk.isEmpty())
            continue;

        auto factory = FileDownloadRequest::taskFactoryForMethod(method);
        if (!factory) {
            setDownloadAborted(QInstaller::DownloadError,
                tr("No download handler registered for scheme \"%1\".").arg(it->taskItem.scheme()));
            return started;
        }

        QList<FileTaskItem> items;
        items.reserve(chunk.size());
        for (const auto &request : chunk)
            items.append(request.taskItem);

        AbstractFileTask *task = factory(items);
        setupFileTask(task, role, chunk.first().transferMethod);
        started = true;
    }

    return started;
}

void FileDownloader::setupFileTask(AbstractFileTask *task,
                                   FileDownloadRequest::Role role,
                                   FileDownloadRequest::TransferMethod transferMethod)
{
    Q_UNUSED(transferMethod)

    if (role == FileDownloadRequest::Role::RegularFile) {
        connect(task,
                &AbstractFileTask::progressChanged,
                this,
                &FileDownloader::setProgress);
    }

    connect(task,
            &AbstractFileTask::fileDownloaded,
            this,
            &FileDownloader::fileDownloaded);

    task->setProgressValueInBytes(true);

    auto *watcher = new QFutureWatcher<FileTaskResult>(this);
    d->m_activeWatchers.append(watcher);

    ++d->m_pendingTasks[role];

    connect(watcher, &QFutureWatcher<FileTaskResult>::finished, this, [this, watcher, role]() {
        onTaskFinished(watcher, role);
    });

    auto future = QtConcurrent::run(&AbstractTask<FileTaskResult>::doTask, static_cast<AbstractTask<FileTaskResult> *>(task));
    watcher->setFuture(future);
}

void KDUpdater::FileDownloader::resetTasks()
{
    for (auto *watcher : std::as_const(d->m_activeWatchers)) {
        try {
            watcher->cancel();
            watcher->waitForFinished();
        } catch (...) {
        }
    }
    qDeleteAll(d->m_activeWatchers);
    d->m_activeWatchers.clear();
    d->m_pendingTasks.clear();
}

void FileDownloader::reset()
{
    resetTasks();
    resetRequests();
    d->m_results.clear();
    d->m_checksumSourceByArchiveTarget.clear();
    d->m_bytesReceived = 0;
    d->m_allBytesReceived = 0;
    d->m_retryCount = scMaxRetries;
    d->m_aborted = false;
    setDataDownloaded(false);
    setSha1Downloaded(false);
}

void KDUpdater::FileDownloader::setProgress(quint64 bytesReceived)
{
    d->m_bytesReceived = bytesReceived + d->m_allBytesReceived;
    emit setProcessedAmount();
}

bool KDUpdater::FileDownloader::isDownloadAborted() const
{
    return d->m_aborted;
}

bool KDUpdater::FileDownloader::dataDownloaded() const
{
    return d->m_dataDownloaded;
}

void KDUpdater::FileDownloader::setDataDownloaded(bool downloaded)
{
    d->m_dataDownloaded = downloaded;
}

bool KDUpdater::FileDownloader::sha1Downloaded() const
{
    return d->m_sha1Downloaded;
}

void KDUpdater::FileDownloader::setSha1Downloaded(bool downloaded)
{
    d->m_sha1Downloaded = downloaded;
}

void FileDownloader::onTaskFinished(QFutureWatcher<FileTaskResult> *watcher,
                                    FileDownloadRequest::Role role)
{
    d->m_activeWatchers.removeOne(watcher);
    watcher->deleteLater();
    
    --d->m_pendingTasks[role];
    
    try {
        watcher->waitForFinished();
        d->m_results[role].append(watcher->future().results());
    } catch (const AuthenticationRequiredException &e) {
        if (e.type() == AuthenticationRequiredException::Type::Proxy) {
            qCWarning(QInstaller::lcInstallerInstallLog) << e.message();
            PackageManagerProxyFactory *factory = d->m_core->proxyFactory();
            if (factory->askProxyCredentials(e.proxy())) {
                d->m_core->setProxyFactory(factory);
                startStage(role);
            } else {
                reset();
                setDownloadAborted(QInstaller::DownloadError, tr("Missing proxy credentials."));
            }
        }
        return;
    } catch (const TaskException &e) {
        setDownloadAborted(QInstaller::DownloadError, e.message());
        return;
    } catch (const QUnhandledException &e) {
        setDownloadAborted(QInstaller::DownloadError, QLatin1String(e.what()));
        return;
    } catch (...) {
        setDownloadAborted(QInstaller::DownloadError, tr("Unknown exception during download."));
        return;
    }

    if (startStage(role))
        return;

    if (d->m_pendingTasks.value(role) > 0)
        return;

    if (role == FileDownloadRequest::Role::Checksum)
        shaDownloadFinished();
    else
        archiveDownloadFinished();
}

void FileDownloader::shaDownloadFinished()
{
    const auto checksumResults = d->m_results.value(FileDownloadRequest::Role::Checksum);
    for (const FileTaskResult &result : checksumResults) {
        const FileTaskItem checksumItem = result.taskItem();
        const QString expectedTarget = checksumItem.target().chopped(5); // strip ".sha1"

        for (auto &request : d->m_pendingRequests) {
            qDebug() << "Matching checksum for request: " << request.taskItem.target() << " with expected target: " << expectedTarget;
            if (request.role == FileDownloadRequest::Role::RegularFile
                    && request.taskItem.target() == expectedTarget) {
                request.taskItem.insert(TaskRole::Checksum, checksumItem.value(TaskRole::Checksum));
                d->m_checksumSourceByArchiveTarget.insert(expectedTarget, qMakePair(checksumItem.source(), checksumItem.scheme()));
                break;
            }
        }
    }
    d->m_results.remove(FileDownloadRequest::Role::Checksum);

    setSha1Downloaded(true);
    emit sha1DownloadFinished();

    startStage(FileDownloadRequest::Role::RegularFile);
}

void FileDownloader::archiveDownloadFinished()
{
    d->m_allBytesReceived = d->m_bytesReceived;

    QList<FileDownloadRequest> failedRequests;
    const auto archiveResults = d->m_results.value(FileDownloadRequest::Role::RegularFile);
    foreach (const FileTaskResult &result, archiveResults) {
        FileTaskItem item = result.value(TaskRole::TaskItem).value<FileTaskItem>();
        const QByteArray checksum = result.value(TaskRole::Checksum).toByteArray().toHex();
        const QByteArray expectedChecksum = item.value(TaskRole::Checksum).toByteArray();

        if (expectedChecksum != checksum) {
            qCWarning(QInstaller::lcInstallerInstallLog) << tr("Hash verification error while "
                "downloading %1. This can be a temporary error, retrying download.\n\n"
                "Expected: %2 \nDownloaded: %3").arg(item.source(),
                QString::fromLatin1(expectedChecksum), QString::fromLatin1(checksum));

            auto [checksumSource, checksumScheme] = d->m_checksumSourceByArchiveTarget.value(item.target());

            FileTaskItem sha1Item = item;
            sha1Item.insert(TaskRole::SourceFile, checksumSource);
            sha1Item.insert(TaskRole::TargetFile, QString(item.target() + QLatin1String(".sha1")));
            sha1Item.insert(TaskRole::Scheme, checksumScheme);

            FileDownloadRequest sha1Request(sha1Item, FileDownloadRequest::Role::Checksum, checksumScheme);

            failedRequests.append(sha1Request);

            FileDownloadRequest archiveRequest(item, FileDownloadRequest::Role::RegularFile, item.scheme());
            failedRequests.append(archiveRequest);
        } else {
            emit registerFile(item);
        }
    }
    d->m_results.remove(FileDownloadRequest::Role::RegularFile);

    if (failedRequests.isEmpty()) {
        setDownloadCompleted();
        return;
    }

    --d->m_retryCount;
    if (d->m_retryCount <= 0) {
        setDownloadAborted(QInstaller::DownloadError, tr("Cannot verify Hash"));
        return;
    }

    resetRequests();
    addRequests(failedRequests);

    for (const FileDownloadRequest &request : std::as_const(failedRequests)) {
        if (request.role != FileDownloadRequest::Role::Checksum)
            continue;
        QFileInfo fi(request.taskItem.target());
        emit retryFileDownload(fi.fileName());
    }

    startStage(FileDownloadRequest::Role::Checksum);
}

FileDownloaderProxyFactory *KDUpdater::FileDownloader::proxyFactory() const
{
    if (d->m_factory)
        return d->m_factory->clone();
    return 0;
}

void KDUpdater::FileDownloader::setProxyFactory(FileDownloaderProxyFactory *factory)
{
    delete d->m_factory;
    d->m_factory = factory;
}

quint64 FileDownloader::bytesReceived() const
{
    return d->m_bytesReceived;
}

CopyFileTask *FileDownloader::createLocalTask(const QList<FileDownloadRequest> &requests)
{
    QList<FileTaskItem> items;

    for (const auto &request : requests)
        items.append(request.taskItem);

    return new CopyFileTask(items);
}

DownloadFileTask *FileDownloader::createNetworkTask(const QList<FileDownloadRequest> &requests)
{
    QList<FileTaskItem> items;

    for (const auto &request : requests)
        items.append(request.taskItem);

    auto *task = new DownloadFileTask(items);

    connect(task,
            &DownloadFileTask::networkDisconnected,
            this,
            &FileDownloader::networkDisconnected);

    return task;
}
