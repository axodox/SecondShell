#include "pch.h"

int WINAPI wWinMain(HINSTANCE /*instance*/, HINSTANCE /*previousInstance*/, PWSTR /*commandLine*/, int /*showCommand*/)
{
  //The driver must not touch input or the screen before the session guard accepts it (stage 5), so for now it does nothing.
  return 0;
}