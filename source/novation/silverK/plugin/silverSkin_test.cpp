#include "silverSkin.h"
#include "silverEditor.h"
#include "silverImageScale.h"
#include <iostream>
#include <stdexcept>
using namespace silverK;
namespace
{
void check(bool ok,const juce::String& message){if(!ok)throw std::runtime_error(message.toStdString());}
juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* found=find(*child,id))return found;return nullptr;}
juce::var object(){return juce::var(new juce::DynamicObject);}
void put(juce::var& target,const char* key,juce::var value){target.getDynamicObject()->setProperty(key,value);}
juce::var box(int x,int y,int w,int h){return juce::var(juce::Array<juce::var>{x,y,w,h});}
void json(const juce::File& file,const juce::var& value){check(file.replaceWithText(juce::JSON::toString(value)),"Write JSON fixture");}
void image(const juce::File& file,juce::Colour colour){juce::Image image(juce::Image::ARGB,16,16,true);juce::Graphics g(image);g.fillAll(colour);file.deleteFile();auto stream=file.createOutputStream();check(stream && juce::PNGImageFormat().writeImageToStream(image,*stream),"Write PNG fixture");}
void click(juce::Component& c,const char* id){auto* button=dynamic_cast<juce::TextButton*>(find(c,id));check(button&&bool(button->onClick),"Missing action: "+juce::String(id));button->onClick();}
void capture(juce::Component& c,const juce::File& file){file.deleteFile();auto output=file.createOutputStream();check(output && juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*output),"Capture "+file.getFileName());}
void imageScaling()
{
    juce::Image stripes(juce::Image::ARGB,512,512,true,juce::SoftwareImageType());
    juce::Image alpha(juce::Image::ARGB,512,512,true,juce::SoftwareImageType());
    for(int y=0;y<512;++y)for(int x=0;x<512;++x){
        stripes.setPixelAt(x,y,x%3==0?juce::Colours::white:juce::Colours::black);
        alpha.setPixelAt(x,y,x%3==0?juce::Colours::white:juce::Colours::transparentBlack);
    }
    ImageScaleCache cache;
    const auto filtered=cache.forSize(stripes,64,64),filteredAlpha=cache.forSize(alpha,64,64);
    juce::Image direct(juce::Image::ARGB,64,64,true,juce::SoftwareImageType());
    {juce::Graphics g(direct);g.drawImage(stripes,direct.getBounds().toFloat(),juce::RectanglePlacement::stretchToFit);}
    int unfilteredError=0;
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){
        int white=0;for(int sx=x*8;sx<(x+1)*8;++sx)white+=sx%3==0;
        const int expected=juce::roundToInt(255.*white/8.);
        const auto c=filtered.getPixelAt(x,y),a=filteredAlpha.getPixelAt(x,y);
        check(std::abs(int(c.getRed())-expected)<=2 && c.getAlpha()==255,"Fine lines alias when reduced");
        check(std::abs(int(a.getAlpha())-expected)<=2 && a.getRed()>=253 && a.getGreen()>=253 && a.getBlue()>=253,"Filtering introduced a dark transparency fringe");
        unfilteredError+=std::abs(int(direct.getPixelAt(x,y).getRed())-expected);
    }
    check(unfilteredError>64*64*20,"Raster fixture does not reproduce the software-renderer defect");
    check(cache.forSize(stripes,64,64)==filtered,"Repeated repaint rebuilt the filtered bitmap");
    check(cache.forSize(stripes,1024,1024)==stripes,"Cache enlarged the original artwork");
    auto replacement=stripes.createCopy();{juce::Graphics g(replacement);g.fillAll(juce::Colours::red);}
    check(cache.forSize(replacement,64,64).getPixelAt(32,32)==juce::Colours::red,"New artwork reused a stale bitmap");
    for(float ui:{.5f,.75f,1.f,1.5f})for(float dpi:{1.f,1.25f,1.5f,2.f}){
        juce::Image output(juce::Image::ARGB,256,256,true,juce::SoftwareImageType());juce::Graphics g(output);
        g.addTransform(juce::AffineTransform::scale(ui*dpi));
        const auto bitmap=cache.forSoftwareDrawing(g,stripes,72.f,72.f);
        const int expected=juce::roundToInt(std::ceil(72.f*ui*dpi));
        check(bitmap.getWidth()==expected && bitmap.getHeight()==expected,"Editor/DPI scale selected the wrong bitmap size");
        check(cache.forSoftwareDrawing(g,stripes,72.f,72.f)==bitmap,"DPI bitmap was not reused");
#if JUCE_WINDOWS
        check(cache.forDrawing(g,stripes,72.f,72.f)==bitmap,"Windows drawing bypassed filtering");
#else
        check(cache.forDrawing(g,stripes,72.f,72.f)==stripes,"Changed native macOS artwork rendering");
#endif
    }
    std::cout<<"PASS: software minification, transparent edges, bitmap reuse/replacement and 16 editor/DPI combinations\n";
}
}
int runChecks()
{
 try{
    imageScaling();
    const auto base=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName()));
    check(base.createDirectory().wasOk(),"Create test data directory");
    const auto root=base.getNonexistentChildFile("skin-packages",{},false);
    const auto created=root.createDirectory();check(created.wasOk(),"Create temporary root: "+created.getErrorMessage());
    struct Cleanup{juce::File root;~Cleanup(){root.deleteRecursively();}} cleanup{root};
    juce::File manifest,other;juce::String error;
    check(SkinPackage::exportTemplate(root,manifest,error),error);
    check(SkinPackage::exportTemplate(root,other,error) && other!=manifest,"Export overwrote the existing template");
    SkinPackage loaded;check(SkinPackage::load(manifest,loaded,error),error);
    check(loaded.images.size()>=5 && loaded.layout["width"]==juce::var(1280),"Template round trip");
    check(SkinPackage::discover(root).size()==2,"Package discovery");
    auto layout=loaded.layout.clone(),theme=juce::JSON::parse(manifest.getSiblingFile("theme.json"));
    put(layout,"width",1600);put(layout,"height",1200);put(layout,"drawPanels",false);put(layout,"performanceCanvas",juce::var(juce::Array<juce::var>{1500,1050}));
    auto components=layout["components"];
    for(const auto& item:std::initializer_list<std::pair<const char*,juce::var>>{{"mainPanel/control:cutoff",box(40,200,100,110)},{"patchBrowser",box(20,120,1400,950)},{"patchSearch",box(20,60,250,30)},{"performance:part1:strip",box(30,80,400,900)},{"performance:part1:volume",box(40,600,100,250)}}){auto v=object();put(v,"bounds",item.second);components.getDynamicObject()->setProperty(item.first,v);}
    put(layout,"components",components);
    auto decoration=object();put(decoration,"id","badge");put(decoration,"parent","mainPanel");put(decoration,"kind","text");put(decoration,"rect",box(20,90,400,30));put(decoration,"text","External skin");
    put(layout,"decorations",juce::var(juce::Array<juce::var>{decoration}));
    theme["palette"].getDynamicObject()->setProperty("text","#112233");
    theme["font"].getDynamicObject()->setProperty("scale",1.2);
    auto captionStyle=object(),captionPalette=object();put(captionPalette,"text","#ffdd88");put(captionStyle,"palette",captionPalette);theme["components"].getDynamicObject()->setProperty("caption:cutoff",captionStyle);
    const auto stripFile=manifest.getParentDirectory().getChildFile("assets/strip.png");
    {juce::Image strip(juce::Image::ARGB,8,16,true);juce::Graphics g(strip);g.setColour(juce::Colours::red);g.fillRect(0,0,8,8);g.setColour(juce::Colours::green);g.fillRect(0,8,8,8);auto stream=stripFile.createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(strip,*stream),"Write filmstrip fixture");}
    theme["images"].getDynamicObject()->setProperty("strip","assets/strip.png");auto strip=object();put(strip,"image","strip");put(strip,"frames",2);put(strip,"orientation","vertical");put(theme,"knobStrip",strip);
    auto background=manifest.getParentDirectory().getChildFile("assets/background.png");image(background,juce::Colour(0xffdd22aa));
    theme["images"].getDynamicObject()->setProperty("background","assets/background.png");
    theme["images"].getDynamicObject()->setProperty("faderThumb","assets/background.png");
    auto surface=object();put(surface,"backgroundImage","background");theme["components"].getDynamicObject()->setProperty("surface",surface);
    const auto font=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_TEST_FONT",{}));
    if(font.existsAsFile()){check(font.copyFileTo(manifest.getParentDirectory().getChildFile("custom.ttf")),"Copy font fixture");theme["font"].getDynamicObject()->setProperty("file","custom.ttf");}
    json(manifest.getSiblingFile("layout.json"),layout);json(manifest.getSiblingFile("theme.json"),theme);
    Processor p;check(p.setDataFolder(root,error),error);p.skinChoice="Auto";p.editorScale.store(.75);
    juce::Component host;host.setComponentID("hostShell");
    Editor editor(p);host.addAndMakeVisible(editor);check(dynamic_cast<juce::Label*>(find(editor,"setupHelp"))->getFont()==juce::Font(15.f),"Built-in typography changed");auto* parameter=p.uiParameter("cutoff");check(parameter!=nullptr,"Cutoff parameter");parameter->setValueNotifyingHost(.4f);const auto value=parameter->getValue();
    check(editor.loadSkin(manifest,error),error);
    check(editor.getWidth()==1200 && editor.getHeight()==900,"Arbitrary canvas/scale");
    check(find(editor,"control:cutoff")->getBounds()==juce::Rectangle<int>(40,200,100,110),"Main control layout override");
    check(find(editor,"patchSearch")->getBounds()==juce::Rectangle<int>(20,60,250,30),"Browser layout override");
    check(find(editor,"performance:part1:strip")->getBounds()==juce::Rectangle<int>(30,80,400,900),"Performance strip layout override");
    check(find(editor,"performance:part1:volume")->getBounds()==juce::Rectangle<int>(40,600,100,250),"Performance control layout override");
    check(find(editor,"decoration:badge")!=nullptr,"Custom decoration");
    auto* caption=dynamic_cast<juce::Label*>(find(editor,"caption:cutoff"));check(caption && caption->findColour(juce::Label::textColourId)==juce::Colour(0xffffdd88),"Per-component text palette");
    check(std::abs(caption->getFont().getHeight()-13.8f)<.1f,"External font scale applied more than once");
    auto* knob=dynamic_cast<juce::Slider*>(find(editor,"cutoff"));check(knob!=nullptr,"Custom skin knob");
    knob->setValue(knob->getMinimum(),juce::dontSendNotification);auto first=knob->createComponentSnapshot(knob->getLocalBounds());
    knob->setValue(knob->getMaximum(),juce::dontSendNotification);auto last=knob->createComponentSnapshot(knob->getLocalBounds());
    check(first.getPixelAt(first.getWidth()/2,first.getHeight()/2)==juce::Colours::red && last.getPixelAt(last.getWidth()/2,last.getHeight()/2)==juce::Colours::green,"Filmstrip frames did not follow the knob");
    auto* fader=dynamic_cast<juce::Slider*>(find(editor,"performance:part1:volume:slider"));check(fader!=nullptr,"Custom fader");
    const auto countPink=[](const juce::Image& bitmap,bool top){int count=0;for(int y=top?0:bitmap.getHeight()/2;y<(top?bitmap.getHeight()/2:bitmap.getHeight());++y)for(int x=0;x<bitmap.getWidth();++x){const auto c=bitmap.getPixelAt(x,y);count+=c.getRed()>180&&c.getBlue()>100&&c.getGreen()<80;}return count;};
    fader->setValue(fader->getMinimum(),juce::dontSendNotification);auto down=fader->createComponentSnapshot(fader->getLocalBounds());
    fader->setValue(fader->getMaximum(),juce::dontSendNotification);auto up=fader->createComponentSnapshot(fader->getLocalBounds());
    check(countPink(down,false)>20&&countPink(up,true)>20,"External fader thumb did not follow its value");
    if(font.existsAsFile()){SkinPackage withFont;check(SkinPackage::load(manifest,withFont,error)&&withFont.typeface!=nullptr,"External typeface");}
    auto snapshot=editor.createComponentSnapshot(editor.getLocalBounds());check(snapshot.getPixelAt(0,0)==juce::Colour(0xffdd22aa),"External background image");
    check(parameter->getValue()==value,"Skin load changed a sound parameter");
    click(editor,"pagePatches");image(background,juce::Colour(0xff22aa66));
    SkinPackage refreshed;check(SkinPackage::load(manifest,refreshed,error),error);check(refreshed.images["background"].getPixelAt(0,0)==juce::Colour(0xff22aa66),"Loader retained an image");
    const auto* oldSurface=find(editor,"surface");
    check(editor.reloadSkin(error),error);
    auto* report=dynamic_cast<juce::Label*>(find(editor,"setupStatus"));
    check(find(editor,"surface")!=oldSurface,"Reload did not replace the surface: "+(report?report->getText():juce::String{}));
    check(find(editor,"patchBrowser")->isVisible(),"Reload lost the active page");
    snapshot=editor.createComponentSnapshot(editor.getLocalBounds());check(snapshot.getPixelAt(0,0)==juce::Colour(0xff22aa66),"Reload retained a cached image");
    check(parameter->getValue()==value,"Reload changed a sound parameter");
    const auto choice=p.skinChoice,path=p.customSkin;const auto* existing=find(editor,"surface");
    auto broken=layout.clone();put(broken,"effects",object());json(manifest.getSiblingFile("layout.json"),broken);
    check(!editor.reloadSkin(error)&&error.isNotEmpty(),"Malformed layout accepted");
    check(find(editor,"surface")==existing && p.skinChoice==choice && p.customSkin==path && parameter->getValue()==value,"Rejected reload modified the working editor/state");
    json(manifest.getSiblingFile("layout.json"),layout);
    auto badTheme=theme.clone();badTheme["images"].getDynamicObject()->setProperty("background","../outside.png");json(manifest.getSiblingFile("theme.json"),badTheme);
    check(!SkinPackage::load(manifest,loaded,error),"Package path traversal accepted");
    badTheme=theme.clone();badTheme["palette"].getDynamicObject()->setProperty("text","wrong");json(manifest.getSiblingFile("theme.json"),badTheme);
    check(!SkinPackage::load(manifest,loaded,error),"Invalid colour accepted");
    badTheme=theme.clone();put(badTheme,"renderStyle","unknown");json(manifest.getSiblingFile("theme.json"),badTheme);
    check(!SkinPackage::load(manifest,loaded,error),"Unknown rendering style accepted");
    json(manifest.getSiblingFile("theme.json"),theme);
    broken=layout.clone();put(broken,"performanceCanvas",juce::var(juce::Array<juce::var>{0,0}));json(manifest.getSiblingFile("layout.json"),broken);
    check(!SkinPackage::load(manifest,loaded,error),"Zero performance canvas accepted");
    broken=layout.clone();put(broken,"components",juce::var(juce::Array<juce::var>{1,2}));json(manifest.getSiblingFile("layout.json"),broken);
    check(!SkinPackage::load(manifest,loaded,error),"Array accepted as a component map");
    json(manifest.getSiblingFile("layout.json"),layout);
    juce::MemoryBlock saved;p.getStateInformation(saved);Processor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));Editor reopened(restored);
    check(restored.skinChoice==choice && reopened.getWidth()==1200 && find(reopened,"control:cutoff")->getX()==40,"Skin choice/layout lost in host state");
    check(editor.loadSkin(other,error),error);check(editor.getWidth()==960,"Switching packages did not restore built-in dimensions");
    {Processor missing;missing.skinChoice="Package:missing";missing.customSkin=root.getChildFile("absent/skin.json").getFullPathName();missing.editorScale.store(.75);Editor fallback(missing);
     check(fallback.getWidth()==960 && dynamic_cast<juce::Label*>(find(fallback,"setupStatus"))->getText().startsWith("Skin:"),"Missing startup skin did not show a usable fallback");}
    const auto capture=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});
    if(capture.isNotEmpty()){const auto dir=juce::File(capture);dir.createDirectory();click(reopened,"pageSetup");::capture(reopened,dir.getChildFile("Skin-Setup.png"));}
    const auto alternate=juce::SystemStats::getEnvironmentVariable("SILVER_K_TEST_SKIN",{});
    if(alternate.isNotEmpty()){
        check(editor.loadSkin(juce::File(alternate),error),error);click(editor,"pageMain");
        check(skinIsFlat(*find(editor,"surface")),"Flat package did not enable vector rendering");
        auto* dial=dynamic_cast<juce::Slider*>(find(editor,"cutoff"));
        check(dial && dial->getTextBoxPosition()==juce::Slider::TextBoxBelow,"Flat dial has no persistent value field");
        check(find(editor,"control:cutoff")->getBounds()==juce::Rectangle<int>(495,378,82,104),"Flat layout did not move the filter");
        check(std::abs(parameter->getValue()-value)<.0001f,"Flat skin changed a parameter");
        for(const auto& domain:juce::StringArray{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx"}){
            auto* action=find(editor,"extra:"+domain);if(!action || !action->isVisible())continue;
            click(editor,("extra:"+domain).toRawUTF8());
            auto* extra=find(editor,"localExtra");auto* content=find(editor,"extraContent");
            check(extra && content && extra->isVisible(),"Flat Extra did not open: "+domain);
            for(auto* child:content->getChildren())check(content->getLocalBounds().contains(child->getBounds()),"Flat Extra clips a control: "+domain+" / "+child->getComponentID());
            if(domain=="voice"){
                check(find(editor,"programClock")->isVisible(),"Flat Options lost the clock selector");
                int values=0;for(auto* child:content->getChildren())for(auto* field:child->getChildren())
                    if(auto* dial=dynamic_cast<juce::Slider*>(field))if(dial->getTextBoxPosition()==juce::Slider::TextBoxRight)++values;
                check(values>0,"Flat Options has no persistent values for mini dials");
            }
            if(capture.isNotEmpty() && domain=="voice"){
                ::capture(editor,juce::File(capture).getChildFile("Silver-Flat-Options.png"));
            }
            click(editor,"extraBack");
        }
        for(const auto& page:juce::StringArray{"Main","Extra","Patches","Perf","Setup"}){
            click(editor,("page"+page).toRawUTF8());
            if(page=="Main")find(editor,"mainPanel")->setEnabled(true);
            if(capture.isNotEmpty()){
                ::capture(editor,juce::File(capture).getChildFile("Silver-Flat-"+page+".png"));
            }
        }
        check(editor.reloadSkin(error),error);check(skinIsFlat(*find(editor,"surface")),"Reload lost flat rendering");
        check(editor.loadSkin(other,error),error);check(!skinIsFlat(*find(editor,"surface")),"Switching to classic retained flat rendering");
        check(std::abs(parameter->getValue()-value)<.0001f,"Skin round trip changed a parameter");
        std::cout<<"PASS: Silver Flat load, persistent values, alternate layout, all pages, Reload and classic round trip\n";
    }
    std::cout<<"PASS: package export/discovery, external artwork/font/palette, arbitrary main/browser/performance layout, Reload, rollback, resource validation and host state\n";
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}

class SkinTestApplication final:public juce::JUCEApplication
{
    std::unique_ptr<Processor> processor;
    std::unique_ptr<Editor> editor;
    juce::File root;
    juce::Component* oldSurface=nullptr;
    bool queueProbe=false;int attempts=0;
    void fail(const juce::String& error){std::cerr<<error<<'\n';juce::PopupMenu::dismissAllActiveMenus();setApplicationReturnValue(1);quit();}
public:
    const juce::String getApplicationName()override{return "Silver-K skin checks";}
    const juce::String getApplicationVersion()override{return "1";}
    bool moreThanOneInstanceAllowed()override{return true;}
    void initialise(const juce::String&)override{
        juce::Timer::callAfterDelay(1,[this]{
            if(runChecks()!=0){setApplicationReturnValue(1);quit();return;}
            try{
                const auto base=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getFullPathName()));
                root=base.getChildFile("skin-ui-"+juce::Uuid().toString());juce::String error;
                processor=std::make_unique<Processor>();check(processor->setDataFolder(root,error),error);processor->skinChoice="Auto";
                editor=std::make_unique<Editor>(*processor);processor->uiParameter("cutoff")->setValueNotifyingHost(.3f);
                click(*editor,"exportSkin");check(SkinPackage::discover(processor->skinsFolder()).size()==1,"Export button did not publish a package");
                auto* selector=dynamic_cast<juce::ComboBox*>(find(*editor,"skinSelect"));check(selector&&selector->getNumItems()==5,"Export did not refresh the package list");
                selector->setSelectedId(100,juce::dontSendNotification);check(selector->getSelectedId()==100,"ComboBox lost the selected package ID");selector->onChange();
                check(juce::MessageManager::callAsync([this]{queueProbe=true;}),"Cannot post to the message queue");
                // Yield to the real application loop: skin changes must happen
                // after the source button/menu callback has returned.
                juce::Timer::callAfterDelay(100,[this]{afterSelection();});
            }catch(const std::exception& e){fail(e.what());}
        });
    }
    void afterSelection(){try{
        if(!processor->skinChoice.startsWith("Package:") && attempts++<100){juce::Timer::callAfterDelay(25,[this]{afterSelection();});return;}
        check(queueProbe,"Application queue did not dispatch the probe");
        check(processor->skinChoice.startsWith("Package:"),"Selecting a discovered package did not activate it: "+dynamic_cast<juce::Label*>(find(*editor,"setupStatus"))->getText());
        oldSurface=find(*editor,"surface");attempts=0;click(*editor,"reloadSkin");
        juce::Timer::callAfterDelay(100,[this]{afterReload();});
    }catch(const std::exception& e){fail(e.what());}}
    void afterReload(){try{
        if(find(*editor,"surface")==oldSurface && attempts++<100){juce::Timer::callAfterDelay(25,[this]{afterReload();});return;}
        check(find(*editor,"surface")!=oldSurface,"Reload button did not rebuild the editor");
        check(std::abs(processor->uiParameter("cutoff")->getValue()-.3f)<.001f,"UI skin switch changed the sound parameter");
        std::cout<<"PASS: real message-loop Export, catalog selection and deferred Reload\n";
        const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        check(display!=nullptr,"Resize-menu checks require a graphical desktop");
        const auto area=display->userArea;
        editor->setTopLeftPosition(area.getPosition()+juce::Point<int>(100,80));
        checkSizeMenu(0);
    }catch(const std::exception& e){fail(e.what());}}
    void checkSizeMenu(int index){try{
        editor->setUiScale(index==0?.5:index==1?.75:1.);
        auto* source=find(*editor,"extra:osc1");check(source!=nullptr,"Resize click source");
        const auto now=juce::Time::getCurrentTime();
        const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),{5,5},juce::ModifierKeys::rightButtonModifier,1,0,0,0,0,source,source,now,{5,5},now,1,false);
        const auto position=event.getScreenPosition();
        editor->mouseDown(event);
        auto* popup=juce::Component::getCurrentlyModalComponent();check(popup && popup->getName()=="menu","Resize popup missing at scale index "+juce::String(index));
        const auto bounds=popup->getScreenBounds();
        check(std::abs(bounds.getX()-position.x)<32 && std::abs(bounds.getY()-position.y)<32,"Resize popup is detached from the right-click: "+bounds.toString()+" vs "+position.toString());
        popup->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
        popup->keyPressed(juce::KeyPress(juce::KeyPress::rightKey));
        auto* submenu=juce::Component::getCurrentlyModalComponent();check(submenu && submenu!=popup,"Resize submenu missing");
        bool custom=false;
        for(auto* item:submenu->getChildren())if(auto* handler=item->getAccessibilityHandler())
            custom|=handler->getTitle()==juce::String("Custom")+juce::String::charToString(0x2026);
        check(custom,"Custom menu item contains incorrectly decoded Unicode");
        const auto capture=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});
        if(capture.isNotEmpty())::capture(*submenu,juce::File(capture).getChildFile("Resize-Menu-"+juce::String(index)+".png"));
        // Continue after JUCE dispatches the menu's completion callbacks, which
        // clear the editor's open-menu guard and may create the size dialog.
        juce::ModalComponentManager::getInstance()->attachCallback(popup,juce::ModalCallbackFunction::create([this,index](int){
            juce::MessageManager::callAsync([this,index]{if(index<2)checkSizeMenu(index+1);else checkSizeDialog();});
        }));
        if(index<2){
            juce::PopupMenu::dismissAllActiveMenus();
        }else{
            // Three presets, a separator, then Custom. Activate the actual item
            // so the asynchronous menu callback and size dialog are exercised.
            for(int n=0;n<3;++n)submenu->keyPressed(juce::KeyPress(juce::KeyPress::downKey));
            submenu->keyPressed(juce::KeyPress(juce::KeyPress::returnKey));
        }
    }catch(const std::exception& e){fail(e.what());}}
    void checkSizeDialog(){try{
        auto* dialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());check(dialog!=nullptr,"Custom resize dialog missing");
        check(dialog->getDescription()==juce::String("Plugin size. Scale: 50")+juce::String::charToString(0x2013)+"150%","Size range contains incorrectly decoded Unicode");
        auto* program=dynamic_cast<juce::TextButton*>(find(*editor,"extra:voice"));
        check(program && program->getTooltip()==juce::String("Program ")+juce::String::charToString(0x2014)+" Options","Options tooltip contains incorrectly decoded Unicode");
        juce::ModalComponentManager::getInstance()->attachCallback(dialog,juce::ModalCallbackFunction::create([this](int){
            juce::MessageManager::callAsync([this]{try{
                check(editor->getWidth()==1050 && editor->getHeight()==820,"Custom resize did not apply through the menu/dialog callbacks");
                std::cout<<"PASS: right-click popup placement at 50/75/100%, Unicode menu/dialog/tooltips and Custom resize\n";setApplicationReturnValue(0);quit();
            }catch(const std::exception& e){fail(e.what());}});
        }));
        dialog->getTextEditor("percent")->setText("82");dialog->exitModalState(1);
    }catch(const std::exception& e){fail(e.what());}}
    void shutdown()override{editor.reset();processor.reset();if(root!=juce::File{})root.deleteRecursively();}
};
int main(int argc,char** argv)
{
    std::cout<<std::unitbuf;std::cerr<<std::unitbuf;
    juce::JUCEApplicationBase::createInstance=[]()->juce::JUCEApplicationBase*{return new SkinTestApplication;};
#if JUCE_WINDOWS
    return juce::JUCEApplicationBase::main();
#else
    std::vector<const char*> arguments(argv,argv+argc);arguments.push_back(nullptr);return juce::JUCEApplicationBase::main(argc,arguments.data());
#endif
}
