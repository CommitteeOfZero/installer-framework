/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef BINARYCONTENT_H
#define BINARYCONTENT_H

#include "binaryformat.h"
#include "binarylayout.h"

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

namespace QInstaller {

class INSTALLER_EXPORT BinaryContent
{
public:
    // the marker to distinguish what kind of binary
    static const qint64 MagicInstallerMarker = 0x12023233UL;
    static const qint64 MagicUninstallerMarker = 0x12023234UL;

    static const qint64 MagicUpdaterMarker = 0x12023235UL;
    static const qint64 MagicPackageManagerMarker = 0x12023236UL;

    // additional distinguishers only used at runtime, not written to the binary itself
    enum MagicMarkerSupplement {
        Default = 0x0,
        OfflineGenerator = 0x1,
        PackageViewer = 0x2,
        PortableInstaller = 0x3,
    };

    // the cookie put at the end of the file
    static const quint64 MagicCookie = 0xc2630a1c99d668f8LL;  // binary
    static const quint64 MagicCookieDat = 0xc2630a1c99d668f9LL; // data

    static qint64 findMagicCookie(QFile *file, quint64 magicCookie);
    static BinaryLayout binaryLayout(QFile *file, quint64 magicCookie);

    static void readBinaryContent(QFile *file,
                                QList<OperationBlob> *operations,
                                ResourceCollectionManager *manager,
                                qint64 *magicMarker,
                                quint64 magicCookie);

    static void writeBinaryContent(QFile *out,
                                const QList<OperationBlob> &operations,
                                const ResourceCollectionManager &manager,
                                qint64 magicMarker,
                                quint64 magicCookie);
};

} // namespace QInstaller

#endif // BINARYCONTENT_H
