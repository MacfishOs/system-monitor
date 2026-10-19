// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ui/view.h"

#include <functional>
#include <string>
#include <vector>

class HistoryGraph;

namespace cutefish::ui {
class Icon;
class Label;
} // namespace cutefish::ui

// A selectable plate in the sidebar: draws its selection and focus, and
// clicks select it.
class SidebarButton : public cutefish::ui::View
{
public:
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

    bool acceptsPointer() const override { return true; }
    // A Tab stop, as a FishUI button is; Space clicks it.
    bool acceptsFocus() const override { return true; }

    std::function<void()> onClicked;

protected:
    // The selection tint and its border.
    virtual cutefish::ui::Color accentColor() const = 0;
    void paint(cutefish::ui::Painter& painter) override;
    void pointerPress(const cutefish::ui::PointerEvent& event) override;
    void pointerRelease(const cutefish::ui::PointerEvent& event) override;
    bool keyPress(const cutefish::ui::KeyEvent& event) override;
    bool keyRelease(const cutefish::ui::KeyEvent& event) override;

private:
    bool m_selected = false;
    bool m_pressed = false;
    bool m_spacePressed = false;
};

// The sidebar's first row, which shows the process list.
class ProcessesButton : public SidebarButton
{
public:
    ProcessesButton();

    void setCount(std::string count);

    cutefish::ui::SizeF sizeHint() const override { return {0, 36}; }
    // Not a Tab stop, unlike the cards.
    bool acceptsFocus() const override { return false; }

protected:
    cutefish::ui::Color accentColor() const override;
    void layout() override;

private:
    cutefish::ui::Icon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_count;
};

// A resource's card in the sidebar: its value now and a minute of history.
class SummaryCard : public SidebarButton
{
public:
    SummaryCard(std::string title, std::string icon, cutefish::ui::Color color);

    void setValue(std::string value);
    void setSubtitle(std::string subtitle);
    void setHistory(std::vector<double> values, std::vector<int64_t> timestamps, int64_t duration);
    void setMaximum(double maximum);
    void setScaleFloor(double floor);

    cutefish::ui::SizeF sizeHint() const override { return {0, 96}; }

protected:
    cutefish::ui::Color accentColor() const override { return m_color; }
    void layout() override;

private:
    cutefish::ui::Color m_color;
    cutefish::ui::Icon* m_icon;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_value;
    cutefish::ui::Label* m_subtitle;
    HistoryGraph* m_graph;
};
