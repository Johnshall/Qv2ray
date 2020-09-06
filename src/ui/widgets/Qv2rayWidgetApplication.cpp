#include "Qv2rayWidgetApplication.hpp"

#include "3rdparty/libsemver/version.hpp"
#include "base/Qv2rayBase.hpp"
#include "components/translations/QvTranslator.hpp"
#include "core/settings/SettingsBackend.hpp"
#include "ui/widgets/styles/StyleManager.hpp"
#include "ui/widgets/windows/w_MainWindow.hpp"
#include "utils/QvHelpers.hpp"

#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_WIN
    #include <Winbase.h>
#endif

namespace Qv2ray
{
    Qv2rayWidgetApplication::Qv2rayWidgetApplication(int &argc, char *argv[]) : Qv2rayPlatformApplication(argc, argv)
    {
    }

    Qv2raySetupStatus Qv2rayWidgetApplication::Initialize()
    {
        const auto result = InitializeInternal();
        if (result != NORMAL)
            return result;
        setQuitOnLastWindowClosed(false);
        hTray = new QSystemTrayIcon();
        return NORMAL;
    }

    void Qv2rayWidgetApplication::TerminateUI()
    {
        delete mainWindow;
        delete hTray;
        delete StyleManager;
    }

#ifndef QV2RAY_NO_SINGLEAPPLICATON
    void Qv2rayWidgetApplication::onMessageReceived(quint32 clientId, QByteArray _msg)
    {
        // Sometimes SingleApplication will send message with clientId == 0, ignore them.
        if (clientId == instanceId())
            return;
        const auto msg = Qv2rayProcessArguments::fromJson(JsonFromString(_msg));
        LOG(MODULE_INIT, "Client ID: " + QSTRN(clientId) + ", message received, version: " + msg.version)
        DEBUG(MODULE_INIT, _msg)
        //
        const auto currentVersion = semver::version::from_string(QV2RAY_VERSION_STRING);
        const auto newVersionString = msg.version.isEmpty() ? "0.0.0" : msg.version.toStdString();
        const auto newVersion = semver::version::from_string(newVersionString);
        //
        if (newVersion > currentVersion)
        {
            const auto newPath = msg.fullArgs.first();
            QString message;
            message += tr("A new version of Qv2ray is attemping to start:") + NEWLINE;
            message += NEWLINE;
            message += tr("New version information: ") + NEWLINE;
            message += tr("Qv2ray version: %1").arg(msg.version) + NEWLINE;
            message += tr("Qv2ray path: %1").arg(newPath) + NEWLINE;
            message += NEWLINE;
            message += tr("Do you want to exit and launch that new version?");

            const auto result = QvMessageBoxAsk(nullptr, tr("New version detected"), message);
            if (result == Yes)
            {
                Qv2rayProcessArgument._qvNewVersionPath = newPath;
                QuitApplication(QVEXIT_NEW_VERSION);
            }
        }

        for (const auto &argument : msg.arguments)
        {
            switch (argument)
            {
                case Qv2rayProcessArguments::EXIT:
                {
                    QuitApplication();
                    break;
                }
                case Qv2rayProcessArguments::NORMAL:
                {
                    mainWindow->show();
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    break;
                }
                case Qv2rayProcessArguments::RECONNECT:
                {
                    ConnectionManager->RestartConnection();
                    break;
                }
                case Qv2rayProcessArguments::DISCONNECT:
                {
                    ConnectionManager->StopConnection();
                    break;
                }
                case Qv2rayProcessArguments::QV2RAY_LINK:
                {
                    for (const auto &link : msg.links)
                    {
                        const auto url = QUrl::fromUserInput(link);
                        const auto command = url.host();
                        auto subcommands = url.path().split("/");
                        subcommands.removeAll("");
                        QMap<QString, QString> args;
                        for (const auto &kvp : QUrlQuery(url).queryItems())
                        {
                            args.insert(kvp.first, kvp.second);
                        }
                        if (command == "open")
                        {
                            emit mainWindow->ProcessCommand(command, subcommands, args);
                        }
                    }
                    break;
                }
            }
        }
    }
#endif

    Qv2rayExitCode Qv2rayWidgetApplication::RunQv2ray()
    {
        RunInternal();
        StyleManager = new QvStyleManager();
        StyleManager->ApplyStyle(GlobalConfig.uiConfig.theme);
        // Show MainWindow
        mainWindow = new MainWindow();
        if (Qv2rayProcessArgument.arguments.contains(Qv2rayProcessArguments::QV2RAY_LINK))
        {
            for (const auto &link : Qv2rayProcessArgument.links)
            {
                const auto url = QUrl::fromUserInput(link);
                const auto command = url.host();
                auto subcommands = url.path().split("/");
                subcommands.removeAll("");
                QMap<QString, QString> args;
                for (const auto &kvp : QUrlQuery(url).queryItems())
                {
                    args.insert(kvp.first, kvp.second);
                }
                if (command == "open")
                {
                    emit mainWindow->ProcessCommand(command, subcommands, args);
                }
            }
        }
#ifdef Q_OS_MACOS
        connect(this, &QApplication::applicationStateChanged, [this](Qt::ApplicationState state) {
            switch (state)
            {
                case Qt::ApplicationActive:
                {
                    mainWindow->show();
                    mainWindow->raise();
                    mainWindow->activateWindow();
                    break;
                }
                case Qt::ApplicationHidden:
                case Qt::ApplicationInactive:
                case Qt::ApplicationSuspended: break;
            }
        });
#endif
        return (Qv2rayExitCode) exec();
    }
} // namespace Qv2ray