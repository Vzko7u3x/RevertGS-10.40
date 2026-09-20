#include "Pch.hpp"
#include <ctime>
#include "Desk.hpp"
#include "Router.hpp"
#include "Arena.hpp"

namespace Revert
{
	DWORD WINAPI Boot(LPVOID)
	{
		Desk::Open();
		Note("boot {}  image 0x{:x}", kFlavorName, Rva::Base());
		std::srand(static_cast<unsigned>(time(nullptr)));

		while (!Engine() || !Engine()->GameViewport)
			Sleep(80);

		UObject::GObjects = Rva::Rel<FChunkedFixedUObjectArray*>(Rva::Objects);
		MarkDedicated();

		if (auto* lp = Engine()->GameViewport->World ? Engine()->GameViewport->World->OwningGameInstance : nullptr)
		{
			if (lp->LocalPlayers.Num())
			{
				auto* pc = lp->LocalPlayers[0]->PlayerController;
				UKismetSystemLibrary::ExecuteConsoleCommand(World(), L"log LogFort VeryVerbose", pc);
			}
		}

		Router::BindAll();

		if (auto* world = World(); world && world->OwningGameInstance && world->OwningGameInstance->LocalPlayers.Num())
		{
			if (auto* pc = world->OwningGameInstance->LocalPlayers[0]->PlayerController)
				pc->SwitchLevel(L"Athena_Terrain");
			world->OwningGameInstance->LocalPlayers.Remove(0);
		}

		CreateThread(nullptr, 0, Desk::Loop, nullptr, 0, nullptr);
		Note("host ready — type help");
		return 0;
	}
}
