/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/
#include "copyfiletask.h"
#include "observer.h"

#include <QDir>
#include <QFileInfo>
#include <QTemporaryFile>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::CopyFileTask
    \internal
*/

CopyFileTask::CopyFileTask(const FileTaskItem &item)
    : AbstractFileTask(item)
{
}

CopyFileTask::CopyFileTask(const QList<FileTaskItem> &items)
    : AbstractFileTask()
{
    setTaskItems(items);
}

CopyFileTask::CopyFileTask(const QString &source)
    : AbstractFileTask(source)
{
}

CopyFileTask::CopyFileTask(const QString &source, const QString &target)
    : AbstractFileTask(source, target)
{
}

void CopyFileTask::doTask(QFutureInterface<FileTaskResult> &fi)
{
    QList<FileTaskItem> items = taskItems();
    fi.reportStarted();
    fi.setExpectedResultCount(items.count());

    if (items.isEmpty()) {
        fi.reportException(TaskException(tr("Invalid task item count.")));
        fi.reportFinished(); return;    // error
    }

    quint64 dataRead = 0;
    for (FileTaskItem &item : items) {
        FileTaskObserver observer(QCryptographicHash::Sha1);
        QFile source(item.source());
        if (!source.open(QIODevice::ReadOnly)) {
            fi.reportException(TaskException(tr("Cannot open file \"%1\" for reading: %2")
                .arg(QDir::toNativeSeparators(source.fileName()), source.errorString())));
            fi.reportFinished(); return;    // error
        }
        observer.setBytesToTransfer(source.size());

        QScopedPointer<QFile> file;
        const QString target = item.target();
        if (target.isEmpty()) {
            QTemporaryFile *tmp = new QTemporaryFile;
            tmp->setAutoRemove(false);
            file.reset(tmp);
        } else {
            file.reset(new QFile(target));
        }
        if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fi.reportException(TaskException(tr("Cannot open file \"%1\" for writing: %2")
                .arg(QDir::toNativeSeparators(file->fileName()), file->errorString())));
            fi.reportFinished(); return;    // error
        }

        QByteArray buffer(32768, Qt::Uninitialized);
        while (!source.atEnd() && source.error() == QFile::NoError) {
            if (fi.isCanceled())
                break;
            if (fi.isSuspended())
                fi.waitForResume();

            const qint64 read = source.read(buffer.data(), buffer.size());
            qint64 written = 0;
            while (written < read) {
                const qint64 toWrite = file->write(buffer.constData() + written, read - written);
                if (toWrite < 0) {
                    fi.reportException(TaskException(tr("Writing to file \"%1\" failed: %2")
                        .arg(QDir::toNativeSeparators(file->fileName()), file->errorString())));
                }
                written += toWrite;
            }

            observer.addSample(read);
            observer.timerEvent(nullptr);
            observer.addBytesTransfered(read);
            dataRead += read;
            if (file->fileName().endsWith(QLatin1String(".sha1")))
                observer.setExpectedSha1(buffer.left(read));
            else
                observer.addCheckSumData(buffer.left(read));

            if (progressValueInBytes())
                emit progressChanged(dataRead);
            else
                fi.setProgressValueAndText(observer.progressValue(), observer.progressText());
        }

        // Expected checksum is read from file
        if (!observer.expectedSha1().isEmpty())
            item.insert(TaskRole::Checksum, observer.expectedSha1());

        fi.reportResult(FileTaskResult(file->fileName(), observer.checkSum(), item));

        QFileInfo fi(file->fileName());
        emit fileDownloaded(fi.fileName(), item.value(TaskRole::ComponentName).toString());
    }

    fi.reportFinished();
}

}   // namespace QInstaller
