/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef COPYFILETASK_H
#define COPYFILETASK_H

#include "abstractfiletask.h"

class FileTaskObserver;

namespace QInstaller {

class INSTALLER_EXPORT CopyFileTask : public AbstractFileTask
{
    Q_OBJECT
    Q_DISABLE_COPY(CopyFileTask)

public:
    CopyFileTask() {}
    explicit CopyFileTask(const FileTaskItem &item);
    explicit CopyFileTask(const QList<FileTaskItem> &items);

    explicit CopyFileTask(const QString &source);
    CopyFileTask(const QString &source, const QString &target);

    void doTask(QFutureInterface<FileTaskResult> &fi) override;
};

}   // namespace QInstaller

#endif // COPYFILETASK_H
