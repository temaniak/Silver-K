#include "silverSoulTrain.h"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface NKSilverSoulTrainHandler : NSObject<WKScriptMessageHandler,WKURLSchemeHandler,WKNavigationDelegate>
@property(nonatomic,assign) silverK::SoulTrainWeb* owner;
@property(nonatomic,strong) NSString* resourcePath;
@end
@implementation NKSilverSoulTrainHandler
-(void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
 if(!self.owner || !message.frameInfo.mainFrame || ![message.frameInfo.request.URL.scheme isEqualToString:@"silver-soultrain"])return;
 if(![NSJSONSerialization isValidJSONObject:message.body])return;
 NSData* data=[NSJSONSerialization dataWithJSONObject:message.body options:0 error:nil];
 self.owner->receive(juce::JSON::parse(juce::String::fromUTF8((const char*)data.bytes,int(data.length))));
}
-(void)webView:(WKWebView*)view startURLSchemeTask:(id<WKURLSchemeTask>)task {
 const auto root=juce::File(juce::String::fromUTF8(self.resourcePath.UTF8String));
 juce::String relative=juce::String::fromUTF8(task.request.URL.path.UTF8String);while(relative.startsWithChar('/'))relative=relative.substring(1);
 if(relative.isEmpty())relative="index.html";
 const auto file=root.getChildFile(relative);
 if(!file.isAChildOf(root) || !file.existsAsFile()) {[task didFailWithError:[NSError errorWithDomain:@"SilverSoulTrain" code:404 userInfo:nil]];return;}
 NSString* mime=@"application/octet-stream";const auto ext=file.getFileExtension().toLowerCase();
 if(ext==".html")mime=@"text/html";else if(ext==".js"||ext==".mjs")mime=@"text/javascript";else if(ext==".css")mime=@"text/css";else if(ext==".json")mime=@"application/json";else if(ext==".svg")mime=@"image/svg+xml";else if(ext==".png")mime=@"image/png";else if(ext==".ttf")mime=@"font/ttf";
 NSData* data=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()]];
 const bool text=ext==".html"||ext==".js"||ext==".mjs"||ext==".css"||ext==".json"||ext==".svg";
 NSString* type=text?[mime stringByAppendingString:@"; charset=utf-8"]:mime;
 [task didReceiveResponse:[[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type":type}]];[task didReceiveData:data];[task didFinish];
}
-(void)webView:(WKWebView*)view stopURLSchemeTask:(id<WKURLSchemeTask>)task {}
-(void)webView:(WKWebView*)view didFinishNavigation:(WKNavigation*)navigation {if(self.owner)self.owner->loaded();}
-(void)webView:(WKWebView*)view decidePolicyForNavigationAction:(WKNavigationAction*)action decisionHandler:(void (^)(WKNavigationActionPolicy))decision {
 decision([action.request.URL.scheme isEqualToString:@"silver-soultrain"]?WKNavigationActionPolicyAllow:WKNavigationActionPolicyCancel);
}
@end
namespace silverK {
struct SoulTrainWeb::Impl {
 juce::NSViewComponent component;
 NKSilverSoulTrainHandler* __strong handler;
 WKWebView* __strong view;
 Impl(SoulTrainWeb& owner,juce::File root) {
  handler=[NKSilverSoulTrainHandler new];handler.owner=&owner;
  if(root==juce::File{}){NSBundle* bundle=[NSBundle bundleForClass:[NKSilverSoulTrainHandler class]];root=juce::File(juce::String::fromUTF8(bundle.resourcePath.UTF8String)).getChildFile("SoulTrain");}
  handler.resourcePath=[NSString stringWithUTF8String:root.getFullPathName().toRawUTF8()];
  WKWebViewConfiguration* configuration=[WKWebViewConfiguration new];
  configuration.websiteDataStore=[WKWebsiteDataStore nonPersistentDataStore];
  [configuration setURLSchemeHandler:handler forURLScheme:@"silver-soultrain"];
  [configuration.userContentController addScriptMessageHandler:handler name:@"silver"];
  NSString* script=@"window.__SILVER_NATIVE__=true;window.silverSend=(data)=>window.webkit.messageHandlers.silver.postMessage(data);";
  [configuration.userContentController addUserScript:[[WKUserScript alloc] initWithSource:script injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
  view=[[WKWebView alloc] initWithFrame:NSMakeRect(0,0,1486,942) configuration:configuration];view.navigationDelegate=handler;
  [view setValue:@NO forKey:@"drawsBackground"];
  component.setView((__bridge void*)view);owner.addAndMakeVisible(component);
  [view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"silver-soultrain://ui/index.html"]]];
 }
 ~Impl(){handler.owner=nullptr;view.navigationDelegate=nil;[view stopLoading];[view.configuration.userContentController removeScriptMessageHandlerForName:@"silver"];component.setView(nullptr);}
};
SoulTrainWeb::SoulTrainWeb(Processor& p,Callbacks cb,juce::File root):processor(p),callbacks(std::move(cb)){setComponentID("soulTrainWeb");impl=std::make_unique<Impl>(*this,root);startTimerHz(20);}
SoulTrainWeb::~SoulTrainWeb(){closeBridge();impl.reset();}
void SoulTrainWeb::resized(){impl->component.setBounds(getLocalBounds());}
void SoulTrainWeb::evaluate(const juce::String& code,std::function<void(juce::var)> completion){
 [impl->view evaluateJavaScript:[NSString stringWithUTF8String:code.toRawUTF8()] completionHandler:^(id value,NSError* failure){if(!completion)return;if(failure){completion({});return;}if([NSJSONSerialization isValidJSONObject:value]){NSData* data=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingFragmentsAllowed error:nil];completion(juce::JSON::parse(juce::String::fromUTF8((const char*)data.bytes,int(data.length))));}else if([value isKindOfClass:[NSString class]])completion(juce::String::fromUTF8([(NSString*)value UTF8String]));else if([value isKindOfClass:[NSNumber class]])completion([(NSNumber*)value doubleValue]);else completion({});}];
}
}
