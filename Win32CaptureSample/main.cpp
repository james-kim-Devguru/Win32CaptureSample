#include "pch.h"
#include "App.h"
#include "SimpleCapture.h"
#include <ShObjIdl.h>
#include "Win32WindowEnumeration.h"
#include <Windowsx.h>

using namespace winrt;
using namespace Windows::UI;
using namespace Windows::UI::Composition;
using namespace Windows::UI::Composition::Desktop;

auto CreateDispatcherQueueController()
{
    namespace abi = ABI::Windows::System;

    DispatcherQueueOptions options
    {
        sizeof(DispatcherQueueOptions),
        DQTYPE_THREAD_CURRENT,
        DQTAT_COM_STA
    };

    Windows::System::DispatcherQueueController controller{ nullptr };
    check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
    return controller;
}

DesktopWindowTarget CreateDesktopWindowTarget(Compositor const& compositor, HWND window)
{
    namespace abi = ABI::Windows::UI::Composition::Desktop;

    auto interop = compositor.as<abi::ICompositorDesktopInterop>();
    DesktopWindowTarget target{ nullptr };
    check_hresult(interop->CreateDesktopWindowTarget(window, true, reinterpret_cast<abi::IDesktopWindowTarget**>(put_abi(target))));
    return target;
}

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR     cmdLine,
    int       cmdShow);

LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam);

auto g_app = std::make_shared<App>();

int CALLBACK WinMain(
    HINSTANCE instance,
    HINSTANCE previousInstance,
    LPSTR     cmdLine,
    int       cmdShow)
{
	init_apartment(apartment_type::single_threaded);

    // Create the window
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = instance;
    wcex.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"Win32CaptureSample";
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    WINRT_VERIFY(RegisterClassEx(&wcex));

    HWND hwnd = CreateWindow(
        L"Win32CaptureSample",
        L"Win32CaptureSample",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        600,
        NULL,
        NULL,
        instance,
        NULL);
    WINRT_VERIFY(hwnd);

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    // Get all the windows
	auto windows = EnumerateWindows();

    // Create a DispatcherQueue for our thread
    auto controller = CreateDispatcherQueueController();

    // Initialize Composition
    auto compositor = Compositor();
    auto target = CreateDesktopWindowTarget(compositor, hwnd);
    auto root = compositor.CreateContainerVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    target.Root(root);

    // Enqueue our capture work on the dispatcher
    auto queue = controller.DispatcherQueue();
    auto success = queue.TryEnqueue([=]() -> void
    {
		g_app->Initialize(windows, root);
    });
    WINRT_VERIFY(success);

    // Message pump
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return msg.wParam;
}

LRESULT CALLBACK WndProc(
    HWND   hwnd,
    UINT   msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
	case WM_LBUTTONUP:
		{
			auto rawX = GET_X_LPARAM(lParam);
			auto rawY = GET_Y_LPARAM(lParam);

			RECT rect = {};
			WINRT_VERIFY(GetClientRect(hwnd, &rect));
			auto windowWidth = (float)(rect.right - rect.left);
			auto windowHeight = (float)(rect.bottom - rect.top);

			g_app->OnClick({ rawX / windowWidth, rawY / windowHeight });
		}
		break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
        break;
    }

    return 0;
}