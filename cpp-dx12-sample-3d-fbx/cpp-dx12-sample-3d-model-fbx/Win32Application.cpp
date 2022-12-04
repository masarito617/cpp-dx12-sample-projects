#include "Win32Application.h"

void Win32Application::Run(DXApplication* dxApp, HINSTANCE hInstance)
{
	// �E�B���h�E�N���X����
	WNDCLASSEX windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = _T("DXSampleClass");
	RegisterClassEx(&windowClass);

	// �E�B���h�E�T�C�Y�̒���
	RECT windowRect = { 0, 0, dxApp->GetWindowWidth(), dxApp->GetWindowHeight() };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, false);

	// �E�B���h�E�I�u�W�F�N�g�̐���
	HWND hwnd = CreateWindow(
		windowClass.lpszClassName,
		dxApp->GetTitle(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowRect.right - windowRect.left,
		windowRect.bottom - windowRect.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	// �A�v���P�[�V����������
	dxApp->OnInit(hwnd);

	// �E�B���h�E�\��
	ShowWindow(hwnd, SW_SHOW);

	// ���C�����[�v
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// �A�v���P�[�V�����X�V
		dxApp->OnUpdate();
		dxApp->OnRender();
	}

	// �A�v���P�[�V�����I��
	dxApp->OnDestroy();

	// �N���X��o�^��������
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

LRESULT CALLBACK Win32Application::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, message, wparam, lparam);
	}
	return 0;
}
