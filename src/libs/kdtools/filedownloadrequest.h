#ifndef FILEDOWNLOADREQUEST_H
#define FILEDOWNLOADREQUEST_H

#include "abstractfiletask.h"

using namespace QInstaller;

namespace KDUpdater {

struct FileDownloadRequest
{
    using TaskFactory = std::function<AbstractFileTask *(const QList<FileTaskItem> &)>;
    enum class Role {
        RegularFile,
        Checksum
    };

    enum class TransferMethod {
        None,
        Network,
        LocalFile
    };

    FileTaskItem taskItem;
    Role role = Role::RegularFile;
    TransferMethod transferMethod = TransferMethod::None;

    FileDownloadRequest(FileTaskItem taskItem, Role role, QString scheme);

    static void registerScheme(const QString &scheme, TransferMethod method);
    static void registerTaskFactory(TransferMethod method, TaskFactory factory);
    static TransferMethod transferMethodForScheme(const QString &scheme);
    static TaskFactory taskFactoryForMethod(TransferMethod method);
    static QStringList supportedSchemes();
};

} // namespace KDUpdater

#endif // FILEDOWNLOADREQUEST_H