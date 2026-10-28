#pragma once
#include "jucePluginLib/patchdb/patchdbtypes.h"
namespace silverK {
bool parseVStationBank(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&);
bool parseVStationProgram(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&);
bool parseVStationSysex(pluginLib::patchDB::DataList&,const pluginLib::patchDB::Data&);
bool unpackVStationPatch(pluginLib::patchDB::Data&,std::string& name);
}
