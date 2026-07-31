/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef DOWNLOADARCHIVESJOB_H
#define DOWNLOADARCHIVESJOB_H

#include "job.h"
#include "packagemanagercore.h"
#include "downloadablearchive.h"
#include <QtCore/QPair>
#include <QtCore/QElapsedTimer>

QT_BEGIN_NAMESPACE
class QTimerEvent;
QT_END_NAMESPACE

#include "filedownloader.h"

namespace QInstaller {

class MessageBoxHandler;

class DownloadArchivesJob : public Job
{
    Q_OBJECT

public:
    explicit DownloadArchivesJob(PackageManagerCore *core, const QString &objectName);
    ~DownloadArchivesJob();

    int numberOfDownloads() const { return m_archivesDownloaded; }
    void setArchivesToDownload(const QList<DownloadableArchive> &archives);
    void setExpectedTotalSize(quint64 total);

Q_SIGNALS:
    void progressChanged(double progress);
    void outputTextChanged(const QString &progress);
    void downloadStatusChanged(const QString &status);

    void hashDownloadReady(const QString &localPath);
    void fileDownloadReady(const QString &localPath);

protected:
    void doStart() override;
    void doCancel() override;
    void timerEvent(QTimerEvent *event) override;

public Q_SLOTS:
    void registerFile(const QInstaller::FileTaskItem &item);
    void fileDownloaded(const QString &fileName, const QString &componentName);
    void setTotalProcessedAmount();
    void retryFileDownload(const QString &fileName);

protected Q_SLOTS:
    void downloadCompleted();
    void sha1DownloadFinished();
    void downloadAborted(const JobError error, const QString &errorStr);
    void finishWithError(const QString &error);
    void fetchArchives();
    void networkDisconnected();

private:
    void setupDownloaders();
    void onDownloadStatusChanged(const quint64 currentDownloaded);

private:
    PackageManagerCore *m_core;
    KDUpdater::FileDownloader *m_downloader = nullptr;

    int m_archivesDownloaded;
    QList<DownloadableArchive> m_archivesToDownload;

    bool m_canceled;
    int m_progressChangedTimerId;

    QElapsedTimer m_totalDownloadSpeedTimer;
    quint64 m_currentDownloaded;
    quint64 m_totalAmount;
    bool m_retryDownload;
};

} // namespace QInstaller

#endif  // DOWNLOADARCHIVESJOB_H
