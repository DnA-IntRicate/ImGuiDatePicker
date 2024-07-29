#pragma once
#include <Mango/Core/Core.hpp>  // TODO: Remove these includes
#include <Mango/Data/Date.hpp>
#include <imgui.h>


namespace ImGui
{
    // TODO: Use tm struct
    MANGO_API bool DatePicker(const std::string& label, Mango::Date& v, bool clampToBorder = false, float itemSpacing = 130.0f);
}
