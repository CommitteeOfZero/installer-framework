/* Copyright C 2002 Klaralvdalens Datakonsult AB KDAB
 * Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef UPDATESINFODATA_P_H
#define UPDATESINFODATA_P_H

#include <QCoreApplication>
#include <QSharedData>

QT_FORWARD_DECLARE_CLASS(QXmlStreamReader)

namespace KDUpdater {

struct UpdateInfo;

struct UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData(const bool postLoadComponentScript);
    ~UpdatesInfoData();

    int error;
    QString errorMessage;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    QList<UpdateInfo> updateInfoList;
    bool m_postLoadComponentScript;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(QXmlStreamReader &reader);
    
    void setInvalidContentError(const QString &detail);
    
    private:
    void processLocalizedTag(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
    void parseOperations(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
    void parseLicenses(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
    void parseDownloadableArchives(QXmlStreamReader &reader, QHash<QString, QVariant> &info) const;
};

} // namespace KDUpdater

#endif // UPDATESINFODATA_P_H
