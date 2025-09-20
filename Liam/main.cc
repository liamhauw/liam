#include "pch.h"
//
#include <iostream>

#include "smoke.h"

_Use_decl_annotations_ int WINAPI WinMain(HINSTANCE h_instance, HINSTANCE,
                                          LPSTR, int cmd_show) {
  Smoke smoke{1280, 720, L"smoke"};
  return Win32::Run(&smoke, h_instance, cmd_show);
}