#include "common.h"
#include "MinHook.h"
#include "external/imgui/imgui.h"
#include "external/imgui/imgui_impl_dx11.h"
#include "external/imgui/imgui_impl_win32.h"
#include <d3d11.h>
#include <windows.h>

uint64_t overlay_present_offset = 0x8f070; //x64 overlay

HINSTANCE m_hmod;

using Present = HRESULT(*)(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags);
Present oPresent = nullptr;


WNDPROC oWndProc;
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;
HWND window = NULL;
ID3D11Device* p_device = NULL;
ID3D11DeviceContext* p_context = NULL;
ID3D11RenderTargetView* mainRenderTargetView = NULL;


DWORD __stdcall freelib(LPVOID lpParameter) {
	Sleep(100);

	MH_RemoveHook(MH_ALL_HOOKS);
	MH_Uninitialize();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	if (mainRenderTargetView) { mainRenderTargetView->Release(); mainRenderTargetView = NULL; }
	if (p_context) { p_context->Release(); p_context = NULL; }
	if (p_device) { p_device->Release(); p_device = NULL; }
	SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)(oWndProc));

	FreeLibraryAndExitThread(m_hmod, 0);
	Sleep(100);

	return 0;
}


HRESULT hook_present(IDXGISwapChain* pThis, UINT SyncInterval, UINT Flags)
{
	if (!init)
	{
		if (SUCCEEDED(pThis->GetDevice(__uuidof(ID3D11Device), (void**)&p_device)))
		{
			p_device->GetImmediateContext(&p_context);
			DXGI_SWAP_CHAIN_DESC sd;
			pThis->GetDesc(&sd);
			window = sd.OutputWindow;
			ID3D11Texture2D* pBackBuffer;
			pThis->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
			p_device->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
			pBackBuffer->Release();
			oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
			ImGui_ImplWin32_Init(window);
			ImGui_ImplDX11_Init(p_device, p_context);
			init = true;
		}
		else
			return oPresent(pThis, SyncInterval, Flags);
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGui::ShowDemoWindow();

	ImGui::EndFrame();
	ImGui::Render();

	p_context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	return oPresent(pThis, SyncInterval, Flags);
}

int WINAPI main()
{
	DWORD64 m_GameOverlayRenderer64 = (DWORD64)LoadLibraryA("GameOverlayRenderer64.dll");

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK) {
		return 1;
	}

	if (MH_CreateHook(reinterpret_cast<void**>(m_GameOverlayRenderer64 + overlay_present_offset), &hook_present, reinterpret_cast<void**>(&oPresent)) != MH_OK) {
		return 1;
	}

	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
		return 1;
	}

	while (true) 
	{
		if (GetAsyncKeyState(VK_TAB)) {
			break;
		}
		Sleep(50);
	}
	CreateThread(NULL, 0, freelib, NULL, 0, NULL);
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hmod, DWORD dwReason, LPVOID pReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hmod);
		m_hmod = hmod;

		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)main, NULL, 0, NULL);
	}
	return TRUE;
}
