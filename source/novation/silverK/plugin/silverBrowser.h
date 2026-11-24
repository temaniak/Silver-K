#pragma once
#include "silverProcessor.h"
#include "jucePluginLib/patchdb/presetSelection.h"
#include "jucePluginLib/patchdb/search.h"
#include <unordered_map>
namespace silverK {
class Browser final:public juce::Component,private juce::TableListBoxModel,private juce::Timer {
public:
 explicit Browser(Processor&);~Browser()override;
 void resized()override;
 void pollLibrary(){timerCallback();}
 void saveViewState(){rememberView();}
 bool stepPatch(int direction,juce::String* error=nullptr);
 juce::var webSnapshot() const;
 void webAction(const juce::var&);
 std::vector<pluginLib::patchDB::PatchPtr> filteredPrograms()const;
 bool activateProgram(unsigned,const pluginLib::patchDB::PatchPtr&,juce::String&);
 bool stepProgram(unsigned,int,juce::String&);
private:
 Processor& p;
 juce::TableListBox table;
 juce::TextEditor search,tags,tagFilter;
 juce::ComboBox model,source,ksBankSelect;
 juce::ToggleButton favourite{"Favourite"},onlyFavourites{"Favourites only"},showAllSlots{"Show all bank slots"},showInit{"Show init"};
 juce::TextButton addFolder{"Add folder"},addFile{"Add file"},refresh{"Rescan"},remove{"Remove source"},saveTags{"Save tags"},exportPatch{"Export .syx"},saveCurrent{"Save current"};
 juce::Label info,bankHint;
 std::unique_ptr<juce::FileChooser> chooser;
 std::vector<pluginLib::patchDB::PatchPtr> rows;
 std::vector<pluginLib::patchDB::DataSourceNodePtr> sources;
 std::vector<Library::PresetPath> presetPaths;
 juce::StringArray facets,selectedFacets;
 juce::var nativeColumnLayout;
 pluginLib::patchDB::DataSourceNodePtr preferredSource;
 juce::String preferredSourceName,lastLibraryError;
 int preferredSourceType=0;
 uint64_t bankRequest=0;
 uint64_t firmwareRevision=~uint64_t{0};
 unsigned revision=~0u;
 pluginLib::patchDB::SearchHandle handle=pluginLib::patchDB::g_invalidSearchHandle;
 std::shared_ptr<std::atomic<bool>> searchCompleted;
 bool dirty=true,lastK=false,updatingRows=false,resultsDirty=true;
 int sortColumn=7;bool sortAscending=true;
 void updateModel();
 void rememberView();
 pluginLib::patchDB::PatchPtr lastActivatedPatch;
 std::array<pluginLib::patchDB::PatchPtr,2> lastPatchForModel{};
 juce::String pendingFirmwareSelection;
 juce::String pendingSound,pendingConversion;
 bool pendingDrumMap=false;
 mutable juce::var cachedWebSnapshot,cachedWebRows;
 mutable std::unordered_map<std::string,juce::String> sourceLabels;
 mutable int cachedWebSelection=-1;
 pluginLib::patchDB::PresetSelection presetSelection;
 static constexpr unsigned KProgram=0, KSSingle=1, KSParts=2;
 unsigned displayedSelectionSlot=~0u;
 unsigned selectionSlot() const;
 void selectRowWithoutActivation(int);
 void followSelectedPart();
 juce::File indexedFirmware;
 juce::int64 lastActivationTime=0;
 juce::String cellText(const pluginLib::patchDB::PatchPtr&,int)const;
 juce::String sourceLabel(const pluginLib::patchDB::DataSourceNode&)const;
 void openSourceFolder(const pluginLib::patchDB::PatchPtr&);
 int rowForPatchName(const juce::String&)const;
 void restoreFirmwareSelection();
 void showLoadStatus(const pluginLib::patchDB::PatchPtr&,unsigned part,const juce::String& conversion);
 void updateRows(std::vector<pluginLib::patchDB::PatchPtr>);
 void sortAndPresentRows(std::vector<pluginLib::patchDB::PatchPtr>);
 void updateBanks(std::vector<pluginLib::patchDB::DataSourceNodePtr>&&);
 void sortOrderChanged(int,bool)override;
 void query();void timerCallback()override;void choose(bool folder,bool paths=false);bool activate(int,juce::String* error=nullptr);
 void chooseSaveScope();
 void saveUserPatch(Processor::PatchSaveScope);
 void exportSelectedPatch(const pluginLib::patchDB::PatchPtr&);
 bool canRemoveSource()const;
 juce::String getCellTooltip(int row,int column)override{return row>=0 && row<int(rows.size())?cellText(rows[row],column):juce::String{};}
 int getNumRows()override{return int(rows.size());}
 void paintRowBackground(juce::Graphics&,int,int,int,bool)override;
 void paintCell(juce::Graphics&,int,int,int,int,bool)override;
 void selectedRowsChanged(int)override;
 void cellClicked(int,int,const juce::MouseEvent&)override;
 void cellDoubleClicked(int,int,const juce::MouseEvent&)override;
 void returnKeyPressed(int row)override{activate(row);}
};
}
