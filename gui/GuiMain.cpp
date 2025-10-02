#include "GuiMain.h"

#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QPointer>
#include <QMessageBox>
#include <QTimer>

#include <CxxPtr/GlibPtr.h>

#include "../StreamerMain.h"

#include "Theme.h"
#include "StreamerEditDialog.h"
#include "NotificationProxy.h"


namespace {

QPointer<StreamerEditDialog> activeEditDialog;

void ActivateEditDialog()
{
    if(activeEditDialog){
        activeEditDialog->raise();
        activeEditDialog->activateWindow();
    }
}

void ShowEditDialog(
    Config* config,
    const std::string& streamerId,
    Config::ReStreamer* streamer = nullptr)
{
    if(activeEditDialog){
        return;
    } else {
        activeEditDialog = new StreamerEditDialog(config, streamerId, streamer);
        activeEditDialog->setAttribute(Qt::WA_DeleteOnClose);
        activeEditDialog->exec();
    }
}

void RefreshTrayMenu(Config* config, QMenu* menu)
{
    menu->clear();

    QAction* addAction = menu->addAction(
        Theme::icon("plus"),
        "Add new Streamer...");
    if(activeEditDialog) {
        addAction->setEnabled(false);
    } else {
        QObject::connect(addAction, &QAction::triggered, [config] () {
            GCharPtr uniqueIdPtr(g_uuid_string_random());
            ShowEditDialog(config, uniqueIdPtr.get());
        });
    }

    if(!config->reStreamersOrder.empty()) {
        menu->addSeparator();
        for(const std::string& id: config->reStreamersOrder) {
            auto& reStreamers = config->reStreamers;
            const auto it = reStreamers.find(id);
            if(it == reStreamers.end())
                continue;

            Config::ReStreamer& reStreamer = it->second;
            const QString description = QString::fromStdString(reStreamer.description).trimmed();
            const QString targetUrl = QString::fromStdString(reStreamer.targetUrl);
            QMenu* streamerMenu = menu->addMenu(
                reStreamer.enabled ? Theme::icon("video") : Theme::icon("video-off"),
                !description.isEmpty() ? description : targetUrl);
            if(activeEditDialog) {
                streamerMenu->setEnabled(false);
            } else {
                if(reStreamer.enabled) {
                    QAction* disableAction = streamerMenu->addAction(
                        Theme::icon("video-off"),
                        "Disable");
                    QObject::connect(disableAction, &QAction::triggered, [config, &id, &reStreamer] () {
                        reStreamer.enabled = false;

                        std::unique_ptr<ConfigChanges> changes = std::make_unique<ConfigChanges>();
                        ConfigChanges::ReStreamerChanges& reStreamerchanges =
                            changes->reStreamersChanges[id];
                        reStreamerchanges.enabled = reStreamer.enabled;

                        PostConfigChanges(std::move(changes));
                        SaveAppConfig(*config);
                    });
                } else {
                    QAction* enableAction = streamerMenu->addAction(
                        Theme::icon("video"),
                        "Enable");
                    QObject::connect(enableAction, &QAction::triggered, [config, &id, &reStreamer] () {
                        reStreamer.enabled = true;

                        std::unique_ptr<ConfigChanges> changes = std::make_unique<ConfigChanges>();
                        ConfigChanges::ReStreamerChanges& reStreamerchanges =
                            changes->reStreamersChanges[id];
                        reStreamerchanges.enabled = reStreamer.enabled;

                        PostConfigChanges(std::move(changes));
                        SaveAppConfig(*config);
                    });
                }
                QAction* editAction = streamerMenu->addAction(
                    Theme::icon("square-pen"),
                    "Edit...");
                QObject::connect(editAction, &QAction::triggered, [config, &id, &reStreamer] () {
                    ShowEditDialog(config, id, &reStreamer);
                });
                QAction* dropAction = streamerMenu->addAction(
                    Theme::icon("trash"),
                    "Drop");
                QObject::connect(dropAction, &QAction::triggered, [config, &id] () {
                    auto& reStreamers = config->reStreamers;

                    reStreamers.erase(id);

                    std::unique_ptr<ConfigChanges> changes = std::make_unique<ConfigChanges>();
                    ConfigChanges::ReStreamerChanges& reStreamerchanges =
                        changes->reStreamersChanges[id];
                    reStreamerchanges.enabled = false;
                    reStreamerchanges.drop = true;

                    PostConfigChanges(std::move(changes));
                    SaveAppConfig(*config);
                });
            }
        }
    }

    menu->addSeparator();
    QAction* quitAction = menu->addAction(
        Theme::icon("log-out"),
        "Quit");
    QObject::connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    if(activeEditDialog) {
        ActivateEditDialog();
    }
}

Q_GLOBAL_STATIC(NotificationProxy, notificationProxy)

}

int GuiMain(int argc, char *argv[], Config* config)
{
    QApplication app(argc, argv);

    if(!QSystemTrayIcon::isSystemTrayAvailable()) {
        return -1;
    }

    app.setWindowIcon(Theme::icon("video"));

    app.setQuitOnLastWindowClosed(false);

    QMenu trayMenu;
    RefreshTrayMenu(config, &trayMenu);
    QObject::connect(&trayMenu, &QMenu::aboutToShow, [config, &trayMenu] () {
        RefreshTrayMenu(config, &trayMenu);
    });

    QSystemTrayIcon trayIcon(Theme::icon("video"));
    trayIcon.setContextMenu(&trayMenu);
    QObject::connect(&trayIcon, &QSystemTrayIcon::activated, [] () {
        ActivateEditDialog();
    });
    trayIcon.show();

    QObject::connect(
        notificationProxy,
        &NotificationProxy::notification,
        &trayMenu,
        [config, &trayIcon] (const std::string& streamerId, NotificationType type) {
            auto it = config->reStreamers.find(streamerId);
            if(it == config->reStreamers.end())
                return;

            QString message;
            switch(type) {
                case NotificationType::Eos:
                    return;
                case NotificationType::SourceError:
                    message = "Failed to receive data from stream source...";
                    break;
                case NotificationType::TargetError:
                    message = "Failed to send data to stream target...";
                    break;
                case NotificationType::OtherError:
                    message = "Something went wrong...";
                    break;
            }

            const Config::ReStreamer& reStreamer = it->second;

            trayIcon.showMessage(
                QString::fromStdString(reStreamer.description),
                message,
                QSystemTrayIcon::Warning,
                3000);
        },
        Qt::QueuedConnection);

    return app.exec();
}

void PostNotification(const std::string& streamerId, NotificationType type)
{
    notificationProxy->postNotification(streamerId, type);
}
