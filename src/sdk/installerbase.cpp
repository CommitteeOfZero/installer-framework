/* Copyright (C) 2026 The Qt Company Ltd.
 *
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
*/

#include "installerbase.h"

#include "installerbasecommons.h"
#include "tabcontroller.h"

#include <init.h>
#include <messageboxhandler.h>
#include <packagemanagercore.h>
#include <globals.h>
#include <runoncechecker.h>

#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>

#ifdef ENABLE_SQUISH
#include <qtbuiltinhook.h>
#endif

InstallerBase::InstallerBase(int &argc, char *argv[])
    : SDKApp<QApplication>(argc, argv)
{
    QInstaller::init(); // register custom operations
#ifdef Q_OS_MACOS
    // The native macOS style draws blank checkboxes in the component tree.
    setStyle(QLatin1String("Fusion"));
#endif
}

InstallerBase::~InstallerBase()
{
    delete m_core;
}

int InstallerBase::run()
{
    QString errorMessage;
    if (!init(errorMessage)) {
        QInstaller::MessageBoxHandler::information(nullptr, QLatin1String("UnableToStart"),
            tr("Unable to start installer"), errorMessage);
        return EXIT_FAILURE;
    }

    if (m_parser.isSet(CommandLineOptions::scShowVirtualComponentsLong)) {
        QFont f;
        f.setItalic(true);
        QInstaller::PackageManagerCore::setVirtualComponentsFont(f);
    }

    qCDebug(QInstaller::lcInstallerInstallLog) << "Language:" << QLocale().uiLanguages()
        .value(0, QLatin1String("No UI language set")).toUtf8().constData();

    {
        QDirIterator fontIt(QStringLiteral(":/fonts"));
        while (fontIt.hasNext()) {
            const QString path = fontIt.next();
            qCDebug(QInstaller::lcDeveloperBuild) << "Registering custom font" << path;
            if (QFontDatabase::addApplicationFont(path) == -1)
                qCWarning(QInstaller::lcDeveloperBuild) << "Failed to register font!";
        }
    }
    //create the wizard GUI
    TabController controller(nullptr);
    controller.setManager(m_core);
    controller.setControlScript(controlScript());
    if (m_core->isInstaller())
        controller.setGui(new InstallerGui(m_core));
    else
        controller.setGui(new MaintenanceGui(m_core));
    emit m_core->guiElementsReady();

    QInstaller::PackageManagerCore::Status status =
        QInstaller::PackageManagerCore::Status(controller.init());
    if (status != QInstaller::PackageManagerCore::Success)
        return status;

    m_core->saveGivenArguments(m_parser.optionNames());

#ifdef ENABLE_SQUISH
    if (m_parser.isSet(CommandLineOptions::scSquishPortLong)) {
        const int maxSquishPortNumber = 65535;
        int squishPort = m_parser.value(CommandLineOptions::scSquishPortLong).toInt();
        if (squishPort <= 0 || squishPort > maxSquishPortNumber) {
            qWarning().noquote() << "Invalid Squish port:" << squishPort;
        } else {
            bool attachSucceeded = Squish::allowAttaching(squishPort);
            qCDebug(QInstaller::lcDeveloperBuild)  << "Attach to squish port" << squishPort << "succeeded: "<<attachSucceeded;
        }
    }
#endif
    const int result = QCoreApplication::instance()->exec();
    if (result != 0)
        return result;

    if (m_core->finishedWithSuccess())
        return QInstaller::PackageManagerCore::Success;

    status = m_core->status();
    switch (status) {
        case QInstaller::PackageManagerCore::Success:
            return status;

        case QInstaller::PackageManagerCore::Canceled:
            return status;

        default:
            break;
    }
    return QInstaller::PackageManagerCore::Failure;

}
