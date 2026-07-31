#include "filedownloadrequest.h"
#include "downloadfiletask.h"
#include "copyfiletask.h"
#include "errors.h"
#include "globals.h"

#include <QtNetwork/QSslSocket>
#include <QtCore/qurl.h>

using namespace KDUpdater;

namespace {
    using SchemeRegistry = QHash<QString, FileDownloadRequest::TransferMethod>;
    using FactoryRegistry = QHash<FileDownloadRequest::TransferMethod, FileDownloadRequest::TaskFactory>;

    Q_GLOBAL_STATIC(SchemeRegistry, schemeRegistry)
    Q_GLOBAL_STATIC(FactoryRegistry, factoryRegistry)
}

void registerBuiltins()
{
    FileDownloadRequest::registerScheme(QLatin1String("file"), FileDownloadRequest::TransferMethod::LocalFile);
    FileDownloadRequest::registerScheme(QLatin1String("http"), FileDownloadRequest::TransferMethod::Network);
    FileDownloadRequest::registerScheme(QLatin1String("ftp"), FileDownloadRequest::TransferMethod::Network);

#ifndef QT_NO_SSL
    if (QSslSocket::supportsSsl()) {
        FileDownloadRequest::registerScheme(QLatin1String("https"), FileDownloadRequest::TransferMethod::Network);
    } else {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "Cannot register file downloads for https protocol:"
               "QSslSocket::supportsSsl() returns false";
    }
#endif

    FileDownloadRequest::registerTaskFactory(FileDownloadRequest::TransferMethod::LocalFile,
        [](const QList<FileTaskItem> &items) -> AbstractFileTask * { return new CopyFileTask(items); });
    FileDownloadRequest::registerTaskFactory(FileDownloadRequest::TransferMethod::Network,
        [](const QList<FileTaskItem> &items) -> AbstractFileTask * { return new DownloadFileTask(items); });
}

void ensureBuiltinRegistered()
{
    static bool registered = [] {
        registerBuiltins();
        return true;
    }();
    Q_UNUSED(registered)
}

void FileDownloadRequest::registerScheme(const QString &scheme, TransferMethod method)
{
    schemeRegistry()->insert(scheme, method);
}

void FileDownloadRequest::registerTaskFactory(TransferMethod method, TaskFactory factory)
{
    factoryRegistry()->insert(method, std::move(factory));
}

FileDownloadRequest::TransferMethod FileDownloadRequest::transferMethodForScheme(const QString &scheme)
{
    ensureBuiltinRegistered();

    const SchemeRegistry *r = schemeRegistry();
    const auto it = r->constFind(scheme);
    if (it == r->constEnd()) {
        qCWarning(QInstaller::lcInstallerInstallLog)
            << "No registered transfer method for scheme" << scheme;
        return TransferMethod::None;
    }
    return it.value();
}

FileDownloadRequest::TaskFactory FileDownloadRequest::taskFactoryForMethod(TransferMethod method)
{
    ensureBuiltinRegistered();

    const FactoryRegistry *r = factoryRegistry();
    const auto it = r->constFind(method);
    if (it == r->constEnd())
        return TaskFactory(); // empty; caller must check
    return it.value();
}

QStringList FileDownloadRequest::supportedSchemes()
{
    ensureBuiltinRegistered();
    return schemeRegistry()->keys();
}

FileDownloadRequest::FileDownloadRequest(FileTaskItem taskItem, Role role, QString scheme)
    : taskItem(taskItem)
    , role(role)
    , transferMethod(transferMethodForScheme(scheme))
{
}

