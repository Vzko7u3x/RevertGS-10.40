#pragma once

#include "Pch.hpp"

#include <atomic>
#include <cctype>
#include <string>
#include <string_view>
#include <thread>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace Revert::Wire
{
	inline std::atomic<int> Inflight{ 0 };

	inline std::wstring Widen(const std::string& text)
	{
		if (text.empty())
			return {};
		std::wstring out(text.size(), L'\0');
		for (size_t i = 0; i < text.size(); ++i)
			out[i] = static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
		return out;
	}

	inline std::string CleanAccount(std::string id)
	{
		auto cut = id.find_last_of(" \t");
		if (cut != std::string::npos)
			id = id.substr(cut + 1);
		if (id.size() > 4 && (id.compare(0, 4, "MCP:") == 0 || id.compare(0, 4, "mcp:") == 0))
			id = id.substr(4);
		std::string out;
		out.reserve(id.size());
		for (unsigned char c : id)
		{
			if (std::isalnum(c) || c == '-')
				out.push_back(static_cast<char>(c));
		}
		return out;
	}

	inline std::string AccountId(AFortPlayerState* ps)
	{
		if (!ps)
			return {};

		auto fromDebug = CleanAccount(UFortKismetLibrary::GetDebugStringForUniqueId(ps->UniqueId).ToString());
		if (fromDebug.size() >= 8)
			return fromDebug;

		auto fromGetter = CleanAccount(UFortKismetLibrary::GetDebugStringForUniqueId(ps->GetUniqueID()).ToString());
		if (fromGetter.size() >= 8)
			return fromGetter;

		auto& bytes = ps->UniqueId.ReplicationBytes;
		if (bytes.Num() >= 8)
		{
			std::string raw;
			raw.reserve(static_cast<size_t>(bytes.Num()));
			for (int i = 0; i < bytes.Num(); ++i)
			{
				const auto c = bytes[i];
				if (c)
					raw.push_back(static_cast<char>(c));
			}
			auto cleaned = CleanAccount(raw);
			if (cleaned.size() >= 8)
				return cleaned;
		}
		return {};
	}

	inline std::string HostUtf8()
	{
		return Cfg::McpHost;
	}

	inline INTERNET_PORT BackendPort()
	{
		return static_cast<INTERNET_PORT>(Cfg::McpPort);
	}

	inline const char* PlaylistKey()
	{
		if (kStormRush)
			return "playlist_showdownalt_solo";
		return "playlist_defaultsolo";
	}

	inline void GetAsync(std::wstring path, const char* tag)
	{
		if (path.empty() || !Cfg::Mcp)
			return;
		Inflight.fetch_add(1, std::memory_order_relaxed);
		std::thread([path = std::move(path), tag]()
		{
			struct Guard
			{
				~Guard() { Inflight.fetch_sub(1, std::memory_order_relaxed); }
			} guard;

			const auto host = Widen(Cfg::McpHost);
			const auto key = Widen(Cfg::McpApiKey);
			auto* session = WinHttpOpen(L"RevertGS", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
			if (!session)
			{
				Note("{} open failed {}", tag, GetLastError());
				return;
			}
			WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
			auto* connect = WinHttpConnect(session, host.c_str(), BackendPort(), 0);
			if (!connect)
			{
				Note("{} connect failed {}", tag, GetLastError());
				WinHttpCloseHandle(session);
				return;
			}
			const DWORD flags = Cfg::McpHttps ? WINHTTP_FLAG_SECURE : 0;
			auto* request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
			if (!request)
			{
				Note("{} request failed {}", tag, GetLastError());
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return;
			}
			if (!key.empty())
			{
				const auto header = L"x-api-key: " + key;
				WinHttpAddRequestHeaders(request, header.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD);
			}
			if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
				!WinHttpReceiveResponse(request, nullptr))
			{
				Note("{} send failed {}", tag, GetLastError());
				WinHttpCloseHandle(request);
				WinHttpCloseHandle(connect);
				WinHttpCloseHandle(session);
				return;
			}
			DWORD status = 0;
			DWORD size = sizeof(status);
			WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
			if (status != 200)
				Note("{} http {}", tag, status);
			WinHttpCloseHandle(request);
			WinHttpCloseHandle(connect);
			WinHttpCloseHandle(session);
		}).detach();
	}

	inline void Drain(DWORD waitMs = 8000)
	{
		const int pending = Inflight.load(std::memory_order_relaxed);
		if (pending <= 0)
			return;
		Note("waiting for {} reward request(s)", pending);
		const auto start = GetTickCount64();
		while (Inflight.load(std::memory_order_relaxed) > 0 && GetTickCount64() - start < waitMs)
			Sleep(25);
		const int left = Inflight.load(std::memory_order_relaxed);
		if (left > 0)
			Note("reward drain timeout — {} still in flight", left);
	}

	inline int PlaceHype(int place)
	{
		if (place <= 0)
			return 0;
		if (place == 1)
			return Cfg::WinHype;
		if (place == 2)
			return Cfg::Place2Hype;
		if (place <= 5)
			return Cfg::Place5Hype;
		if (place <= 10)
			return Cfg::Place10Hype;
		if (place <= 25)
			return Cfg::Place25Hype;
		if (place <= 50)
			return Cfg::Place50Hype;
		return Cfg::PlaceRestHype;
	}

	inline int PlaceXp(int place)
	{
		if (place <= 0)
			return 0;
		if (place == 1)
			return Cfg::WinXp;
		if (place == 2)
			return Cfg::Place2Xp;
		if (place <= 5)
			return Cfg::Place5Xp;
		if (place <= 10)
			return Cfg::Place10Xp;
		if (place <= 25)
			return Cfg::Place25Xp;
		if (place <= 50)
			return Cfg::Place50Xp;
		return Cfg::PlaceRestXp;
	}

	inline void ArenaGrant(const std::string& account, int hype, int place, int kills, bool won)
	{
		if (!Cfg::Mcp || !Cfg::Arena || !Cfg::ArenaPath || !*Cfg::ArenaPath || account.empty())
			return;
		if (hype <= 0 && place <= 0 && kills <= 0 && !won)
			return;
		const std::string wonFlag = won ? "win" : "0";
		GetAsync(Widen(std::vformat(
			std::string_view(Cfg::ArenaPath),
			std::make_format_args(hype, place, kills, wonFlag, account))), "arena");
		Note("arena grant  {}  +{} hype  place {}  kills {}  {}", account, hype, place, kills, won ? "win" : "out");
	}

	inline void XpGrant(const std::string& account, int amount)
	{
		if (!Cfg::Mcp || !Cfg::XpPath || !*Cfg::XpPath || account.empty() || amount <= 0)
			return;
		const int level = Cfg::XpLevel > 0 ? Cfg::XpLevel : 1;
		GetAsync(Widen(std::vformat(
			std::string_view(Cfg::XpPath),
			std::make_format_args(amount, level, account))), "xp");
		Note("xp grant  {}  +{}", account, amount);
	}

	inline void VbucksGrant(const std::string& account, int amount, const char* reason)
	{
		if (!Cfg::Mcp || !Cfg::VbucksPath || !*Cfg::VbucksPath || account.empty() || amount <= 0 || !reason || !*reason)
			return;
		const auto playlist = PlaylistKey();
		const std::string flag = reason;
		GetAsync(Widen(std::vformat(
			std::string_view(Cfg::VbucksPath),
			std::make_format_args(amount, playlist, flag, account))), "vbucks");
		Note("vbucks grant  {}  +{}  {}", account, amount, reason);
	}

	inline void QueryProfile(const std::string& account)
	{
		if (!Cfg::Mcp || !Cfg::McpQueryPath || !*Cfg::McpQueryPath || account.empty())
			return;
		GetAsync(Widen(std::vformat(std::string_view(Cfg::McpQueryPath), std::make_format_args(account))), "profile");
	}

	inline void SaveKill(const std::string& account)
	{
		if (account.empty())
			return;
		ArenaGrant(account, Cfg::KillHype, 0, 1, false);
		if (Cfg::KillXp > 0)
			XpGrant(account, Cfg::KillXp);
		VbucksGrant(account, Cfg::KillVbucks, "kill");
	}

	inline void SavePlace(const std::string& account, int place, int kills, bool won)
	{
		if (account.empty())
			return;
		const int hype = won ? Cfg::WinHype : PlaceHype(place);
		const int xp = won ? Cfg::WinXp : PlaceXp(place);
		ArenaGrant(account, hype, place, kills, won);
		XpGrant(account, xp);
		if (won)
			VbucksGrant(account, Cfg::WinVbucks, "win");
		QueryProfile(account);
	}
}
