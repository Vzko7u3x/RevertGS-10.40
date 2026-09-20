#pragma once

// Backend = dsq  (C:\Users\3\Downloads\dsq)
// Edit this file, then rebuild the DLL.

namespace Revert::Cfg
{
	// --- MCP (dsq) ---
	inline constexpr bool Mcp = true;
	inline constexpr const char* McpHost = "147.189.168.159";
	inline constexpr int McpPort = 3551;
	inline constexpr bool McpHttps = false;
	inline constexpr const char* McpApiKey = "9c8b840ff889206a41ed895e9a1c35e067146a990974ff493cdc11399524c9b9";
	inline constexpr int McpDedicated = 3;
	inline constexpr const char* McpQueryPath =
		"/fortnite/api/game/v2/profile/{}/dedicated_server/QueryProfile?profileId=athena&rvn=-1";

	inline constexpr bool Arena = true;
	inline constexpr const char* ArenaPath = "/Revert/gameserver/arena/{}/{}/{}/{}/{}";
	inline constexpr const char* XpPath = "/Revert/gameserver/xp/{}/{}/{}";
	inline constexpr const char* VbucksPath = "/Revert/gameserver/add/{}/{}/{}/{}";

	// --- Listen ---
	inline constexpr int ListenPort = 7777;
	inline constexpr int MaxPlayers = 100;
	inline constexpr int MaxTickRate = 30;
	inline constexpr float WarmupSeconds = 10.f;
	inline constexpr float CloseGsDelay = 4.f; // wait after match before next round (no crash)
	inline constexpr int WarmupRequired = 1;

	// --- LateGame zone ---
	inline constexpr int LateZone = 3;
	inline constexpr float ZoneHold = 20.f;
	inline constexpr float ZoneShrink = 30.f;
	inline constexpr float ZoneSizes[5] = { 20000.f, 9500.f, 4000.f, 1000.f, 0.f };

	// --- Bus ---
	inline constexpr float BusHeight = 20000.f;
	inline constexpr float BusLife = 5.f;
	inline constexpr float DropLock = 0.f;

	// --- Arena hype ---
	inline constexpr int KillHype = 25;
	inline constexpr int WinHype = 120;
	inline constexpr int Place2Hype = 60;
	inline constexpr int Place5Hype = 40;
	inline constexpr int Place10Hype = 25;
	inline constexpr int Place25Hype = 15;
	inline constexpr int Place50Hype = 10;
	inline constexpr int PlaceRestHype = 5;

	inline constexpr int KillXp = 0; // XP only on match complete (place/win) — dsq increments matches per XP grant
	inline constexpr int WinXp = 800;
	inline constexpr int Place2Xp = 400;
	inline constexpr int Place5Xp = 250;
	inline constexpr int Place10Xp = 150;
	inline constexpr int Place25Xp = 80;
	inline constexpr int Place50Xp = 50;
	inline constexpr int PlaceRestXp = 25;
	inline constexpr int XpLevel = 1;

	inline constexpr int KillVbucks = 50;
	inline constexpr int WinVbucks = 150;

	// --- Combat ---
	inline constexpr float SiphonHealth = 100.f;
	inline constexpr float SiphonShield = 100.f;

	// --- Solo bots ---
	inline constexpr int BotSpawnPerTick = 4;
}
