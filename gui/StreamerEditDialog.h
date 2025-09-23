#pragma once

#include <optional>

#include <QDialog>

#include "../Config.h"


namespace Ui {
class StreamerEditDialog;
}

class StreamerEditDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StreamerEditDialog(
        Config* config,
        const std::string& streamerId,
        Config::ReStreamer* reStreamer = nullptr);
    ~StreamerEditDialog();

    void accept() override;

private:
    void validate();

private:
    Ui::StreamerEditDialog* _ui;
    Config *const _config;
    const std::string _streamerId;
    Config::ReStreamer *const _reStreamer;
};
