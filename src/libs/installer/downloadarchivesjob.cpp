/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/
#include "downloadarchivesjob.h"

#include "binaryformatenginehandler.h"
#include "component.h"
#include "packagemanagercore.h"
#include "fileutils.h"
#include "abstractfiletask.h"
#include "downloadfiletask.h"

#include "filedownloader.h"
#include "filedownloaderfactory.h"

#include <QtCore/QFile>
#include <QtCore/QTimerEvent>

using namespace QInstaller;
using namespace KDUpdater;

/*!
    Creates a new DownloadArchivesJob with parent \a core.
*/
DownloadArchivesJob::DownloadArchivesJob(PackageManagerCore *core, const QString &objectName)
    : Job(core)
    , m_core(core)
    , m_archivesDownloaded(0)
    , m_canceled(false)
    , m_progressChangedTimerId(0)
    , m_currentDownloaded(0)
    , m_totalAmount(0)
    , m_retryDownload(false)
{
    setCapabilities(Cancelable);
    setObjectName(objectName);
}

/*!
    Destroys the DownloadArchivesJob.
*/
DownloadArchivesJob::~DownloadArchivesJob()
{
    if (m_downloader)
        m_downloader->deleteLater();
}

/*!
    Sets the \a archives to download. The first value of each pair contains the file name to register
    the file in the installer's internal file system, the second one the source url.
*/
void DownloadArchivesJob::setArchivesToDownload(const QList<DownloadableArchive> &archives)
{
    m_archivesToDownload = archives;
}

/*!
    Sets the expected total size of archives to download to \a total.
*/
void DownloadArchivesJob::setExpectedTotalSize(quint64 total)
{
    m_totalAmount = total;
}

/*!
    \reimp
*/
void DownloadArchivesJob::doStart()
{
    m_totalDownloadSpeedTimer.start();
    m_archivesDownloaded = 0;
    fetchArchives();
}

/*!
    \reimp
*/
void DownloadArchivesJob::doCancel()
{
    m_canceled = true;
    if (m_downloader)
        m_downloader->reset();

    emitFinishedWithError(Job::Canceled, tr("Download canceled."));
}

void DownloadArchivesJob::fetchArchives()
{
    if (m_archivesToDownload.isEmpty()) {
        emitFinished();
        return;
    }
    setupDownloaders();
    m_downloader->download();
}

void DownloadArchivesJob::networkDisconnected()
{
    setTotalProcessedAmount();
}

/*!
    This is used to reduce the \c progressChanged signals for \a event.
*/
void DownloadArchivesJob::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_progressChangedTimerId) {
        killTimer(m_progressChangedTimerId);
        m_progressChangedTimerId = 0;

        quint64 currentDownloaded = 0;
        currentDownloaded += m_downloader->bytesReceived();
        setProcessedAmount(currentDownloaded);

        // processedAmount might excess totalAmount if sha mismatch is detected
        // and some packages are refetched.
        if (totalAmount() != 0 && double(processedAmount()) > totalAmount()) {
            if (m_retryDownload)
                emit downloadStatusChanged(tr("Redownloading packages due to previous verification errors."));
            return;
        }
        if (totalAmount() > 0)
            emit progressChanged(double(processedAmount()) / double(totalAmount()));

        onDownloadStatusChanged(currentDownloaded);
    }
}

/*!
    Builds a textual representation of the total download \a currentDownloaded
    and emits the \c {downloadStatusChanged()} signal.
*/
void DownloadArchivesJob::onDownloadStatusChanged(const quint64 currentDownloaded)
{
    if (m_canceled)
        return;

    QString status;

    if (totalAmount() > 0) {
        QString bytesReceived = humanReadableSize(currentDownloaded);
        const QString bytesToReceive = humanReadableSize(totalAmount());

        // remove the unit from the bytesReceived value if bytesToReceive has the same
        const QString tmp = bytesToReceive.mid(bytesToReceive.indexOf(QLatin1Char(' ')));
        if (bytesReceived.endsWith(tmp))
            bytesReceived.chop(tmp.length());

        status = tr("%1 of %2").arg(bytesReceived, bytesToReceive);
    } else if (currentDownloaded > 0) {
        status = tr("%1 downloaded.").arg(humanReadableSize(currentDownloaded));
    }

    quint64 totalDownloadSpeed = 0;

    // Show download speed after download has fully started and we can more reliably
    // calculate the real download speed
    if (m_totalDownloadSpeedTimer.isValid() && m_totalDownloadSpeedTimer.elapsed() > 8000)
        totalDownloadSpeed = currentDownloaded / double(m_totalDownloadSpeedTimer.elapsed() / 1000);

    if (totalAmount() > 0 && totalDownloadSpeed > 0 && m_currentDownloaded != currentDownloaded) {
        const quint64 time = (totalAmount() - currentDownloaded) / totalDownloadSpeed;

        int s = time % 60;
        const int d = time / 86400;
        const int h = (time / 3600) - (d * 24);
        const int m = (time / 60) - (d * 1440) - (h * 60);

        QString days;
        if (d > 0)
            days = tr("%n day(s), ", "", d);

        QString hours;
        if (h > 0)
            hours = tr("%n hour(s), ", "", h);

        QString minutes;
        if (m > 0)
            minutes = tr("%n minute(s)", "", m);

        QString seconds;
        if (s >= 0 && minutes.isEmpty()) {
            s = (s <= 0 ? 1 : s);
            seconds = tr("%n second(s)", "", s);
        }
        status += tr(" - %1%2%3%4 remaining.").arg(days, hours, minutes, seconds);
    } else {
        status += tr(" - unknown time remaining.");
    }
    m_currentDownloaded = currentDownloaded;
    emit downloadStatusChanged(tr("Downloading: ")+ status);
}

void DownloadArchivesJob::setTotalProcessedAmount()
{
    // Use timer to prevent UI from updating progress too frequently
    if (!m_progressChangedTimerId)
        m_progressChangedTimerId = startTimer(300);
}

/*!
    Registers the just downloaded file in the installer's file system.
*/
void DownloadArchivesJob::registerFile(const FileTaskItem &item)
{
    if (m_canceled || m_archivesToDownload.isEmpty())
        return;

    ++m_archivesDownloaded;
    if (m_progressChangedTimerId) {
        killTimer(m_progressChangedTimerId);
        m_progressChangedTimerId = 0;
    }

    BinaryFormatEngineHandler::instance()->registerResource(item.value(TaskRole::Name).toString(), item.target());

    emit fileDownloadReady(item.target());
}

void DownloadArchivesJob::fileDownloaded(const QString &fileName, const QString &componentName)
{
    emit outputTextChanged(tr("File \"%1\" downloaded for component %2.")
                               .arg(fileName, componentName));
}

void DownloadArchivesJob::retryFileDownload(const QString &fileName)
{
    m_retryDownload = true;
    emit outputTextChanged(tr("Hash verification error while downloading %1. "
                              "This can be a temporary error, retrying download.").arg(fileName));
}

void DownloadArchivesJob::downloadCompleted()
{
    // Wait for all downloaders to complete
    if (m_downloader && !m_downloader->dataDownloaded())
        return;
    emitFinished();
    m_archivesToDownload.clear();
}

void DownloadArchivesJob::sha1DownloadFinished()
{
    // Wait for all downloaders to complete
    if (!m_downloader->sha1Downloaded())
        return;
    
    // Start showing progress of downloaded data
    setTotalAmount(m_totalAmount);
}


void DownloadArchivesJob::downloadAborted(const JobError error, const QString &errorStr)
{
    if (m_canceled)
        return;

    emitFinishedWithError(error, errorStr);
}

void DownloadArchivesJob::finishWithError(const QString &error)
{
    emitFinishedWithError(QInstaller::DownloadError, error);
}

void DownloadArchivesJob::setupDownloaders()
{
    const QString &queryString = m_core->value(scUrlQueryString);
    for (const DownloadableArchive &item : std::as_const(m_archivesToDownload)) {
        const QFileInfo fi(item.fileName);

        const Component *const component =
            m_core->componentByName(
                PackageManagerCore::checkableName(QFileInfo(fi.path()).fileName()));

        if (!component) {
            emit outputTextChanged(
                tr("Cannot find component for %1.")
                    .arg(QFileInfo(fi.path()).fileName()));
            continue;
        }

        QString fullQueryString;
        if (!queryString.isEmpty())
            fullQueryString = QLatin1String("?") + queryString;

        if(!m_downloader) {
            m_downloader = FileDownloaderFactory::instance().create(this);
            if (!m_downloader) {
                return;
            }

            m_downloader->setPackageManagerCore(m_core);
        }

        const QString archiveTarget = 
            component->localTempPath()
            + QLatin1Char('/')
            + component->name()
            + QLatin1Char('/')
            + fi.fileName();

        QUrl archiveUrl(item.url);
        archiveUrl.setQuery(fullQueryString.mid(1));
        const QString archiveSource = (archiveUrl.scheme() == QLatin1String("file"))
            ? archiveUrl.toLocalFile()
            : archiveUrl.toString();

        FileTaskItem archiveTaskItem(archiveSource, archiveTarget);

        const QString target = 
            component->localTempPath()
            + QLatin1Char('/')
            + component->name()
            + QLatin1Char('/')
            + fi.fileName();

        QUrl sha1Url = item.sha1Url;
        sha1Url.setQuery(fullQueryString.mid(1));

        const QString sha1Target = target + QLatin1String(".sha1");

        QString sha1Source = (sha1Url.scheme() == QLatin1String("file"))
            ? sha1Url.toLocalFile()
            : sha1Url.toString();

        FileTaskItem shaTaskItem(sha1Source, sha1Target);        
        
        archiveTaskItem.insert(TaskRole::SourceFile, archiveSource);
        archiveTaskItem.insert(TaskRole::TargetFile, target);
        archiveTaskItem.insert(TaskRole::Scheme, archiveUrl.scheme());
        archiveTaskItem.insert(TaskRole::Name, item.fileName);
        archiveTaskItem.insert(TaskRole::ComponentName, component->displayName());

        shaTaskItem.insert(TaskRole::SourceFile, sha1Source);
        shaTaskItem.insert(TaskRole::TargetFile, sha1Target);
        shaTaskItem.insert(TaskRole::Scheme, sha1Url.scheme());
        shaTaskItem.insert(TaskRole::Name, QString(item.fileName + QLatin1String(".sha1")));
        shaTaskItem.insert(TaskRole::ComponentName, component->displayName());

        QAuthenticator authenticator;
        authenticator.setUser(component->value(QLatin1String("username")));
        authenticator.setPassword(component->value(QLatin1String("password")));
        archiveTaskItem.insert(TaskRole::Authenticator,
            QVariant::fromValue(authenticator));
        shaTaskItem.insert(TaskRole::Authenticator,
            QVariant::fromValue(authenticator));

        m_downloader->addRequest(FileDownloadRequest(archiveTaskItem,FileDownloadRequest::Role::RegularFile, archiveUrl.scheme()));
        m_downloader->addRequest(FileDownloadRequest(shaTaskItem, FileDownloadRequest::Role::Checksum, sha1Url.scheme()));
    }
    connect(m_downloader, &FileDownloader::downloadAborted, this, &DownloadArchivesJob::downloadAborted,
        Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::setProcessedAmount,
            this, &DownloadArchivesJob::setTotalProcessedAmount, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::sha1DownloadFinished,
            this, &DownloadArchivesJob::sha1DownloadFinished, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::registerFile,
            this, &DownloadArchivesJob::registerFile, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::fileDownloaded,
            this, &DownloadArchivesJob::fileDownloaded, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::retryFileDownload,
            this, &DownloadArchivesJob::retryFileDownload, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::downloadCompleted,
            this, &DownloadArchivesJob::downloadCompleted, Qt::QueuedConnection);
    connect(m_downloader, &FileDownloader::networkDisconnected,
            this, &DownloadArchivesJob::networkDisconnected, Qt::QueuedConnection);
    
}
