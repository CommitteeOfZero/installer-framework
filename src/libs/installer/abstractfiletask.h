/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef ABSTRACTFILETASK_H
#define ABSTRACTFILETASK_H

#include "abstracttask.h"
#include "installer_global.h"

#include <QObject>
#include <QReadWriteLock>

namespace QInstaller {

namespace TaskRole {
enum TaskRole
{
    Checksum,
    TaskItem,
    SourceFile,
    TargetFile,
    Name,
    ComponentName,
    Scheme,
    UserRole = 1000
};
}

class FileTaskItem : public AbstractTaskData
{
public:
    FileTaskItem() {}
    explicit FileTaskItem(const QString &s)
    {
        insert(TaskRole::SourceFile, s);
    }
    FileTaskItem(const QString &s, const QString &t)
    {
        insert(TaskRole::SourceFile, s);
        insert(TaskRole::TargetFile, t);
    }

    QString source() const { return value(TaskRole::SourceFile).toString(); }
    QString target() const { return value(TaskRole::TargetFile).toString(); }
    QString scheme() const { return value(TaskRole::Scheme).toString(); }
};

class FileTaskResult : public AbstractTaskData
{
public:
    FileTaskResult() {}
    FileTaskResult(const QString &t, const QByteArray &c, const FileTaskItem &i)
    {
        insert(TaskRole::Checksum, c);
        insert(TaskRole::TargetFile, t);
        insert(TaskRole::TaskItem, QVariant::fromValue(i));
    }

    QString target() const { return value(TaskRole::TargetFile).toString(); }
    QByteArray checkSum() const { return value(TaskRole::Checksum).toByteArray(); }
    FileTaskItem taskItem() const { return value(TaskRole::TaskItem).value<FileTaskItem>(); }
};

class INSTALLER_EXPORT AbstractFileTask : public AbstractTask<FileTaskResult>
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractFileTask)

public:
    AbstractFileTask();
    virtual ~AbstractFileTask() {}

    explicit AbstractFileTask(const QString &source);
    explicit AbstractFileTask(const FileTaskItem &item);
    AbstractFileTask(const QString &source, const QString &target);

    QList<FileTaskItem> taskItems() const;
    void setTaskItem(const FileTaskItem &item);
    void setProgressValueInBytes(bool progressInBytes);
    bool progressValueInBytes() const;

Q_SIGNALS:
    void fileDownloaded(const QString &fileName, const QString &componentName);
    void progressChanged(const quint64 progress);

protected:
    void clearTaskItems();
    void addTaskItem(const FileTaskItem &item);
    void setTaskItems(const QList<FileTaskItem> &items);
    void addTaskItems(const QList<FileTaskItem> &items);

private:
    void registerMetaTypes();

private:
    QList<FileTaskItem> m_items;
    mutable QReadWriteLock m_lock;
    bool m_progressValueInBytes;
};

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::FileTaskItem)
Q_DECLARE_METATYPE(QInstaller::FileTaskResult)
Q_DECLARE_METATYPE(QInstaller::TaskException)

#endif // ABSTRACTFILETASK_H
