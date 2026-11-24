#pragma once

#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#else
 #include <dlfcn.h>
#endif

namespace silverK {

inline juce::File soulTrainResources(const void* moduleAddress,const juce::String& name)
{
#if JUCE_WINDOWS
    HMODULE module=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(moduleAddress),&module))
        return {};

    std::wstring path(32768,L'\0');
    const auto length=GetModuleFileNameW(module,path.data(),DWORD(path.size()));
    if(length==0 || length>=path.size()) return {};
    path.resize(length);
    const auto binaryDirectory=juce::File(juce::String(path.c_str())).getParentDirectory();
    const auto parent=binaryDirectory.getParentDirectory();
    if(parent.getFileName().equalsIgnoreCase("Contents") && binaryDirectory.getFileName().endsWithIgnoreCase("-win"))
        return parent.getChildFile("Resources").getChildFile(name);
    return binaryDirectory.getChildFile(name);
#else
    Dl_info module{};
    if(!dladdr(moduleAddress,&module) || !module.dli_fname) return {};
    return juce::File(juce::String::fromUTF8(module.dli_fname)).getParentDirectory().getParentDirectory()
        .getChildFile("Resources").getChildFile(name);
#endif
}
}
