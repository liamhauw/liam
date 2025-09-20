#include "pch.h"
//
#include "win32.h"

HWND Win32::h_wnd_ = nullptr;

int Win32::Run(DX *dx, HINSTANCE h_instance, int cmd_show) {
  // Parse the command line parameters.
  int argc;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  dx->ParseCommandLineArgs(argv, argc);
  LocalFree(argv);

  // Initialize the window class.
  WNDCLASSEX wnd_class = {0};
  wnd_class.cbSize = (sizeof(WNDCLASSEX));
  wnd_class.style = CS_HREDRAW | CS_VREDRAW;
  wnd_class.lpfnWndProc = WndProc;
  wnd_class.hInstance = h_instance;
  wnd_class.hCursor = LoadCursor(NULL, IDC_ARROW);
  wnd_class.lpszClassName = L"DX";
  RegisterClassEx(&wnd_class);

  RECT rect = {0, 0, static_cast<LONG>(dx->width()),
               static_cast<LONG>(dx->height())};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  // Create the window and store a handle to it.
  h_wnd_ =
      CreateWindow(wnd_class.lpszClassName, dx->title(), WS_OVERLAPPEDWINDOW,
                   CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
                   rect.bottom - rect.top, nullptr, nullptr, h_instance, dx);

  // Initialize the dx. OnInit is defined in each child-implementation of dx.
  dx->OnInit();

  ShowWindow(h_wnd_, cmd_show);

  // Main dx loop.
  MSG msg = {};
  while (msg.message != WM_QUIT) {
    // Process any message in the queue.
    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    } else {
      dx->OnUpdate();
      dx->OnRender();
    }
  }

  dx->OnDestroy();

  return static_cast<int>(msg.wParam);
}

// Main message handler for the dx.
LRESULT CALLBACK Win32::WndProc(HWND h_wnd, UINT msg, WPARAM w_param,
                                LPARAM l_param) {
  DX *dx = reinterpret_cast<DX *>(GetWindowLongPtr(h_wnd, GWLP_USERDATA));

  switch (msg) {
    case WM_CREATE: {
      // Save the dx passed in to CreateWindow.
      LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(l_param);
      SetWindowLongPtr(
          h_wnd, GWLP_USERDATA,
          reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
    }
      return 0;

    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
      dx->OnMouseDown(w_param, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
      return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
      dx->OnMouseUp(w_param, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
      return 0;

    case WM_MOUSEMOVE:
      dx->OnMouseMove(w_param, GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param));
      return 0;

    case WM_KEYDOWN:
      if (dx) {
        dx->OnKeyDown(static_cast<UINT8>(w_param));
      }
      return 0;

    case WM_KEYUP:
      if (dx) {
        dx->OnKeyUp(static_cast<UINT8>(w_param));
      }
      return 0;

    case WM_MOUSEWHEEL:
      dx->OnMouseWheel((float)GET_WHEEL_DELTA_WPARAM(w_param) / WHEEL_DELTA);
      return 0;

    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }

  // Handle any messages the switch statement didn't.
  return DefWindowProc(h_wnd, msg, w_param, l_param);
}