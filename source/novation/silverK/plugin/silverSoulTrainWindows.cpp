#include "silverSoulTrain.h"
#include "silverSoulTrainResources.h"

#include <commctrl.h>
#include <wrl.h>
#include <WebView2.h>

namespace silverK {
namespace {
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

constexpr wchar_t hostName[]=L"silver-soultrain.invalid";
constexpr wchar_t entryUrl[]=L"https://silver-soultrain.invalid/index.html";
constexpr wchar_t bridgeScript[]=LR"(
if (window.top === window && window.location.origin === 'https://silver-soultrain.invalid') {
    window.__SILVER_NATIVE__ = true;
    window.silverSend = data => window.chrome.webview.postMessage(data);
}
)";
constexpr char moduleAnchor=0;

juce::File bundledResources(){
 return soulTrainResources(&moduleAnchor,"SoulTrain");
}

template<class Read> juce::String readText(Read read){
 LPWSTR text=nullptr;
 const auto result=read(&text);
 const std::unique_ptr<wchar_t,decltype(&CoTaskMemFree)> storage(text,&CoTaskMemFree);
 return SUCCEEDED(result) && text?juce::String(text):juce::String{};
}
}

struct SoulTrainWeb::Impl final : juce::HWNDComponent, private juce::Timer {
 SoulTrainWeb& owner;
 juce::File resourceRoot;
 ComPtr<ICoreWebView2Controller> controller;
 ComPtr<ICoreWebView2> webView;
 bool comInitialised=false, closing=false, failed=false;
 double startedAt=0;

 Impl(SoulTrainWeb& web,juce::File root)
  :owner(web),resourceRoot(root==juce::File{}?bundledResources():std::move(root)){
  startTimer(50);
 }

 ~Impl() override {
  closing=true;
  stopTimer();
  if(controller)controller->Close();
  webView.Reset();
  controller.Reset();
  if(const auto window=static_cast<HWND>(getHWND())){
   RemoveWindowSubclass(window,windowMessage,1);
   setHWND(nullptr);
  }
  if(comInitialised)CoUninitialize();
 }

 void fail(const juce::String& reason,HRESULT result=E_FAIL){
  if(failed || closing)return;
  failed=true;
  stopTimer();
  if(owner.callbacks.nativeAction)
   owner.callbacks.nativeAction("soulTrainUnavailable:"+reason+" (0x"
                                +juce::String::toHexString(juce::uint32(result))+")");
 }

 bool check(HRESULT result,const char* operation){
  if(SUCCEEDED(result))return true;
  fail(operation,result);
  return false;
 }

 void timerCallback() override {
  if(owner.ready){stopTimer();return;}
  if(startedAt!=0){
   if(juce::Time::getMillisecondCounterHiRes()-startedAt>15000)
    fail("SoulTrain did not start in WebView2");
   return;
  }
  if(!isShowing())return;
  if(auto* peer=getPeer()){
   startedAt=juce::Time::getMillisecondCounterHiRes();
   attach(static_cast<HWND>(peer->getNativeHandle()));
  }
 }

 static LRESULT CALLBACK windowMessage(HWND window,UINT message,WPARAM wParam,LPARAM lParam,
                                      UINT_PTR,DWORD_PTR ownerPointer){
  auto& self=*reinterpret_cast<Impl*>(ownerPointer);
  if(self.controller && !self.closing){
   if(message==WM_SIZE)self.updateBounds();
   if(message==WM_MOVE)self.controller->NotifyParentWindowPositionChanged();
  }
  return DefSubclassProc(window,message,wParam,lParam);
 }

 void updateBounds(){
  if(controller){
   RECT bounds{};
   if(GetClientRect(static_cast<HWND>(getHWND()),&bounds))controller->put_Bounds(bounds);
  }
 }

 void attach(HWND parent){
  if(!resourceRoot.getChildFile("index.html").existsAsFile()){
   fail("SoulTrain resources are missing from the build");
   return;
  }
  if(!check(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED),"Cannot initialise the WebView2 editor thread"))return;
  comInitialised=true;
  const auto window=CreateWindowExW(0,L"STATIC",nullptr,WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
                                    0,0,0,0,parent,nullptr,nullptr,nullptr);
  if(!window){fail("Cannot create the WebView2 editor window",HRESULT_FROM_WIN32(GetLastError()));return;}
  setHWND(window);
  if(!SetWindowSubclass(window,windowMessage,1,reinterpret_cast<DWORD_PTR>(this))){
   fail("Cannot configure the WebView2 editor window");
   return;
  }

  // VST3 can reside in a read-only system folder, so give the browser a user profile.
  const auto profile=owner.processor.dataFolder.getChildFile("SoulTrain-WebView2");
  const auto create=profile.createDirectory();
  if(create.failed()){fail("Cannot open the WebView2 profile: "+create.getErrorMessage());return;}
  const juce::Component::SafePointer<Impl> self(this);
  auto handler=Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
   [self](HRESULT result,ICoreWebView2Environment* environment)->HRESULT {
    if(self && !self->closing && !self->failed && self->check(result,"Install the Microsoft Edge WebView2 Runtime"))
     self->createController(environment);
    return S_OK;
   });
  check(CreateCoreWebView2EnvironmentWithOptions(nullptr,profile.getFullPathName().toWideCharPointer(),nullptr,handler.Get()),
        "Cannot start WebView2. Check the Microsoft Edge WebView2 Runtime");
 }

 void createController(ICoreWebView2Environment* environment){
  if(!environment){fail("WebView2 environment is unavailable");return;}
  const juce::Component::SafePointer<Impl> self(this);
  auto handler=Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
   [self](HRESULT result,ICoreWebView2Controller* incoming)->HRESULT {
    if(!self || self->closing || self->failed){if(incoming)incoming->Close();return S_OK;}
    if(self->check(result,"Cannot create the WebView2 controller"))self->initialiseView(incoming);
    return S_OK;
   });
  check(environment->CreateCoreWebView2Controller(static_cast<HWND>(getHWND()),handler.Get()),
        "Cannot attach the WebView2 controller");
 }

 void initialiseView(ICoreWebView2Controller* incoming){
  if(!incoming){fail("WebView2 controller is unavailable");return;}
  controller=incoming;
  if(!check(controller->get_CoreWebView2(&webView),"Cannot open the WebView2 editor"))return;
  ComPtr<ICoreWebView2_3> localResources;
  if(!check(webView.As(&localResources),"WebView2 Runtime is too old for SoulTrain"))return;
  if(!check(localResources->SetVirtualHostNameToFolderMapping(hostName,resourceRoot.getFullPathName().toWideCharPointer(),
                                                              COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY),
            "Cannot mount the bundled SoulTrain resources"))return;

  ComPtr<ICoreWebView2Settings> settings;
  if(!check(webView->get_Settings(&settings),"Cannot read WebView2 settings")
     || !check(settings->put_IsWebMessageEnabled(TRUE),"Cannot enable the SoulTrain bridge"))return;
  settings->put_AreDefaultContextMenusEnabled(FALSE);
  settings->put_AreDevToolsEnabled(FALSE);

  const juce::Component::SafePointer<Impl> self(this);
  EventRegistrationToken token{};
  if(!check(webView->add_NavigationStarting(Callback<ICoreWebView2NavigationStartingEventHandler>(
   [](ICoreWebView2*,ICoreWebView2NavigationStartingEventArgs* event)->HRESULT {
    const auto uri=readText([&](LPWSTR* value){return event->get_Uri(value);});
    return event->put_Cancel(uri!=juce::String(entryUrl));
   }).Get(),&token),"Cannot restrict SoulTrain navigation"))return;
  if(!check(webView->add_NewWindowRequested(Callback<ICoreWebView2NewWindowRequestedEventHandler>(
   [](ICoreWebView2*,ICoreWebView2NewWindowRequestedEventArgs* event)->HRESULT {
    return event->put_Handled(TRUE);
   }).Get(),&token),"Cannot restrict SoulTrain popups"))return;
  if(!check(webView->add_WebMessageReceived(Callback<ICoreWebView2WebMessageReceivedEventHandler>(
   [self](ICoreWebView2*,ICoreWebView2WebMessageReceivedEventArgs* event)->HRESULT {
    if(!self || self->closing || self->failed)return S_OK;
    const auto source=readText([&](LPWSTR* value){return event->get_Source(value);});
    if(source==juce::String(entryUrl)){
     const auto json=readText([&](LPWSTR* value){return event->get_WebMessageAsJson(value);});
     if(json.isNotEmpty())self->owner.receive(juce::JSON::parse(json));
    }
    return S_OK;
   }).Get(),&token),"Cannot receive SoulTrain signals"))return;
  if(!check(webView->add_NavigationCompleted(Callback<ICoreWebView2NavigationCompletedEventHandler>(
   [self](ICoreWebView2*,ICoreWebView2NavigationCompletedEventArgs* event)->HRESULT {
    if(!self || self->closing || self->failed)return S_OK;
    BOOL success=FALSE;
    if(SUCCEEDED(event->get_IsSuccess(&success)) && success)self->owner.loaded();
    else self->fail("Cannot load the bundled SoulTrain page");
    return S_OK;
   }).Get(),&token),"Cannot observe SoulTrain page loading"))return;

  updateBounds();
  controller->put_IsVisible(TRUE);
  auto handler=Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
   [self](HRESULT result,LPCWSTR)->HRESULT {
    if(self && !self->closing && !self->failed && self->check(result,"Cannot install the SoulTrain bridge"))
     self->check(self->webView->Navigate(entryUrl),"Cannot navigate to the bundled SoulTrain page");
    return S_OK;
   });
  check(webView->AddScriptToExecuteOnDocumentCreated(bridgeScript,handler.Get()),"Cannot install the SoulTrain bridge");
 }

 void evaluate(const juce::String& code,std::function<void(juce::var)> completion){
  if(!webView || failed || closing){if(completion)completion({});return;}
  // ExecuteScript returns JSON, including for scalar results.
  auto handler=Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
   [completion](HRESULT result,LPCWSTR json)->HRESULT {
    if(completion)completion(SUCCEEDED(result) && json?juce::JSON::fromString(juce::String(json)):juce::var{});
    return S_OK;
   });
  const auto result=webView->ExecuteScript(code.toWideCharPointer(),handler.Get());
  if(FAILED(result) && completion)completion({});
 }
};

SoulTrainWeb::SoulTrainWeb(Processor& p,Callbacks cb,juce::File root)
 :processor(p),callbacks(std::move(cb)){
 setComponentID("soulTrainWeb");
 impl=std::make_unique<Impl>(*this,std::move(root));
 addAndMakeVisible(*impl);
 startTimerHz(20);
}
SoulTrainWeb::~SoulTrainWeb(){closeBridge();impl.reset();}
void SoulTrainWeb::resized(){impl->setBounds(getLocalBounds());}
void SoulTrainWeb::evaluate(const juce::String& code,std::function<void(juce::var)> completion){
 impl->evaluate(code,std::move(completion));
}
}
