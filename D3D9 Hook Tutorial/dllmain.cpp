#pragma comment(lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")
#pragma comment(lib, "detours.lib")

#include <Windows.h>
#include <iostream>
#include <string>
#include <Detours.h>
#include <imgui.h>
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <d3dx9.h>

#define LPDIRECT3DDEVICE g_pD3D

typedef HRESULT(__stdcall* EndScene)(LPDIRECT3DDEVICE9 pDevice);
HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice);
EndScene oEndScene;

LPDIRECT3DDEVICE9	g_pd3dDevice = NULL;
HWND				window = NULL;

LRESULT WINAPI		WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
WNDPROC				wndproc_orig = NULL;

// Win32 message handler
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
	{
		//Send input to our menu
		return true;
	}

	//Send input to game
	return CallWindowProc(wndproc_orig, hWnd, msg, wParam, lParam);
}

BOOL CALLBACK EnumWindowsCallback(HWND handle, LPARAM lParam)
{
	DWORD wndProcId;
	GetWindowThreadProcessId(handle, &wndProcId);

	if (GetCurrentProcessId() != wndProcId)
		return TRUE; // skip to next window

	window = handle;
	return FALSE; // window found abort search
}

HWND GetProcessWindow()
{
	window = NULL;
	EnumWindows(EnumWindowsCallback, NULL);
	return window;
}

bool GetD3D9Device(void ** pTable, size_t Size)
{
	if (!pTable)
		return false;

	IDirect3D9 * g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	if (!g_pD3D)
		return false;

	// options to create dummy device
	D3DPRESENT_PARAMETERS d3dpp = {};
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = GetProcessWindow();
	d3dpp.Windowed = false;

	g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice);

	if (!g_pd3dDevice)
	{
		// may fail in windowed fullscreen mode, trying again with windowed mode
		d3dpp.Windowed = !d3dpp.Windowed;

		g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &g_pd3dDevice);

		if (!g_pd3dDevice)
		{
			g_pD3D->Release();
			return false;
		}
	}

	memcpy(pTable, *reinterpret_cast<void***>(g_pd3dDevice), Size);

	g_pd3dDevice->Release();
	g_pD3D->Release();
	return true;
}

void CleanupDeviceD3D()
{
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
	if (g_pD3D) { g_pD3D->Release(); g_pD3D = NULL; }
}

DWORD WINAPI mainThread(PVOID base)
{
	void * d3d9Device[119];

	if (GetD3D9Device(d3d9Device, sizeof(d3d9Device)))
	{
		oEndScene = (EndScene)d3d9Device[42];

		oEndScene = (EndScene)Detours::X86::DetourFunction((uintptr_t)d3d9Device[42], (uintptr_t)hkEndScene);

		while (true)
		{
			if (GetAsyncKeyState(VK_F10))
			{
				ImGui_ImplDX9_Shutdown();
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext();

				CleanupDeviceD3D();

				FreeLibraryAndExitThread(static_cast<HMODULE>(base), 1);
			}
		}

		//main();

	}
	FreeLibraryAndExitThread(static_cast<HMODULE>(base), 1);
}

HRESULT __stdcall hkEndScene(LPDIRECT3DDEVICE9 pDevice)
{
	static bool init = false;

	if (!init)
	{
		MessageBoxA(0, "Hello from endscene!", "Hello from endscene!", 0);
		init = true;
	}

	return oEndScene(pDevice);
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		CreateThread(nullptr, NULL, mainThread, hModule, NULL, nullptr);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

