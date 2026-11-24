# Microsoft WebView2 headers and the architecture-specific static loader.
set(SILVER_WEBVIEW2_SDK_PATH "" CACHE PATH "Optional extracted Microsoft.Web.WebView2 NuGet package")
if(NOT SILVER_WEBVIEW2_SDK_PATH)
 include(FetchContent)
 if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
 endif()
 FetchContent_Declare(silver_webview2_sdk
  URL "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/1.0.3065.39/microsoft.web.webview2.1.0.3065.39.nupkg"
  URL_HASH SHA256=ede79a79e1d7e3c0d284b1cf591226ab2868743260033e0080f875e657cc8a4d
  DOWNLOAD_NAME Microsoft.Web.WebView2.zip
  SOURCE_SUBDIR silver-download-only)
 FetchContent_MakeAvailable(silver_webview2_sdk)
 set(SILVER_WEBVIEW2_SDK_PATH "${silver_webview2_sdk_SOURCE_DIR}")
endif()

set(silverWebView2Native "${SILVER_WEBVIEW2_SDK_PATH}/build/native")
if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[Aa][Rr][Mm]64$")
 set(silverWebView2Arch arm64)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
 set(silverWebView2Arch x64)
else()
 set(silverWebView2Arch x86)
endif()
set(silverWebView2Loader "${silverWebView2Native}/${silverWebView2Arch}/WebView2LoaderStatic.lib")
if(NOT EXISTS "${silverWebView2Loader}" OR NOT EXISTS "${silverWebView2Native}/include/WebView2.h")
 message(FATAL_ERROR "SILVER_WEBVIEW2_SDK_PATH must contain the extracted WebView2 SDK")
endif()
add_library(silverWebView2 STATIC IMPORTED)
set_target_properties(silverWebView2 PROPERTIES
 IMPORTED_LOCATION "${silverWebView2Loader}"
 INTERFACE_INCLUDE_DIRECTORIES "${silverWebView2Native}/include"
 INTERFACE_LINK_LIBRARIES "comctl32;ole32;version")
