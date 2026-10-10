#pragma once

#ifdef SECONDSHELL_COMMON_EXPORTS
#define SECONDSHELL_COMMON_API __declspec(dllexport)
#else
#define SECONDSHELL_COMMON_API __declspec(dllimport)
#endif