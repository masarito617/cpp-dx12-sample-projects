#include "Win32Application.h"
#include "DXApplication.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	// WIC�t�@�C���Ǎ��̂��߁ACOM�����������Ă���
	auto result = CoInitializeEx(NULL, COINITBASE_MULTITHREADED);
	if (FAILED(result)) return 1;

	DXApplication dxApp(1280, 720, L"DX Sample");
	Win32Application::Run(&dxApp, hInstance);
	return 0;
}
