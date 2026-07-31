#ifndef DOWNLOADABLEARCHIVE_H
#define DOWNLOADABLEARCHIVE_H

#include <QString>
#include <QList>
#include <QUrl>

namespace QInstaller {

struct DownloadableArchive
{
    QString fileName;
    QUrl url;
    QUrl sha1Url;
};

}

#endif // DOWNLOADABLEARCHIVE_H
