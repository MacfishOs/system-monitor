// SPDX-FileCopyrightText: 2026 Reion Wong <reionwong@gmail.com>
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "components.h"

#include <cstdint>
#include <string>
#include <vector>

class HistoryGraph;

namespace cutefish::ui {
class ColumnLayout;
}

// A resource's minute of history at full width, with its current value.
class ChartCard : public Plate
{
public:
    ChartCard();

    void setTitle(std::string title);
    void setValue(std::string value);
    void setSubtitle(std::string subtitle);
    void setColors(cutefish::ui::Color primary, cutefish::ui::Color secondary);
    void setSeries(std::vector<double> values, std::vector<double> secondaryValues, std::vector<int64_t> timestamps,
                   int64_t duration);
    void setMaximum(double maximum);
    // Unset: 100 % for a fixed maximum, the scale's rate otherwise.
    void setScaleLabel(std::string label);
    void setLegends(std::string primary, std::string secondary);
    void setDurationText(std::string text);

    HistoryGraph* graph() const { return m_graph; }

    cutefish::ui::SizeF sizeHint() const override { return {0, 276}; }

protected:
    void layout() override;

private:
    void updateScale();

    cutefish::ui::ColumnLayout* m_column;
    cutefish::ui::Label* m_title;
    cutefish::ui::Label* m_value;
    cutefish::ui::Label* m_subtitle;
    cutefish::ui::Label* m_scale;
    HistoryGraph* m_graph;
    cutefish::ui::Label* m_duration;
    cutefish::ui::Label* m_primaryLegend;
    cutefish::ui::Label* m_secondaryLegend;
    cutefish::ui::Label* m_now;
    std::string m_scaleText;
    double m_maximum = 100;
    cutefish::ui::Color m_primary;
    cutefish::ui::Color m_secondary;
};
