#include "ImGuiDatePicker.hpp"
#include <imgui_internal.h>
#include <cstdint>
#include <chrono>
#include <vector>
#include <unordered_map>


#define GET_DAY(timePoint) int(timePoint.tm_mday)
#define GET_MONTH(timePoint) int(timePoint.tm_mon + 1)
#define GET_YEAR(timePoint) int(timePoint.tm_year + 1900)

#define SET_DAY(timePoint, day) timePoint.tm_mday = day
#define SET_MONTH(timePoint, month) timePoint.tm_mon = month - 1
#define SET_YEAR(timePoint, year) timePoint.tm_year = year - 1900

namespace ImGui
{
    static const std::vector<std::string> MONTHS =
    {
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    };

    static const std::vector<std::string> DAYS =
    {
        "Mo",
        "Tu",
        "We",
        "Th",
        "Fr",
        "Sa",
        "Su"
    };

    // Implements Zeller's Congruence to determine the day of week [1, 7](Mon-Sun) from the given parameters
    inline static uint32_t DayOfWeek(uint32_t dayOfMonth, uint32_t month, uint32_t year) noexcept
    {
        if ((month == 1) || (month == 2))
        {
            month += 12;
            year -= 1;
        }

        uint32_t h = (dayOfMonth
            + static_cast<uint32_t>(std::floor((13 * (month + 1)) / 5.0))
            + year
            + static_cast<uint32_t>(std::floor(year / 4.0))
            - static_cast<uint32_t>(std::floor(year / 100.0))
            + static_cast<uint32_t>(std::floor(year / 400.0))) % 7;

        return static_cast<uint32_t>(std::floor(((h + 5) % 7) + 1));
    }

    constexpr static bool IsLeapYear(uint32_t year) noexcept
    {
        if ((year % 400) == 0)
            return true;

        if ((year % 4 == 0) && ((year % 100) != 0))
            return true;

        return false;
    }

    inline static uint32_t NumDaysInMonth(uint32_t month, uint32_t year)
    {
        if (month == 2)
            return IsLeapYear(year) ? 29 : 28;

        // Month index paired to the number of days in that month excluding February
        static const std::unordered_map<uint32_t, uint32_t> monthDayMap =
        {
            { 1,  31 },
            { 3,  31 },
            { 4,  30 },
            { 5,  31 },
            { 6,  30 },
            { 7,  31 },
            { 8,  31 },
            { 9,  30 },
            { 10, 31 },
            { 11, 30 },
            { 12, 31 }
        };

        return monthDayMap.at(month);
    }

    // Returns the number of calendar weeks spanned by month in the specified year
    inline static uint32_t NumWeeksInMonth(uint32_t month, uint32_t year)
    {
        uint32_t days = NumDaysInMonth(month, year);
        uint32_t firstDay = DayOfWeek(1, month, year);

        return static_cast<uint32_t>(std::ceil((days + firstDay - 1) / 7.0));
    }

    // Returns a vector containing dates as they would appear on the calendar for a given week. Populates 0 if there is no day.
    inline static std::vector<uint32_t> CalendarWeek(uint32_t week, uint32_t startDay, uint32_t daysInMonth)
    {
        std::vector<uint32_t> res(7, 0);
        int startOfWeek = 7 * (week - 1) + 2 - startDay;

        if (startOfWeek >= 1)
            res[0] = (uint32_t)startOfWeek;

        for (uint32_t i = 1; i < 7; ++i)
        {
            int day = startOfWeek + i;
            if ((day >= 1) && (day <= (int)daysInMonth))
                res[i] = day;
        }

        return res;
    }

    constexpr static tm EncodeTimePoint(int dayOfMonth, int month, int year) noexcept
    {
        tm res{ };
        res.tm_mday = dayOfMonth;
        res.tm_mon = month - 1;
        res.tm_year = year - 1900;
        res.tm_isdst = -1;

        return res;
    }

    inline static tm Today() noexcept
    {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::time_t currentTime = std::chrono::system_clock::to_time_t(now);

        return *std::gmtime(&currentTime);
    }

    inline static tm PreviousMonth(const tm& timePoint) noexcept
    {
        int month = GET_MONTH(timePoint);
        int year = GET_YEAR(timePoint);

        if (month == 1)
        {
            uint32_t newDay = std::min((uint32_t)GET_DAY(timePoint), NumDaysInMonth(12, --year));
            return EncodeTimePoint(newDay, 12, year);
        }

        uint32_t newDay = std::min((uint32_t)GET_DAY(timePoint), NumDaysInMonth(--month, year));
        return EncodeTimePoint(newDay, month, year);
    }

    inline static tm NextMonth(const tm& timePoint) noexcept
    {
        int month = GET_MONTH(timePoint);
        int year = GET_YEAR(timePoint);

        if (month == 12)
        {
            uint32_t newDay = std::min((uint32_t)GET_DAY(timePoint), NumDaysInMonth(1, ++year));
            return EncodeTimePoint(newDay, 1, year);
        }

        uint32_t newDay = std::min((uint32_t)GET_DAY(timePoint), NumDaysInMonth(++month, year));
        return EncodeTimePoint(newDay, month, year);
    }

    constexpr static bool IsMinDate(const tm& timePoint) noexcept
    {
        return (GET_MONTH(timePoint) == 1) && (GET_YEAR(timePoint) == IMGUI_DATEPICKER_YEAR_MIN);
    }

    constexpr static bool IsMaxDate(const tm& timePoint) noexcept
    {
        return (GET_MONTH(timePoint) == 12) && (GET_YEAR(timePoint) == IMGUI_DATEPICKER_YEAR_MAX);
    }

    static bool ComboBox(const std::string& label, const std::vector<std::string>& items, int& v)
    {
        bool res = false;

        ImGuiIO& io = ImGui::GetIO();   // TODO: a better way must be found to supply the bold font. Rename it alt font
        auto boldFont = io.Fonts->Fonts[1];

        ImGui::PushFont(boldFont);
        if (ImGui::BeginCombo(label.c_str(), items[v].c_str()))
        {
            for (int i = 0; i < items.size(); ++i)
            {
                bool selected = (items[v] == items[i]);
                if (ImGui::Selectable(items[i].c_str(), &selected))
                {
                    v = i;
                    res = true;
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        ImGui::PopFont();
        return res;
    }

    bool DatePicker(const std::string& label, tm& v, bool clampToBorder, float itemSpacing)
    {
        bool res = false;

    //    ImGuiContext& g = *GImGui;
        ImGuiWindow* window = GetCurrentWindow();

        if (window->SkipItems)
            return false;

        ImGuiIO& io = GetIO();
        auto boldFont = io.Fonts->Fonts[1]; // TODO: a better way must be found to supply the bold font. Rename it alt font

        bool hiddenLabel = label.substr(0, 2) == "##";
        std::string myLabel = (hiddenLabel) ? label.substr(2) : label;

        if (!hiddenLabel)
        {
            Text(label.c_str());
            SameLine((itemSpacing == 0.0f) ? 0.0f : GetCursorPos().x + itemSpacing);
        }

        if (clampToBorder)
            SetNextItemWidth(GetContentRegionAvail().x);

        const ImVec2 windowSize = ImVec2(274.5f, 301.5f);
        SetNextWindowSize(windowSize);

        // TODO: Stop using fmtlib
        if (BeginCombo(fmt::format("##{0}", myLabel).c_str(), v.ToLongString().c_str()))
        {
            int monthIdx = v.GetMonth() - 1;
            int year = v.GetYear();

            PushItemWidth((GetContentRegionAvail().x * 0.5f));

            if (ComboBox(fmt::format("##CmbMonth_{0}", myLabel), MONTHS, monthIdx))
                v.SetMonth(monthIdx + 1);

            PopItemWidth();
            SameLine();
            PushItemWidth(GetContentRegionAvail().x);

            if (InputInt(fmt::format("##IntYear_{0}", myLabel).c_str(), &year))
            {
                year = std::min(std::max(MIN_YEAR, year), MAX_YEAR);
                v.SetYear(year);
            }

            PopItemWidth();

            const float contentWidth = GetContentRegionAvail().x;
            const float arrowSize = GetFrameHeight();
            const float arrowButtonWidth = arrowSize * 2.0f + GetStyle().ItemSpacing.x;
            const float bulletSize = arrowSize - 5.0f;
            const float bulletButtonWidth = bulletSize + GetStyle().ItemSpacing.x;
            const float combinedWidth = arrowButtonWidth + bulletButtonWidth;
            const float offset = (contentWidth - combinedWidth) * 0.5f;

            SetCursorPosX(GetCursorPosX() + offset);
            PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
            PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            BeginDisabled(IsMinDate(v));

            if (ArrowButtonEx(fmt::format("##ArrowLeft_{0}", myLabel).c_str(), ImGuiDir_Left, ImVec2(arrowSize, arrowSize)))
                v = PreviousMonth(v);

            EndDisabled();
            PopStyleColor(2);
            SameLine();
            PushStyleColor(ImGuiCol_Button, GetStyleColorVec4(ImGuiCol_Text));
            SetCursorPosY(GetCursorPosY() + 2.0f);

            if (ButtonEx(fmt::format("##ArrowMid_{0}", myLabel).c_str(), ImVec2(bulletSize, bulletSize)))
            {
                v = Date::Today();
                res = true;
                CloseCurrentPopup();
            }

            PopStyleColor();
            SameLine();
            PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            BeginDisabled(IsMaxDate(v));

            if (ArrowButtonEx(fmt::format("##ArrowRight_{0}", myLabel).c_str(), ImGuiDir_Right, ImVec2(arrowSize, arrowSize)))
                v = NextMonth(v);

            EndDisabled();
            PopStyleColor(2);
            PopStyleVar();

            constexpr ImGuiTableFlags TABLE_FLAGS = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit |
                ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_NoHostExtendY;

            if (BeginTable(fmt::format("##Table_{0}", myLabel).c_str(), 7, TABLE_FLAGS, GetContentRegionAvail()))
            {
                for (const auto& day : DAYS)
                    TableSetupColumn(day.c_str(), ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHeaderWidth, 30.0f);

                PushStyleColor(ImGuiCol_HeaderHovered, GetStyleColorVec4(ImGuiCol_TableHeaderBg));
                PushStyleColor(ImGuiCol_HeaderActive, GetStyleColorVec4(ImGuiCol_TableHeaderBg));
                PushFont(boldFont);
                TableHeadersRow();
                PopStyleColor();
                PopFont();

                TableNextRow();
                TableSetColumnIndex(0);

                uint32_t month = monthIdx + 1;
                uint32_t firstDayOfMonth = DayOfWeek(1, month, (uint32_t)year);
                uint32_t numDaysInMonth = NumDaysInMonth(month, (uint32_t)year);
                uint32_t numWeeksInMonth = NumWeeksInMonth(month, (uint32_t)year);

                for (uint32_t i = 1; i <= numWeeksInMonth; ++i)
                {
                    for (const auto& day : CalendarWeek(i, firstDayOfMonth, numDaysInMonth))
                    {
                        if (day != 0)
                        {
                            PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);

                            const bool selected = day == v.GetDayOfMonth();
                            if (!selected)
                            {
                                PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                                PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                            }

                            if (Button(std::to_string(day).c_str(), ImVec2(GetContentRegionAvail().x, GetTextLineHeightWithSpacing() + 5.0f)))
                            {
                                v = Date::EncodeDate(day, month, (uint32_t)year);
                                res = true;
                                CloseCurrentPopup();
                            }

                            if (!selected)
                                PopStyleColor(2);

                            PopStyleVar();
                        }

                        if (day != numDaysInMonth)
                            TableNextColumn();
                    }
                }

                EndTable();
            }

            EndCombo();
        }

        return res;
    }
}
