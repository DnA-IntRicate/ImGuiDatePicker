#include <mgpch.hpp>
#include <Mango/GUI/ImGuiDatePicker.hpp>
#include <Mango/GUI/ImGuiExtensions.hpp> // TODO: Remove these includes
#include <Mango/GUI/ImGuiWidgets.hpp>    // TODO: Remove these includes
#include <imgui_internal.h>
using namespace Mango;


namespace ImGui
{
    // This represents the year of the epoch for ActiveX Database Objects
    constexpr int MIN_YEAR = 1970;  // TODO: Adjust this, maybe define this in the header file with a conditional macro

    // The max year supported by the current implementation
    constexpr int MAX_YEAR = 3000;

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
    inline static uint32 DayOfWeek(uint32 dayOfMonth, uint32 month, uint32 year) noexcept
    {
        if ((month == 1) || (month == 2))
        {
            month += 12;
            year -= 1;
        }

        uint32 h = (dayOfMonth
            + static_cast<uint32>(std::floor((13 * (month + 1)) / 5.0))
            + year
            + static_cast<uint32>(std::floor(year / 4.0))
            - static_cast<uint32>(std::floor(year / 100.0))
            + static_cast<uint32>(std::floor(year / 400.0))) % 7;

        return static_cast<uint32>(std::floor(((h + 5) % 7) + 1));
    }

    constexpr static bool IsLeapYear(uint32 year) noexcept
    {
        if ((year % 400) == 0)
            return true;

        if ((year % 4 == 0) && ((year % 100) != 0))
            return true;

        return false;
    }

    inline static uint32 NumDaysInMonth(uint32 month, uint32 year)
    {
        if (month == 2)
            return IsLeapYear(year) ? 29 : 28;

        // Month index paired to the number of days in that month excluding February
        static const std::unordered_map<uint32, uint32> monthDayMap =
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
    inline static uint32 NumWeeksInMonth(uint32 month, uint32 year)
    {
        uint32 days = NumDaysInMonth(month, year);
        uint32 firstDay = DayOfWeek(1, month, year);

        return static_cast<uint32>(std::ceil((days + firstDay - 1) / 7.0));
    }

    // Returns a vector containing dates as they would appear on the calendar for a given week. Populates 0 if there is no day.
    inline static std::vector<uint32> CalendarWeek(uint32 week, uint32 startDay, uint32 daysInMonth)
    {
        std::vector<uint32> res(7, 0);
        int startOfWeek = 7 * (week - 1) + 2 - startDay;

        if (startOfWeek >= 1)
            res[0] = (uint32)startOfWeek;

        for (uint32 i = 1; i < 7; ++i)
        {
            int day = startOfWeek + i;
            if ((day >= 1) && (day <= (int)daysInMonth))
                res[i] = day;
        }

        return res;
    }

    inline static Date PreviousMonth(const Date& date)
    {
        uint32 month = date.GetMonth();
        uint32 year = date.GetYear();

        if (month == 1)
        {
            uint32 newDay = std::min(date.GetDayOfMonth(), NumDaysInMonth(12, --year));
            return Date::EncodeDate(newDay, 12, year);
        }

        uint32 newDay = std::min(date.GetDayOfMonth(), NumDaysInMonth(--month, year));
        return Date::EncodeDate(newDay, month, year);
    }

    inline static Date NextMonth(const Date& date)
    {
        uint32 month = date.GetMonth();
        uint32 year = date.GetYear();

        if (month == 12)
        {
            uint32 newDay = std::min(date.GetDayOfMonth(), NumDaysInMonth(1, ++year));
            return Date::EncodeDate(newDay, 1, year);
        }

        uint32 newDay = std::min(date.GetDayOfMonth(), NumDaysInMonth(++month, year));
        return Date::EncodeDate(newDay, month, year);
    }

    inline static bool IsMinDate(const Date& date)
    {
        return (date.GetMonth() == 1) && (date.GetYear() == MIN_YEAR);
    }

    inline static bool IsMaxDate(const Date& date)
    {
        return (date.GetMonth() == 12) && (date.GetYear() == MAX_YEAR);
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

    bool DatePicker(const std::string& label, Mango::Date& v, bool clampToBorder, float itemSpacing)
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

                uint32 month = monthIdx + 1;
                uint32 firstDayOfMonth = DayOfWeek(1, month, (uint32)year);
                uint32 numDaysInMonth = NumDaysInMonth(month, (uint32)year);
                uint32 numWeeksInMonth = NumWeeksInMonth(month, (uint32)year);

                for (uint32 i = 1; i <= numWeeksInMonth; ++i)
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
                                v = Date::EncodeDate(day, month, (uint32)year);
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
