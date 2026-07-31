/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#ifndef COMPONENT_P_H
#define COMPONENT_P_H

#include "qinstallerglobal.h"
#include "downloadablearchive.h"

#include <QJSValue>
#include <QPointer>
#include <QStringList>
#include <QUrl>

namespace QInstaller {

class Component;
class PackageManagerCore;
class ScriptEngine;

class ComponentPrivate
{
    QInstaller::Component* const q;

public:
    enum InstallState {
        Uninstalled = 0,
        Installed = 1
    };

    explicit ComponentPrivate(PackageManagerCore *core, Component *qq);
    ~ComponentPrivate();

    ScriptEngine *scriptEngine() const;

    PackageManagerCore *m_core;
    Component *m_parentComponent;
    OperationList m_operations;
    Operation *m_licenseOperation;
    Operation *m_minimumProgressOperation;
    InstallState m_installState;

    bool m_newlyInstalled;
    bool m_operationsCreated;
    bool m_autoCreateOperations;
    bool m_operationsCreatedSuccessfully;
    bool m_updateIsAvailable;
    bool m_treeNameMoveChildren;
    bool m_postLoadScript;

    QString m_componentName;
    QUrl m_repositoryUrl;
    QString m_localTempPath;
    QJSValue m_scriptContext;
    QJSValue m_postScriptContext;
    QHash<QString, QString> m_vars;
    QList<Component*> m_childComponents;
    QList<Component*> m_allChildComponents;
    QHash<QString, DownloadableArchive> m_downloadableArchives;
    QList<DownloadableArchive> m_downloadableArchivesInit;
    QStringList m_stopProcessForUpdateRequests;
    QHash<QString, QPointer<QWidget> > m_userInterfaces;
    QHash<QString, QVariant> m_scriptHash;

    // < display name, < file name, file content > >
    QHash<QString, QVariantMap> m_licenses;
    QList<QPair<QString, bool> > m_pathsForUninstallation;
};


// -- ComponentModelHelper

class INSTALLER_EXPORT ComponentModelHelper
{
public:
    enum Roles {
        Action = Qt::UserRole + 1,
        LocalDisplayVersion,
        RemoteDisplayVersion,
        ReleaseDate,
        UncompressedSize,
        ExpandedByDefault
    };

    enum InstallAction {
        Install,
        Uninstall,
        KeepInstalled,
        KeepUninstalled,
        AutodependUninstallation
    };

    enum Column {
        NameColumn = 0,
        ActionColumn,
        InstalledVersionColumn,
        NewVersionColumn,
        ReleaseDateColumn,
        UncompressedSizeColumn,
        LastColumn
    };

    explicit ComponentModelHelper();

    int childCount() const;
    Component* childAt(int index) const;
    QList<Component*> childItems() const;

    bool isEnabled() const;
    void setEnabled(bool enabled);

    bool isTristate() const;
    void setTristate(bool tristate);

    bool isCheckable() const;
    void setCheckable(bool checkable);

    void setSelectable(bool selectable);

    bool isExpandedByDefault() const;
    void setExpandedByDefault(bool expandedByDefault);

    InstallAction installAction() const;
    void setInstallAction(InstallAction action);

    Qt::ItemFlags flags() const;
    void setFlags(Qt::ItemFlags flags);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

    QVariant data(int role = Qt::UserRole + 1) const;
    void setData(const QVariant &value, int role = Qt::UserRole + 1);

protected:
    void setPrivate(ComponentPrivate *componentPrivate);

private:
    void changeFlags(bool enable, Qt::ItemFlags itemFlags);

private:
    QHash<int, QVariant> m_values;

    ComponentPrivate *m_componentPrivate;
};

} // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::ComponentModelHelper::InstallAction)

#endif  // COMPONENT_P_H
