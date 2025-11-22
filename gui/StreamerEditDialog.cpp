#include "StreamerEditDialog.h"
#include "ui_StreamerEditDialog.h"


#include <QPushButton>

#include "../StreamerMain.h"


StreamerEditDialog::StreamerEditDialog(
    Config* config,
    const std::string& streamerId,
    Config::ReStreamer *const reStreamer /*= nullptr*/):
    QDialog(nullptr),
    _ui(new Ui::StreamerEditDialog),
    _config(config),
    _streamerId(streamerId),
    _reStreamer(reStreamer)
{
    _ui->setupUi(this);

    QFont font = _ui->youTubeStudioLinkLabel->font();
    font.setPointSize(font.pointSize() - 1);
    font.setItalic(true);
    _ui->youTubeStudioLinkLabel->setFont(font);

    QPalette palette = _ui->youTubeStudioLinkLabel->palette();
    palette.setColor(QPalette::WindowText, palette.color(QPalette::PlaceholderText));
    _ui->youTubeStudioLinkLabel->setPalette(palette);

#if VK_VIDEO_STREAMER || YOUTUBE_LIVE_STREAMER
    _ui->keyLabel->setVisible(true);
    _ui->keyEdit->setVisible(true);
    _ui->targetUrlLabel->setVisible(false);
    _ui->targetUrlEdit->setVisible(false);
#else
    _ui->keyLabel->setVisible(false);
    _ui->keyEdit->setVisible(false);
    _ui->targetUrlLabel->setVisible(true);
    _ui->targetUrlEdit->setVisible(true);
#endif

#if !YOUTUBE_LIVE_STREAMER
    _ui->youTubeStudioLinkLabel->setVisible(false);
#endif

    if(reStreamer) {
        const QString description = QString::fromStdString(reStreamer->description).trimmed();
        setWindowTitle(description.isEmpty() ? tr("Edit streamer...") : description);
        _ui->nameEdit->setText(description);
        _ui->sourceUrlEdit->setText(QString::fromStdString(reStreamer->sourceUrl));
        _ui->targetUrlEdit->setText(QString::fromStdString(reStreamer->targetUrl));
        _ui->enabledCheckBox->setChecked(reStreamer->enabled);
    } else {
        setWindowTitle(tr("New streamer..."));
    }

    connect(_ui->nameEdit, &QLineEdit::textEdited, this, &StreamerEditDialog::validate);
    connect(_ui->sourceUrlEdit, &QLineEdit::textEdited, this, &StreamerEditDialog::validate);
    connect(_ui->targetUrlEdit, &QLineEdit::textEdited, this, &StreamerEditDialog::validate);
    connect(_ui->keyEdit, &QLineEdit::textEdited, this, &StreamerEditDialog::validate);
    connect(_ui->enabledCheckBox, &QCheckBox::checkStateChanged, this, &StreamerEditDialog::validate);

    validate();

    setFixedSize(size());
}

StreamerEditDialog::~StreamerEditDialog()
{
    delete _ui;
}

void StreamerEditDialog::validate()
{
    const bool valid =
        !_ui->nameEdit->text().trimmed().isEmpty() &&
        !_ui->sourceUrlEdit->text().trimmed().isEmpty() &&
#if VK_VIDEO_STREAMER || YOUTUBE_LIVE_STREAMER
        !_ui->keyEdit->text().trimmed().isEmpty();
#else
        !_ui->targetUrlEdit->text().trimmed().isEmpty();
#endif

    const QString description = _ui->nameEdit->text().trimmed();
    const QString sourceUrl = _ui->sourceUrlEdit->text().trimmed();
    const QString key = _ui->keyEdit->text().trimmed();
    const QString targetUrl = _ui->targetUrlEdit->text().trimmed();
    const bool enabled = _ui->enabledCheckBox->checkState() == Qt::CheckState::Checked;

    const bool changed =
        !_reStreamer || (
            description != QString::fromStdString(_reStreamer->description) ||
            sourceUrl != QString::fromStdString(_reStreamer->sourceUrl) ||
#if VK_VIDEO_STREAMER || YOUTUBE_LIVE_STREAMER
            (
                !key.isEmpty() &&
                Config::ReStreamer::BuildTargetUrl(key.toStdString()) != _reStreamer->targetUrl
            ) ||
#else
            targetUrl != QString::fromStdString(_reStreamer->targetUrl) ||
#endif
            enabled != _reStreamer->enabled
        );

    _ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(valid && changed);
}

void StreamerEditDialog::accept()
{
    const QString description = _ui->nameEdit->text().trimmed();
    const QString sourceUrl = _ui->sourceUrlEdit->text().trimmed();
    const QString key = _ui->keyEdit->text().trimmed();
#if VK_VIDEO_STREAMER || YOUTUBE_LIVE_STREAMER
    const QString targetUrl = key.isEmpty() ?
        QString() :
        QString::fromStdString(Config::ReStreamer::BuildTargetUrl(key.toStdString()));
#else
    const QString targetUrl = _ui->targetUrlEdit->text().trimmed();
#endif

    const bool enabled = _ui->enabledCheckBox->checkState() == Qt::CheckState::Checked;

    if(_reStreamer) { // editing
        const bool descriptionChanged = QString::fromStdString(_reStreamer->description) != description;
        const bool sourceUrlChanged = QString::fromStdString(_reStreamer->sourceUrl) != sourceUrl;
        const bool targetUrlChanged = QString::fromStdString(_reStreamer->targetUrl) != targetUrl;
        const bool enabledChanged = _reStreamer->enabled != enabled;

        if(descriptionChanged || sourceUrlChanged || targetUrlChanged || enabledChanged) {
            std::unique_ptr<ConfigChanges> changes = std::make_unique<ConfigChanges>();
            ConfigChanges::ReStreamerChanges& reStreamerchanges =
                changes->reStreamersChanges[_streamerId];

            if(descriptionChanged) {
                _reStreamer->description = description.toStdString();
                reStreamerchanges.description = description.toStdString();
            }
            if(sourceUrlChanged) {
                _reStreamer->sourceUrl = sourceUrl.toStdString();
                reStreamerchanges.sourceUrl = sourceUrl.toStdString();
            }
            if(targetUrlChanged) {
                _reStreamer->targetUrl = targetUrl.toStdString();
                reStreamerchanges.targetUrl = targetUrl.toStdString();
            }
            if(enabledChanged) {
                _reStreamer->enabled = enabled;
                reStreamerchanges.enabled = enabled;
            }

            PostConfigChanges(std::move(changes));
            SaveAppConfig(*_config);
        }
    } else { // new streamer
        std::unique_ptr<ConfigChanges> changes = std::make_unique<ConfigChanges>();
        ConfigChanges::ReStreamerChanges& reStreamerChanges =
            changes->reStreamersChanges[_streamerId];

        reStreamerChanges.description = description.toStdString();
        reStreamerChanges.sourceUrl = sourceUrl.toStdString();
        reStreamerChanges.targetUrl = targetUrl.toStdString();
        reStreamerChanges.enabled = enabled;

        _config->addReStreamer(_streamerId, reStreamerChanges.makeReStreamer());

        PostConfigChanges(std::move(changes));
        SaveAppConfig(*_config);
    }

    QDialog::accept();
}
