//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "console/computer_dialog.h"

#include <QDateTime>
#include <QMessageBox>

#include "client/ui/desktop_config_dialog.h"
#include "common/user_util.h"
#include "console/computer_group_item.h"

namespace aspia {

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kMinNameLength = 1;
constexpr int kMaxCommentLength = 2048;

} // namespace

ComputerDialog::ComputerDialog(QWidget* parent,
                               Mode mode,
                               proto::address_book::Computer* computer,
                               proto::address_book::ComputerGroup* parent_computer_group)
    : QDialog(parent),
      mode_(mode),
      computer_(computer)
{
    ui.setupUi(this);

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/img/monitor-keyboard.png")),
                                     tr("Desktop Manage"),
                                     QVariant(proto::SESSION_TYPE_DESKTOP_MANAGE));

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/img/monitor.png")),
                                     tr("Desktop View"),
                                     QVariant(proto::SESSION_TYPE_DESKTOP_VIEW));

    ui.combo_session_config->addItem(QIcon(QStringLiteral(":/img/folder-stand.png")),
                                     tr("File Transfer"),
                                     QVariant(proto::SESSION_TYPE_FILE_TRANSFER));

    ui.edit_parent_name->setText(QString::fromStdString(parent_computer_group->name()));
    ui.edit_name->setText(QString::fromStdString(computer_->name()));
    ui.edit_address->setText(QString::fromStdString(computer_->address()));
    ui.spinbox_port->setValue(computer_->port());
    ui.edit_username->setText(QString::fromStdString(computer_->username()));
    ui.edit_password->setText(QString::fromStdString(computer->password()));
    ui.edit_comment->setPlainText(QString::fromStdString(computer->comment()));

    connect(ui.combo_session_config, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ComputerDialog::sessionTypeChanged);

    connect(ui.button_show_password, &QPushButton::toggled,
            this, &ComputerDialog::showPasswordButtonToggled);

    connect(ui.button_session_config, &QPushButton::released,
            this, &ComputerDialog::sessionConfigButtonPressed);

    connect(ui.button_box, &QDialogButtonBox::clicked,
            this, &ComputerDialog::buttonBoxClicked);
}

void ComputerDialog::sessionTypeChanged(int item_index)
{
    proto::SessionType session_type = static_cast<proto::SessionType>(
        ui.combo_session_config->itemData(item_index).toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
            ui.button_session_config->setEnabled(true);
            break;

        default:
            ui.button_session_config->setEnabled(false);
            break;
    }
}

void ComputerDialog::showPasswordButtonToggled(bool checked)
{
    if (checked)
    {
        ui.edit_password->setEchoMode(QLineEdit::Normal);
        ui.edit_password->setInputMethodHints(Qt::ImhNone);
    }
    else
    {
        ui.edit_password->setEchoMode(QLineEdit::Password);
        ui.edit_password->setInputMethodHints(Qt::ImhHiddenText | Qt::ImhSensitiveData |
                                              Qt::ImhNoAutoUppercase | Qt::ImhNoPredictiveText);
    }
}

void ComputerDialog::sessionConfigButtonPressed()
{
    proto::SessionType session_type =
        static_cast<proto::SessionType>(ui.combo_session_config->currentData().toInt());

    switch (session_type)
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        {
            DesktopConfigDialog dialog(session_type,
                                       computer_->session_config().desktop_manage(),
                                       this);
            if (dialog.exec() == QDialog::Accepted)
            {
                computer_->mutable_session_config()->mutable_desktop_manage()->CopyFrom(
                    dialog.config());
            }
        }
        break;

        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            DesktopConfigDialog dialog(session_type,
                                       computer_->session_config().desktop_view(),
                                       this);
            if (dialog.exec() == QDialog::Accepted)
            {
                computer_->mutable_session_config()->mutable_desktop_view()->CopyFrom(
                    dialog.config());
            }
        }
        break;

        default:
            break;
    }
}

void ComputerDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui.button_box->standardButton(button) == QDialogButtonBox::Ok)
    {
        QString name = ui.edit_name->text();
        if (name.length() > kMaxNameLength)
        {
            showError(tr("Too long name. The maximum length of the name is %n characters.",
                         "", kMaxNameLength));
            ui.edit_name->setFocus();
            return;
        }
        else if (name.length() < kMinNameLength)
        {
            showError(tr("Name can not be empty."));
            ui.edit_name->setFocus();
            return;
        }

        QString username = ui.edit_username->text();
        QString password = ui.edit_password->text();

        if (!username.isEmpty() && !UserUtil::isValidUserName(username))
        {
            showError(tr("The user name can not be empty and can contain only"
                         " alphabet characters, numbers and ""_"", ""-"", ""."" characters."));
            ui.edit_name->setFocus();
            return;
        }

        QString comment = ui.edit_comment->toPlainText();
        if (comment.length() > kMaxCommentLength)
        {
            showError(tr("Too long comment. The maximum length of the comment is %n characters.",
                         "", kMaxCommentLength));
            ui.edit_comment->setFocus();
            return;
        }

        int64_t current_time = QDateTime::currentSecsSinceEpoch();

        if (mode_ == CreateComputer)
            computer_->set_create_time(current_time);

        computer_->set_modify_time(current_time);
        computer_->set_name(name.toStdString());
        computer_->set_address(ui.edit_address->text().toStdString());
        computer_->set_port(ui.spinbox_port->value());
        computer_->set_username(username.toStdString());
        computer_->set_password(password.toStdString());
        computer_->set_comment(comment.toStdString());

        accept();
    }
    else
    {
        reject();
    }

    close();
}

void ComputerDialog::showError(const QString& message)
{
    QMessageBox(QMessageBox::Warning, tr("Warning"), message, QMessageBox::Ok, this).exec();
}

} // namespace aspia
