// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "processespage.h"

#include "base/signal.h"
#include "ui/dialog_window.h"

#include <memory>

class Monitor;

// A process's or application's details, kept current while open.
class ProcessDetails
{
public:
    ProcessDetails(cutefish::ui::Window* parent, Monitor& monitor, const Selection& target);

    void show();
    cutefish::ui::Window* window() const { return m_dialog.window(); }

    std::function<void()> onClosed;

private:
    class Content;

    void update();

    Monitor& m_monitor;
    Selection m_target;
    cutefish::ui::DialogWindow m_dialog;
    Content* m_content;
    cutefish::base::Subscription m_updated;
};
