#pragma once

#include "Pch.hpp"
#include <fstream>
#include "Arena.hpp"
#include "Router.hpp"
#include "Flavor.hpp"

namespace Revert::Desk
{
	inline void Help()
	{
		Note("commands: start | reload (next round) | dump | log | list | help");
	}

	inline void DumpObjects()
	{
		std::ofstream out("revert-objects.txt");
		if (!out)
		{
			Note("cannot write revert-objects.txt");
			return;
		}
		for (int i = 0; i < UObject::GObjects->Num(); ++i)
		{
			if (auto* object = UObject::GObjects->GetObjectById(i))
				out << object->InternalIndex << "  " << object->GetFullName() << '\n';
		}
		Note("wrote revert-objects.txt");
	}

	inline void ListClients()
	{
		auto* world = World();
		if (!world || !world->NetDriver)
		{
			Note("no net driver yet");
			return;
		}
		for (int i = 0; i < world->NetDriver->ClientConnections.Num(); ++i)
		{
			auto* conn = world->NetDriver->ClientConnections[i];
			auto* pc = conn ? As<APlayerController>(conn->PlayerController) : nullptr;
			auto* ps = pc ? pc->PlayerState : nullptr;
			Note("slot {}  {}", i, ps ? ps->GetPlayerName().ToString() : "?");
		}
	}

	inline DWORD WINAPI Loop(LPVOID)
	{
		Help();
		char line[256]{};
		while (std::cin.getline(line, sizeof(line)))
		{
			std::string cmd(line);
			if (cmd.empty())
				continue;
			if (cmd == "start")
			{
				if (kPlotMode)
					Arena::HoldCreative();
				else
					Arena::LaunchBus();
			}
			else if (cmd == "reload")
				Arena::QueueReload(true);
			else if (cmd == "dump")
				DumpObjects();
			else if (cmd == "log")
			{
				Router::LogCalls = !Router::LogCalls;
				Note("process log {}", Router::LogCalls ? "on" : "off");
			}
			else if (cmd == "list")
				ListClients();
			else if (cmd == "help")
				Help();
			else
				Note("unknown command");
		}
		return 0;
	}

	inline void Open()
	{
		AllocConsole();
		FILE* stream = nullptr;
		freopen_s(&stream, "CONOUT$", "w", stdout);
		freopen_s(&stream, "CONIN$", "r", stdin);
		SetConsoleTitleA(kDeskTitle);
		std::ios::sync_with_stdio(false);
	}
}
