#pragma once
#include <winsock2.h>
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>
#include <wtsapi32.h>
#include <wincred.h>
#include <dpapi.h>
#define SECURITY_WIN32
#include <security.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlhost.h>

#include <deque>
#include <fstream>
#include <future>
#include <unordered_set>

//Undefine GetCurrentTime macro to prevent conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <wil/cppwinrt_helpers.h>

#include "Include/Axodox.Infrastructure.h"
#include "Include/Axodox.Json.h"
#include "Include/Axodox.Networking.h"
#include "Include/Axodox.Threading.h"