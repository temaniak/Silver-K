#include "silverSkin.h"
#include "silverProcessor.h"
#include "BinaryData.h"
#include <set>
#include <cmath>

namespace silverK
{
namespace
{
struct BaseFont final:juce::DynamicObject { explicit BaseFont(juce::Font f):font(std::move(f)){} juce::Font font; };
juce::var object() { return juce::var(new juce::DynamicObject); }
juce::var embedded(const char* name)
{
    int size=0; const auto* data=BinaryData::getNamedResource(name,size);
    return juce::JSON::parse(juce::String::fromUTF8(data,size));
}
juce::var merge(juce::var base, const juce::var& changes)
{
    if (!base.getDynamicObject() || !changes.getDynamicObject()) return changes.clone();
    for (const auto& p:changes.getDynamicObject()->getProperties())
        base.getDynamicObject()->setProperty(p.name,merge(base[p.name],p.value));
    return base;
}
juce::String paletteKey(juce::uint32 value)
{
    switch(value | 0xff000000u)
    {
        case 0xff171b20: return "text";
        case 0xff087fe5: return "accent";
        case 0xfff1f4f7: return "lightText";
        case 0xffced1d5: return "background";
        case 0xff24262a: return "dark";
        case 0xffeef1f5: return "panelTop";
        case 0xffe4eaf0: return "panelBottom";
        case 0xffbdc7cf: return "border";
        case 0xff0752ba: return "display";
        default: return juce::String::toHexString(value | 0xff000000u).paddedLeft('0',8);
    }
}
const std::map<juce::String,int>& juceColourIds()
{
    static const std::map<juce::String,int> ids={
        {"menuBackground",juce::PopupMenu::backgroundColourId},{"menuText",juce::PopupMenu::textColourId},{"menuHighlight",juce::PopupMenu::highlightedBackgroundColourId},{"menuHighlightText",juce::PopupMenu::highlightedTextColourId},
        {"editorBackground",juce::TextEditor::backgroundColourId},{"editorText",juce::TextEditor::textColourId},{"editorOutline",juce::TextEditor::outlineColourId},{"editorFocus",juce::TextEditor::focusedOutlineColourId},{"editorSelection",juce::TextEditor::highlightColourId},{"editorSelectionText",juce::TextEditor::highlightedTextColourId},
        {"tableBackground",juce::ListBox::backgroundColourId},{"tableHeaderBackground",juce::TableHeaderComponent::backgroundColourId},{"tableHeaderText",juce::TableHeaderComponent::textColourId},{"tableHeaderOutline",juce::TableHeaderComponent::outlineColourId},{"tableHeaderHighlight",juce::TableHeaderComponent::highlightColourId},
        {"scrollbarThumb",juce::ScrollBar::thumbColourId},{"scrollbarBackground",juce::ScrollBar::backgroundColourId},{"sliderTrack",juce::Slider::trackColourId},{"sliderThumb",juce::Slider::thumbColourId},{"sliderBackground",juce::Slider::backgroundColourId},
        {"tooltipBackground",juce::TooltipWindow::backgroundColourId},{"tooltipText",juce::TooltipWindow::textColourId},{"tooltipOutline",juce::TooltipWindow::outlineColourId},{"toggleText",juce::ToggleButton::textColourId},{"toggleTick",juce::ToggleButton::tickColourId},{"toggleDisabledTick",juce::ToggleButton::tickDisabledColourId},{"comboText",juce::ComboBox::textColourId}
    };return ids;
}
bool number(const juce::var& v) { return (v.isInt() || v.isInt64() || v.isDouble()) && std::isfinite(double(v)); }
bool rectangle(const juce::var& v)
{
    if (!v.isArray() || v.size()!=4) return false;
    for (int n=0;n<4;++n) if (!number(v[n]) || std::abs(double(v[n]))>16384 || double(v[n])!=int(v[n])) return false;
    return int(v[2])>0 && int(v[3])>0;
}
juce::Rectangle<int> bounds(const juce::var& v) { return {int(v[0]),int(v[1]),int(v[2]),int(v[3])}; }
bool colourString(const juce::var& v)
{
    const auto s=v.toString();return v.isString() && s.startsWithChar('#') && (s.length()==7 || s.length()==9) && s.substring(1).containsOnly("0123456789abcdefABCDEF");
}
juce::Colour parseColour(const juce::var& v)
{
    auto s=v.toString().substring(1);return juce::Colour(juce::uint32(s.getHexValue64()) | (s.length()==6?0xff000000u:0));
}
juce::File localFile(const juce::File& root, const juce::String& path)
{
    if (path.isEmpty() || juce::File::isAbsolutePath(path) || path.containsChar('\\') || path.containsChar(':')) return {};
    const auto parts=juce::StringArray::fromTokens(path,"/","");
    auto f=root;
    for (const auto& part:parts)
    {
        if (part.isEmpty() || part=="." || part=="..") return {};
        f=f.getChildFile(part); if (f.isSymbolicLink()) return {};
    }
    return f.isAChildOf(root)?f:juce::File{};
}
bool readJson(const juce::File& file, juce::var& result, juce::String& error)
{
    if (!file.existsAsFile() || file.getSize()>4*1024*1024) { error="Missing or oversized JSON: "+file.getFileName();return false; }
    const auto status=juce::JSON::parse(file.loadFileAsString(),result);
    if (status.failed() || !result.getDynamicObject()) { error=file.getFileName()+": "+(status.failed()?status.getErrorMessage():"expected an object");return false; }
    return true;
}
bool validControl(const juce::String& id)
{
    if(id.isEmpty())return false;
    juce::StringArray variants{id};
    for (const auto& token:juce::StringArray{"{osc}","{lfo}","{pw}"})
    {
        juce::StringArray next;
        const auto values=token=="{pw}"?juce::StringArray{"pulse_width","lfo2_pwm","env_pwm"}:token=="{osc}"?juce::StringArray{"1","2","3"}:juce::StringArray{"1","2"};
        for (const auto& value:variants) { if (!value.contains(token)) next.add(value);else for(const auto& replacement:values)next.add(value.replace(token,replacement)); }
        variants=next;
    }
    static const auto known=[] { std::set<juce::String> ids;for(const auto& c:ksPlugin::controls)ids.insert(semanticId(c.id));for(const auto& c:kControls())ids.insert(semanticId(c.id));return ids; }();
    for(const auto& variant:variants)if(!known.count(semanticId(variant)))return false;
    return true;
}
bool validateLayout(const juce::var& l, juce::String& error)
{
    auto fail=[&](const juce::String& s){error="Layout: "+s;return false;};
    if(!number(l["width"]) || !number(l["height"]) || int(l["width"])<320 || int(l["width"])>4096 || int(l["height"])<320 || int(l["height"])>4096)return fail("width and height must be between 320 and 4096");
    if(l["layout"].toString()!="studio" && l["layout"].toString()!="legacy")return fail("layout must be studio or legacy");
    for(const auto& key:juce::StringArray{"header","title","lcd","fmButton","effectMenuArea","effectSelect"})if(!rectangle(l[juce::Identifier(key)]))return fail("invalid rectangle: "+key);
    const auto canvas=l["performanceCanvas"];
    if(!canvas.isArray() || canvas.size()!=2 || !number(canvas[0]) || !number(canvas[1]) || int(canvas[0])<320 || int(canvas[0])>4096 || int(canvas[1])<320 || int(canvas[1])>4096)return fail("invalid performanceCanvas size");
    const auto baseline=embedded("silverk_json");
    for(const auto& key:juce::StringArray{"actions","advanced","setup","selectors","studioHeader","studioRegions"})
    {
        const auto node=l[juce::Identifier(key)], ref=baseline[juce::Identifier(key)];
        if(!node.getDynamicObject())return fail("expected object: "+key);
        for(const auto& p:ref.getDynamicObject()->getProperties())
        {
            const auto v=node[p.name];
            if(p.value.isArray() && p.value.size()==4 && number(p.value[0])) { if(!rectangle(v))return fail("invalid rectangle: "+key+"."+p.name.toString()); }
            else if(p.value.isArray())
            {
                if(!v.isArray() || v.size()!=p.value.size())return fail("invalid array: "+key+"."+p.name.toString());
                for(const auto& r:*v.getArray())if(!rectangle(r))return fail("invalid rectangle: "+key+"."+p.name.toString());
            }
            else if(!number(v) || int(v)<1 || int(v)>4096)return fail("invalid size: "+key+"."+p.name.toString());
        }
    }
    for(const auto& key:juce::StringArray{"knobDiameter","cutoffDiameter","classicDiameter"})if(!number(l[juce::Identifier(key)]) || int(l[juce::Identifier(key)])<4 || int(l[juce::Identifier(key)])>512)return fail("invalid knob size: "+key);
    const juce::StringArray kinds{"menu","knob","mixer","compact","row","extraRow","fader","led","lamps","waveButtons","panelKnob","panelMenu","classic","miniKnob","inline","rhythm"};
    auto controls=[&](const juce::var& list){
        if(!list.isArray() || list.size()>512)return false;
        std::set<juce::String> seen;
        for(const auto& c:*list.getArray())if(!c.getDynamicObject() || !validControl(c["id"].toString()) || !seen.insert(c["id"].toString()).second || !kinds.contains(c["kind"].toString()) || !rectangle(c["rect"]))return false;
        return true;
    };
    if(!controls(l["controls"]) || !controls(l["studioModels"]["K"]) || !controls(l["studioModels"]["KS"]))return fail("invalid controls, duplicate IDs or unknown parameter/kind");
    if(!l["sections"].isArray() || l["sections"].size()>128)return fail("invalid sections");
    for(const auto& s:*l["sections"].getArray()){
        if(!s.isArray() || s.size()!=5 || !s[4].isString())return fail("invalid section");
        juce::Array<juce::var> r;for(int n=0;n<4;++n)r.add(s[n]);if(!rectangle(juce::var(r)))return fail("invalid section rectangle");
    }
    for(const auto& key:juce::StringArray{"pwmSelectors","lfoSelectors","effectSlots","effectMenus"}){
        const auto list=l[juce::Identifier(key)];if(!list.isArray() || list.size()!=baseline[juce::Identifier(key)].size())return fail("invalid array: "+key);
        for(const auto& r:*list.getArray())if(!rectangle(r))return fail("invalid rectangle: "+key);
    }
    if(!l["effects"].isArray() || l["effects"].size()<1 || l["effects"].size()>32)return fail("expected 1 to 32 effects");
    for(const auto& fx:*l["effects"].getArray()){
        if(!fx.getDynamicObject() || fx["name"].toString().isEmpty() || !fx["labels"].isArray())return fail("invalid effect");
        for(const auto& key:juce::StringArray{"controls","menus"}){
            const auto list=fx[juce::Identifier(key)];if(!list.isArray() || list.size()>(key=="controls"?6:16))return fail("invalid effect "+key);
            for(const auto& id:*list.getArray())if(!validControl(id.toString()))return fail("unknown effect parameter: "+id.toString());
        }
        if(fx["labels"].size()!=fx["controls"].size())return fail("effect labels do not match controls");
    }
    if(l.hasProperty("components")){
        if(!l["components"].getDynamicObject() || l["components"].getDynamicObject()->getProperties().size()>4096)return fail("invalid components");
        for(const auto& p:l["components"].getDynamicObject()->getProperties())if(!p.value.getDynamicObject() || (p.value.hasProperty("bounds")&&!rectangle(p.value["bounds"])))return fail("invalid component: "+p.name.toString());
    }
    if(l.hasProperty("decorations")){
        if(!l["decorations"].isArray() || l["decorations"].size()>512)return fail("invalid decorations");
        std::set<juce::String> ids;
        for(const auto& d:*l["decorations"].getArray())if(!d.getDynamicObject() || !rectangle(d["rect"]) || !juce::StringArray{"image","text","panel"}.contains(d["kind"].toString()) || d["id"].toString().isEmpty() || !ids.insert(d["id"].toString()).second || (d.hasProperty("colour")&&!colourString(d["colour"])) || (d.hasProperty("fontSize")&&(!number(d["fontSize"]) || double(d["fontSize"])<4 || double(d["fontSize"])>256)))return fail("invalid decoration");
    }
    return true;
}
const SkinLookAndFeel* look(const juce::Component& c) { return dynamic_cast<const SkinLookAndFeel*>(&c.getLookAndFeel()); }
juce::String componentPath(const juce::Component& c)
{
    const auto id=c.getComponentID();if(id=="surface")return {};
    const auto* parent=c.getParentComponent();const auto prefix=parent?componentPath(*parent):juce::String{};
    return id.isEmpty()?prefix:prefix.isEmpty()?id:prefix+"/"+id;
}
juce::var componentOptions(const juce::var& map,const juce::Component& c)
{
    if(c.getComponentID().isEmpty())return {};
    const auto byId=map[juce::Identifier(c.getComponentID())];const auto path=componentPath(c);const auto byPath=path.isEmpty()?juce::var{}:map[juce::Identifier(path)];
    return byPath.getDynamicObject()?merge(byId.clone(),byPath):byId;
}
}

SkinPackage SkinPackage::builtin()
{
    SkinPackage s;s.id="builtin";s.name="Silver-K";s.layout=embedded("silverk_json");s.theme=embedded("silverktheme_json");
    const std::pair<const char*,const char*> assets[]={{"knobBody","silverkknobbody_png"},{"knobGrip","silverkknobgrip_png"},{"knobBase","silverkstandknobbase_png"},{"buttonOff","silverkbuttonoff_png"},{"buttonOn","silverkbuttonon_png"}};
    for(const auto& a:assets){int size=0;const auto* data=BinaryData::getNamedResource(a.second,size);s.images[a.first]=juce::ImageFileFormat::loadFrom(data,size);}
    return s;
}
bool SkinPackage::load(const juce::File& file,SkinPackage& result,juce::String& error)
{
    auto candidate=builtin();juce::var manifest;
    const auto entry=file.isDirectory()?file.getChildFile("skin.json"):file;
    if(!readJson(entry,manifest,error))return false;
    candidate.manifest=entry;
    if(manifest.hasProperty("schemaVersion")){
        if(!manifest["schemaVersion"].isInt() || int(manifest["schemaVersion"])!=1 || manifest["engine"].toString()!="silver-k-juce"){error="Unsupported skin schemaVersion or engine";return false;}
        candidate.id=manifest["id"].toString();candidate.name=manifest["name"].toString();
        if(candidate.id.isEmpty() || candidate.id.length()>128 || !candidate.id.containsOnly("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-") || candidate.name.isEmpty()){error="Skin needs a name and an ID using letters, digits, dots, underscores or hyphens";return false;}
        const auto root=entry.getParentDirectory();juce::var layout,theme;
        if(!readJson(localFile(root,manifest["layout"].toString()),layout,error) || !readJson(localFile(root,manifest["theme"].toString()),theme,error))return false;
        candidate.layout=merge(candidate.layout,layout);candidate.theme=merge(candidate.theme,theme);
        const auto images=candidate.theme["images"];
        if(!images.getDynamicObject()){error="Theme images must be an object";return false;}
        for(const auto& p:images.getDynamicObject()->getProperties()){
            const auto resource=localFile(root,p.value.toString());
            if(!resource.existsAsFile() || resource.getSize()>32*1024*1024){error="Missing or oversized image: "+p.value.toString();return false;}
            auto image=juce::ImageFileFormat::loadFrom(resource);
            if(!image.isValid() || image.getWidth()>8192 || image.getHeight()>8192){error="Invalid image: "+p.value.toString();return false;}
            candidate.images[p.name.toString()]=image;
        }
        const auto font=candidate.theme["font"]["file"].toString();
        if(font.isNotEmpty()){
            juce::MemoryBlock bytes;const auto resource=localFile(root,font);
            if(!resource.existsAsFile() || resource.getSize()>16*1024*1024 || !resource.loadFileAsData(bytes) || !(candidate.typeface=juce::Typeface::createSystemTypefaceFor(bytes.getData(),bytes.getSize()))){error="Invalid font: "+font;return false;}
        }
    }else{
        // Existing standalone Silver-K layout JSONs remain readable.
        if(!manifest["controls"].isArray() || !manifest["effects"].isArray()){error="Expected a skin package manifest or a Silver-K layout JSON";return false;}
        candidate.id="legacy";candidate.name=manifest["name"].toString();candidate.layout=merge(candidate.layout,manifest);if(manifest["layout"].toString()!="studio")candidate.layout.getDynamicObject()->setProperty("layout","legacy");
    }
    if(!validateLayout(candidate.layout,error))return false;
    const auto palette=candidate.theme["palette"];
    if(!palette.getDynamicObject()){error="Theme palette must be an object";return false;}
    for(const auto& p:palette.getDynamicObject()->getProperties())if(!colourString(p.value)){error="Invalid palette colour: "+p.name.toString();return false;}
    if(candidate.theme.hasProperty("renderStyle") && !juce::StringArray{"classic","flat"}.contains(candidate.theme["renderStyle"].toString())){error="Theme renderStyle must be classic or flat";return false;}
    const auto font=candidate.theme["font"];
    if(!font.getDynamicObject() || !number(font["scale"]) || double(font["scale"])<.5 || double(font["scale"])>2){error="Font scale must be between 0.5 and 2";return false;}
    const auto juceColours=candidate.theme["juceColours"];
    if(!juceColours.getDynamicObject()){error="Theme juceColours must be an object";return false;}
    for(const auto& p:juceColours.getDynamicObject()->getProperties())if(!juceColourIds().count(p.name.toString()) || !colourString(p.value)){error="Invalid JUCE colour: "+p.name.toString();return false;}
    const auto styles=candidate.theme["components"];
    if(!styles.getDynamicObject()){error="Theme components must be an object";return false;}
    for(const auto& p:styles.getDynamicObject()->getProperties()){
        const auto style=p.value;
        if(!style.getDynamicObject()){error="Invalid component style: "+p.name.toString();return false;}
        if(style.hasProperty("fontSize") && (!number(style["fontSize"]) || double(style["fontSize"])<4 || double(style["fontSize"])>256)){error="Invalid component fontSize";return false;}
        if(style.hasProperty("palette")){
            if(!style["palette"].getDynamicObject()){error="Invalid component palette";return false;}
            for(const auto& colour:style["palette"].getDynamicObject()->getProperties())if(!colourString(colour.value)){error="Invalid component colour";return false;}
        }
        for(const auto& key:juce::StringArray{"backgroundImage","knobStrip","buttonOff","buttonOn","faderTrack","faderThumb"})if(style.hasProperty(juce::Identifier(key))&&!candidate.images.count(style[juce::Identifier(key)].toString())){error="Unknown image in component style: "+key;return false;}
    }
    if(candidate.theme.hasProperty("knobStrip")){
        const auto strip=candidate.theme["knobStrip"];const int frames=int(strip["frames"]);const auto found=candidate.images.find(strip["image"].toString());
        if(!strip.getDynamicObject() || frames<2 || frames>1024 || found==candidate.images.end() || !juce::StringArray{"vertical","horizontal"}.contains(strip["orientation"].toString()) || (strip["orientation"].toString()=="horizontal"?found->second.getWidth():found->second.getHeight())%frames!=0){error="Invalid knobStrip image, frame count or orientation";return false;}
    }
    for(const auto& p:styles.getDynamicObject()->getProperties())if(p.value.hasProperty("knobStrip")){
        const auto config=candidate.theme["knobStrip"];const int frames=int(config["frames"]);const auto& bitmap=candidate.images.at(p.value["knobStrip"].toString());
        if(frames<2 || frames>1024 || (config["orientation"].toString()=="horizontal"?bitmap.getWidth():bitmap.getHeight())%frames!=0){error="Component knobStrip does not match global frame geometry";return false;}
    }
    if(auto* array=candidate.layout["decorations"].getArray())for(const auto& d:*array){
        if(d["kind"].toString()=="image"&&!candidate.images.count(d["image"].toString())){error="Unknown decoration image: "+d["image"].toString();return false;}
        juce::StringArray parents{"","surface","mainPanel","patchBrowser","performancePanel","performanceCanvas","localExtra","advancedContent"};
        for(int n=1;n<=4;++n){parents.add("performance:part"+juce::String(n)+":strip");parents.add("performance:part"+juce::String(n)+":controls");}
        if(!parents.contains(d["parent"].toString())){error="Unknown decoration parent: "+d["parent"].toString();return false;}
    }
    result=std::move(candidate);error.clear();return true;
}
std::vector<std::pair<juce::String,juce::File>> SkinPackage::discover(const juce::File& folder)
{
    std::vector<std::pair<juce::String,juce::File>> result;
    auto entries=folder.findChildFiles(juce::File::findDirectories,false);entries.sort();
    for(const auto& dir:entries){const auto file=dir.getChildFile("skin.json");juce::var manifest;juce::String ignored;if(file.existsAsFile()&&readJson(file,manifest,ignored))result.emplace_back(manifest.getProperty("name",dir.getFileName()).toString(),file);}
    return result;
}
bool SkinPackage::exportTemplate(const juce::File& folder,juce::File& result,juce::String& error)
{
    if(const auto status=folder.createDirectory();status.failed()){error=status.getErrorMessage();return false;}
    auto target=folder.getNonexistentChildFile("Silver-K Template",{},false);
    if(const auto status=target.getChildFile("assets").createDirectory();status.failed()){error=status.getErrorMessage();return false;}
    auto manifest=object();auto* m=manifest.getDynamicObject();m->setProperty("schemaVersion",1);m->setProperty("engine","silver-k-juce");m->setProperty("id","silver-k-"+juce::Uuid().toString());m->setProperty("name",target.getFileName());m->setProperty("author","");m->setProperty("layout","layout.json");m->setProperty("theme","theme.json");
    auto skin=builtin();auto theme=skin.theme.clone();auto images=object();
    const std::pair<const char*,const char*> assets[]={{"knobBody","silverkknobbody_png"},{"knobGrip","silverkknobgrip_png"},{"knobBase","silverkstandknobbase_png"},{"buttonOff","silverkbuttonoff_png"},{"buttonOn","silverkbuttonon_png"}};
    bool ok=true;
    for(const auto& a:assets){int size=0;const auto* data=BinaryData::getNamedResource(a.second,size);const auto name=juce::String("assets/")+a.first+".png";ok=ok&&data&&target.getChildFile(name).replaceWithData(data,size);images.getDynamicObject()->setProperty(a.first,name);}
    theme.getDynamicObject()->setProperty("images",images);
    ok=ok&&target.getChildFile("layout.json").replaceWithText(juce::JSON::toString(skin.layout))&&target.getChildFile("theme.json").replaceWithText(juce::JSON::toString(theme));
    // Publish the manifest last so scans never list an incomplete export.
    ok=ok&&target.getChildFile("skin.json").replaceWithText(juce::JSON::toString(manifest));
    if(!ok){target.deleteRecursively();error="Cannot export skin template";return false;}
    result=target.getChildFile("skin.json");return true;
}

SkinLookAndFeel::SkinLookAndFeel(const SkinPackage& s):package(s)
{
    const std::pair<int,juce::uint32> colours[]={{juce::PopupMenu::backgroundColourId,0xff202a34},{juce::PopupMenu::textColourId,0xfff1f4f7},{juce::PopupMenu::highlightedBackgroundColourId,0xff087fe5},{juce::ComboBox::textColourId,0xfff1f4f7},{juce::ToggleButton::textColourId,0xff171b20},{juce::ToggleButton::tickColourId,0xff087fe5},{juce::TextEditor::textColourId,0xff171b20},{juce::TextEditor::backgroundColourId,0xfff1f4f7},{juce::TextEditor::outlineColourId,0xffbdc7cf},{juce::TextEditor::focusedOutlineColourId,0xff087fe5},{juce::ListBox::backgroundColourId,0xffe5e7ea},{juce::TableHeaderComponent::backgroundColourId,0xffced1d5},{juce::TableHeaderComponent::textColourId,0xff171b20},{juce::TableHeaderComponent::outlineColourId,0xffbdc7cf},{juce::TableHeaderComponent::highlightColourId,0x33087fe5},{juce::ScrollBar::thumbColourId,0xffbdc7cf},{juce::Slider::trackColourId,0xff087fe5},{juce::Slider::thumbColourId,0xfff1f4f7},{juce::Slider::backgroundColourId,0xff24262a},{juce::TooltipWindow::backgroundColourId,0xff202a34},{juce::TooltipWindow::textColourId,0xfff1f4f7},{juce::TooltipWindow::outlineColourId,0xffbdc7cf}};
    for(const auto& c:colours)setColour(c.first,colour(c.second));
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);setColour(juce::ToggleButton::tickDisabledColourId,colour(0xff171b20).withAlpha(.4f));
    for(const auto& p:package.theme["juceColours"].getDynamicObject()->getProperties()){
        const auto found=juceColourIds().find(p.name.toString());if(found!=juceColourIds().end())setColour(found->second,parseColour(p.value));
    }
}
juce::Colour SkinLookAndFeel::colour(juce::uint32 original)const
{
    const auto c=juce::Colour(original);const auto v=package.theme["palette"][juce::Identifier(paletteKey(original))];
    return colourString(v)?parseColour(v).withMultipliedAlpha(c.getFloatAlpha()):c;
}
juce::Font SkinLookAndFeel::font(float height,int style)const
{
    const auto settings=package.theme["font"];height*=float(settings.getProperty("scale",1.));
    if(package.typeface)return juce::Font(package.typeface).withHeight(height).withStyle(style);
    const auto family=settings["family"].toString();return family.isEmpty()?juce::Font(height,style):juce::Font(family,height,style);
}
const juce::Image& SkinLookAndFeel::image(const juce::String& id)const{const auto it=package.images.find(id);static const juce::Image empty;return it==package.images.end()?empty:it->second;}
juce::Typeface::Ptr SkinLookAndFeel::getTypefaceForFont(const juce::Font& f){return package.typeface?package.typeface:LookAndFeel_V4::getTypefaceForFont(f);}
juce::Font SkinLookAndFeel::getPopupMenuFont(){const auto base=LookAndFeel_V4::getPopupMenuFont();return font(base.getHeight(),base.getStyleFlags());}
juce::Font SkinLookAndFeel::getTextButtonFont(juce::TextButton&,int){return font(13.f,juce::Font::bold);}
juce::Font SkinLookAndFeel::getLabelFont(juce::Label& l){return l.getFont();}
juce::Font SkinLookAndFeel::getComboBoxFont(juce::ComboBox&){return font(12.5f);}
juce::Font SkinLookAndFeel::getSliderPopupFont(juce::Slider&){return font(13.f);}
bool SkinLookAndFeel::drawKnobStrip(juce::Graphics& g,juce::Slider& s,juce::Rectangle<float> r,float position)const
{
    const auto config=package.theme["knobStrip"];const auto style=s.getProperties()["silverSkinStyle"];
    const auto& bitmap=image(style.getProperty("knobStrip",config["image"]).toString());const int frames=int(config["frames"]);
    if(!bitmap.isValid() || frames<2)return false;
    const bool horizontal=config["orientation"].toString()=="horizontal";const int w=bitmap.getWidth()/(horizontal?frames:1),h=bitmap.getHeight()/(horizontal?1:frames),frame=juce::jlimit(0,frames-1,juce::roundToInt(position*(frames-1)));
    g.drawImage(bitmap,r.getX(),r.getY(),r.getWidth(),r.getHeight(),horizontal?frame*w:0,horizontal?0:frame*h,w,h);return true;
}
juce::Colour skinColour(const juce::Component& c,juce::uint32 value)
{
    if(const auto* l=look(c)){
        const auto palette=c.getProperties()["silverSkinStyle"]["palette"];const auto v=palette[juce::Identifier(paletteKey(value))];
        return colourString(v)?parseColour(v).withMultipliedAlpha(juce::Colour(value).getFloatAlpha()):l->colour(value);
    }
    return juce::Colour(value);
}
bool skinIsFlat(const juce::Component& c){const auto* l=look(c);return l && l->isFlat();}
void setSkinColour(juce::Component& c,int id,juce::Colour value)
{
    c.getProperties().set(juce::Identifier("silverSkinColour_"+juce::String::toHexString(id)),int(value.getARGB()));
    c.setColour(id,skinColour(c,value));
}
juce::Font skinFont(const juce::Component& c,float height,int style){
    const auto size=c.getProperties()["silverSkinStyle"]["fontSize"];if(number(size))height=float(size);
    if(const auto* l=look(c))return l->font(height,style);return juce::Font(height,style);
}
void applySkin(juce::Component& c,bool includeRoot)
{
    const auto* l=look(c);if(!l)return;
    if(includeRoot){
        const auto geometry=componentOptions(l->package.layout["components"],c);
        if(c.getComponentID()!="surface" && geometry.hasProperty("bounds"))c.setBounds(bounds(geometry["bounds"]));
        if(!bool(c.getProperties()["silverSkinApplied"])){
            const auto style=componentOptions(l->package.theme["components"],c);c.getProperties().set("silverSkinStyle",style);c.getProperties().set("silverSkinApplied",true);
            // Pinned JUCE stores explicit component colours in jcclr_<hex ID>.
            juce::Array<std::pair<int,juce::Colour>> updates;
            for(const auto& p:c.getProperties())if(p.name.toString().startsWith("jcclr_")){
                const auto id=p.name.toString().substring(6);const auto original=c.getProperties().getWithDefault(juce::Identifier("silverSkinColour_"+id),p.value);
                updates.add({id.getHexValue32(),skinColour(c,juce::uint32(juce::int64(original)))});
            }
            for(const auto& p:updates)c.setColour(p.first,p.second);
            for(const auto& p:l->package.theme["juceColours"].getDynamicObject()->getProperties()){
                const auto found=juceColourIds().find(p.name.toString());if(found!=juceColourIds().end()&&c.isColourSpecified(found->second))c.setColour(found->second,parseColour(p.value));
            }
            if(style.hasProperty("align"))if(auto* label=dynamic_cast<juce::Label*>(&c))label->setJustificationType(style["align"].toString()=="centre"?juce::Justification::centred:style["align"].toString()=="right"?juce::Justification::centredRight:juce::Justification::centredLeft);
            for(const auto& key:juce::StringArray{"rotaryStyle","rotaryDiameter","standKnob","blueVolume","captionWidth"})if(style.hasProperty(juce::Identifier(key)))c.getProperties().set(juce::Identifier(key),style[juce::Identifier(key)]);
        }
        const auto style=c.getProperties()["silverSkinStyle"];
        if(auto* label=dynamic_cast<juce::Label*>(&c)){
            auto& props=label->getProperties();
            if(!props.contains("silverSkinBaseFont"))props.set("silverSkinBaseFont",juce::var(new BaseFont(label->getFont())));
            const auto base=static_cast<BaseFont*>(props["silverSkinBaseFont"].getDynamicObject())->font;const float height=float(style.getProperty("fontSize",base.getHeight()));
            const auto f=l->package.typeface || l->package.theme["font"]["family"].toString().isNotEmpty()?l->font(height,base.getStyleFlags()):base.withHeight(height*float(l->package.theme["font"]["scale"]));
            if(label->getFont()!=f)label->setFont(f);
        }
        if(auto* editor=dynamic_cast<juce::TextEditor*>(&c)){
            auto& props=editor->getProperties();if(!props.contains("silverSkinBaseFont"))props.set("silverSkinBaseFont",juce::var(new BaseFont(editor->getFont())));
            const auto base=static_cast<BaseFont*>(props["silverSkinBaseFont"].getDynamicObject())->font;const auto f=l->font(float(style.getProperty("fontSize",base.getHeight())));if(editor->getFont()!=f)editor->setFont(f);
        }
    }
    for(auto* child:c.getChildren())applySkin(*child);
}
bool paintSkinBackground(const juce::Component& c,juce::Graphics& g)
{
    const auto* l=look(c);if(!l)return false;
    const auto style=c.getProperties()["silverSkinStyle"];const auto& image=l->image(style["backgroundImage"].toString());
    if(image.isValid()){g.drawImage(image,c.getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);return true;}
    return false;
}
bool skinDrawPanels(const juce::Component& c)
{
    const auto* l=look(c);return !l || (bool(l->package.layout.getProperty("drawPanels",true))&&!bool(c.getProperties()["silverSkinStyle"]["hideDecoration"]));
}
SkinDecoration::SkinDecoration(juce::var d):description(std::move(d))
{
    setComponentID("decoration:"+description["id"].toString());setInterceptsMouseClicks(false,false);setBounds(bounds(description["rect"]));
}
void SkinDecoration::paint(juce::Graphics& g)
{
    const auto* l=look(*this);if(!l)return;const auto kind=description["kind"].toString();
    if(kind=="image"){g.drawImage(l->image(description["image"].toString()),getLocalBounds().toFloat(),juce::RectanglePlacement::stretchToFit);return;}
    g.setColour(description.hasProperty("colour")?parseColour(description["colour"]):l->colour(0xff171b20));
    if(kind=="panel")g.fillRoundedRectangle(getLocalBounds().toFloat(),float(description.getProperty("radius",0.)));
    else{g.setFont(l->font(float(description.getProperty("fontSize",14.)),bool(description["bold"])?juce::Font::bold:juce::Font::plain));g.drawFittedText(description["text"].toString(),getLocalBounds(),description["align"].toString()=="centre"?juce::Justification::centred:description["align"].toString()=="right"?juce::Justification::centredRight:juce::Justification::centredLeft,4);}
}
}
