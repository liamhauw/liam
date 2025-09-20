#ifndef WIN32_H
#define WIN32_H

#include "dx.h"

class DX;

class Win32 {
 public:
  static int Run(DX *dx, HINSTANCE h_instance, int cmd_show);
  static HWND h_wnd() { return h_wnd_; }

 protected:
  static LRESULT CALLBACK WndProc(HWND h_wnd, UINT msg, WPARAM w_param,
                                     LPARAM l_param);

 private:
  static HWND h_wnd_;
};

#endif  // !WIN32_H
