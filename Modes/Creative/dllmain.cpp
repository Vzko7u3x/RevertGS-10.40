#include <Windows.h>
#include "Host.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(module);
		CreateThread(nullptr, 0, Revert::Boot, nullptr, 0, nullptr);
	}
	return TRUE;
}
