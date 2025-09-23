#include "Theme.h"

#include <QGuiApplication>
#include <QStyleHints>


QIcon Theme::icon(const QString& name)
{
    using namespace Qt;

    ColorScheme currentScheme = QGuiApplication::styleHints()->colorScheme();
    switch(currentScheme) {
        case ColorScheme::Dark:
            return QIcon(":/icons/" + name + "-dark.svg");
        case ColorScheme::Unknown:
        case ColorScheme::Light:
        default:
            return QIcon(":/icons/" + name + "-light.svg");
    }
}
