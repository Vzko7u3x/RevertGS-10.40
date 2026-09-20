#pragma once

#if defined(REVERT_LATEGAME)
inline constexpr bool kStormRush = true;
inline constexpr bool kPlotMode = false;
inline constexpr const char* kDeskTitle = "revert GS  |  LateGame  |  10.40";
inline constexpr const char* kFlavorName = "LateGame";
#elif defined(REVERT_CREATIVE)
inline constexpr bool kStormRush = false;
inline constexpr bool kPlotMode = true;
inline constexpr const char* kDeskTitle = "revert GS  |  Creative  |  10.40";
inline constexpr const char* kFlavorName = "Creative";
#else
inline constexpr bool kStormRush = false;
inline constexpr bool kPlotMode = false;
inline constexpr const char* kDeskTitle = "revert GS  |  Solo  |  10.40";
inline constexpr const char* kFlavorName = "Solo";
#endif

inline constexpr bool kSolo = !kStormRush && !kPlotMode;
