#pragma once

#include <QObject>

#include "../Types.h"


class NotificationProxy: public QObject
{
    Q_OBJECT

public:
    void postNotification(const std::string& streamerId, NotificationType type) {
        emit notification(streamerId, type);
    }

signals:
    void notification(const std::string& streamerId, NotificationType);
};
