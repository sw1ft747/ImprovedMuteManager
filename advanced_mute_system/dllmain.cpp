// C++
// Sven Co-op - Advanced Mute System

#include <Windows.h>
#include <stdio.h>

#include "sdk.h"
#include "patterns.h"

#include "modules/engine.h"
#include "modules/client.h"

#include "utils/signature_scanner.h"
#include "libdasm/libdasm.h"

#define MOD_VERSION "1.0.2"

// Show console for debugging or something
#define SHOW_CONSOLE 0

// "Interfaces"
cl_enginefunc_t *g_pEngineFuncs = NULL;
cl_clientfunc_t *g_pClientFuncs = NULL;
engine_studio_api_t *g_pEngineStudio = NULL;
r_studio_interface_t *g_pStudioAPI = NULL;
CStudioModelRenderer *g_pStudioRenderer = NULL;

bool InitInterfaces()
{
	INSTRUCTION instruction;

	void *pEngineFuncs = FIND_PATTERN(L"hw.dll", Patterns::Interfaces::EngineFuncs);

	if (!pEngineFuncs)
	{
		printf("'cl_enginefunc_t' failed initialization\n");
		return false;
	}

	void *pClientFuncs = FIND_PATTERN(L"hw.dll", Patterns::Interfaces::ClientFuncs);

	if (!pClientFuncs)
	{
		printf("'cl_clientfunc_t' failed initialization\n");
		return false;
	}

	void *pEngineStudio = FIND_PATTERN(L"hw.dll", Patterns::Interfaces::EngineStudio);

	if (!pEngineStudio)
	{
		printf("'engine_studio_api_t' failed initialization\n");
		return false;
	}

	void *pStudioAPI = pEngineStudio;

	// g_pEngineFuncs
	get_instruction(&instruction, (BYTE *)pEngineFuncs, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_PUSH && instruction.op1.type == OPERAND_TYPE_IMMEDIATE)
	{
		g_pEngineFuncs = reinterpret_cast<cl_enginefunc_t *>(instruction.op1.immediate);
	}
	else
	{
		printf("'cl_enginefunc_t' failed initialization #2\n");
		return false;
	}

	// g_pClientFuncs
	get_instruction(&instruction, (BYTE *)pClientFuncs + 0x15, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_MEMORY && instruction.op2.type == OPERAND_TYPE_REGISTER)
	{
		g_pClientFuncs = reinterpret_cast<cl_clientfunc_t *>(instruction.op1.displacement);
	}
	else
	{
		printf("'cl_clientfunc_t' failed initialization #2\n");
		return false;
	}

	// g_pEngineStudio
	get_instruction(&instruction, (BYTE *)pEngineStudio, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_PUSH && instruction.op1.type == OPERAND_TYPE_IMMEDIATE)
	{
		g_pEngineStudio = reinterpret_cast<engine_studio_api_t *>(instruction.op1.immediate);
	}
	else
	{
		printf("'engine_studio_api_t' failed initialization #2\n");
		return false;
	}

	// g_pStudioAPI
	get_instruction(&instruction, (BYTE *)pStudioAPI + 0x5, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_PUSH && instruction.op1.type == OPERAND_TYPE_IMMEDIATE)
	{
		g_pStudioAPI = reinterpret_cast<r_studio_interface_t *>(instruction.op1.immediate);
	}
	else
	{
		printf("'r_studio_interface_t' failed initialization\n");
		return false;
	}

	// g_pStudioRenderer
	get_instruction(&instruction, (BYTE *)g_pClientFuncs->HUD_GetStudioModelInterface + 0x26, MODE_32);

	if (instruction.type == INSTRUCTION_TYPE_MOV && instruction.op1.type == OPERAND_TYPE_REGISTER && instruction.op2.type == OPERAND_TYPE_IMMEDIATE)
	{
		g_pStudioRenderer = reinterpret_cast<CStudioModelRenderer *>(instruction.op2.immediate);
	}
	else
	{
		printf("'CStudioModelRenderer' failed initialization\n");
		return false;
	}

	return true;
}

DWORD WINAPI MainThread(HMODULE hModule)
{
#if SHOW_CONSOLE
	FILE *file;
	AllocConsole();
	freopen_s(&file, "CONOUT$", "w", stdout);
#endif

	DWORD dwHWDLL = (DWORD)GetModuleHandle(L"hw.dll");

	if (dwHWDLL)
	{
		if (!InitInterfaces())
			goto FAILURE_EXIT;

		if (!InitClientModule())
			goto FAILURE_EXIT;

		InitEngineModule();

		g_pEngineFuncs->Con_Printf("<< Advanced Mute System >>\n");
		g_pEngineFuncs->Con_Printf("Author: Sw1ft\n");
		g_pEngineFuncs->Con_Printf("Version: " MOD_VERSION "\n");
		g_pEngineFuncs->Con_Printf("<< Advanced Mute System >>\n");

		while (true) { }
	}
	else
	{
		printf("It is not a Half-Life game..\n");

	FAILURE_EXIT:
		printf("Exiting...\n");
		Sleep(3500);
	}

#if SHOW_CONSOLE
	if (file) fclose(file);
	FreeConsole();
#endif

	FreeLibraryAndExitThread(hModule, 0);

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
	{
		HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
		if (hThread) CloseHandle(hThread);
		break;
	}

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}