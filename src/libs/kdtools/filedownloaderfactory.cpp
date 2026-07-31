/* Copyright C 2013 Klaralvdalens Datakonsult AB KDAB
 * Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#include "filedownloaderfactory.h"
#include "filedownloader.h"
#include "globals.h"

#include <QtNetwork/QSslSocket>

using namespace KDUpdater;

/*!
   \inmodule kdupdater
   \class KDUpdater::FileDownloaderFactory
   \brief The FileDownloaderFactory class acts as a factory for KDUpdater::FileDownloader.

   Only one instance of this class can be created and its reference can be fetched from the instance() method.
*/

/*!
    Returns the file downloader factory instance.
*/
FileDownloaderFactory& FileDownloaderFactory::instance()
{
    static KDUpdater::FileDownloaderFactory theFactory;
    return theFactory;
}

/*!
    Constructs a file downloader factory and registers the default file downloader set.
*/
FileDownloaderFactory::FileDownloaderFactory()
    : d (new FileDownloaderFactoryData)
{}

/*!
    Sets \a factory as the file downloader proxy factory.
*/
void FileDownloaderFactory::setProxyFactory(FileDownloaderProxyFactory *factory)
{
    delete FileDownloaderFactory::instance().d->m_factory;
    FileDownloaderFactory::instance().d->m_factory = factory;
}

/*!
    Destroys the file downloader factory.
*/
FileDownloaderFactory::~FileDownloaderFactory()
{
    delete d;
}

/*!
     Returns a new instance of a KDUpdater::FileDownloader class.
*/
FileDownloader* FileDownloaderFactory::create(QObject *parent) const
{
    FileDownloader *downloader = new FileDownloader(parent);
    downloader->setProxyFactory(d->m_factory->clone());
    
    return downloader;
}