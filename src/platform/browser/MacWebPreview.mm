#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "platform/browser/MacWebPreview.hpp"
#include <utility>

using lucid::modules::WebPreviewAssets;
using lucid::platform::MacWebPreview;
using lucid::platform::MacWebPreviewResult;

// This native preview is intentionally not presented as a sandbox. Scheme and
// navigation restrictions reduce accidental network use but do not establish
// a security boundary around arbitrary browser content.
@interface LucidWebPreviewSession : NSObject <WKURLSchemeHandler, WKNavigationDelegate, WKUIDelegate> {
@public
    WebPreviewAssets assets_;
    MacWebPreview::Completion completion_;
    WKWebView* view_;
    NSWindow* window_;
    BOOL finished_;
    WKNavigation* navigation_;
    NSUInteger navigationGeneration_;
}
- (void)stop;
- (void)finish:(MacWebPreviewResult)result;
@end

@implementation LucidWebPreviewSession
- (void)stop {
    finished_ = YES;
    ++navigationGeneration_;
    navigation_ = nil;
    completion_ = {};
    [view_ stopLoading];
    view_.navigationDelegate = nil;
    view_.UIDelegate = nil;
    [window_ orderOut:nil];
    window_.contentView = nil;
    view_ = nil;
    window_ = nil;
}
- (void)finish:(MacWebPreviewResult)result {
    if (finished_) return;
    auto completion = std::move(completion_);
    [self stop];
    if (completion) completion(std::move(result));
}
- (void)webView:(WKWebView*)webView startURLSchemeTask:(id<WKURLSchemeTask>)task {
    (void)webView;
    NSURL* url = task.request.URL;
    NSString* absolute = url.absoluteString;
    NSString* prefix = @"lucid-preview://preview.local";
    if (!absolute || ![absolute hasPrefix:prefix] || url.user.length || url.password.length || url.port ||
        ![url.host.lowercaseString isEqualToString:@"preview.local"]) {
        [task didFailWithError:[NSError errorWithDomain:@"LucidPreview" code:1 userInfo:nil]]; return;
    }
    NSString* suffix = [absolute substringFromIndex:prefix.length];
    if (![suffix hasPrefix:@"/"]) { [task didFailWithError:[NSError errorWithDomain:@"LucidPreview" code:2 userInfo:nil]]; return; }
    NSString* noFragment = [[suffix componentsSeparatedByString:@"#"] firstObject];
    const char* method = task.request.HTTPMethod.UTF8String;
    const char* path = noFragment.UTF8String;
    auto response = assets_.respond(method ? method : "", path ? path : "");
    NSMutableDictionary<NSString*, NSString*>* headers = [NSMutableDictionary dictionary];
    for (const auto& [key, value] : response.headers) {
        NSString* k = [NSString stringWithUTF8String:key.c_str()];
        NSString* v = [NSString stringWithUTF8String:value.c_str()];
        if (k && v) headers[k] = v;
    }
    if (!response.contentType.empty()) headers[@"Content-Type"] = [NSString stringWithUTF8String:response.contentType.c_str()];
    NSHTTPURLResponse* http = [[NSHTTPURLResponse alloc] initWithURL:url statusCode:response.status HTTPVersion:@"HTTP/1.1" headerFields:headers];
    if (!http) { [task didFailWithError:[NSError errorWithDomain:@"LucidPreview" code:3 userInfo:nil]]; return; }
    [task didReceiveResponse:http];
    if (!response.body.empty()) [task didReceiveData:[NSData dataWithBytes:response.body.data() length:response.body.size()]];
    [task didFinish];
}
- (void)webView:(WKWebView*)webView stopURLSchemeTask:(id<WKURLSchemeTask>)task { (void)webView; (void)task; }
- (void)webView:(WKWebView*)webView decidePolicyForNavigationAction:(WKNavigationAction*)action decisionHandler:(void (^)(WKNavigationActionPolicy))decision {
    (void)webView;
    NSURL* url = action.request.URL;
    BOOL allowed = [url.scheme.lowercaseString isEqualToString:@"lucid-preview"] &&
        [url.host.lowercaseString isEqualToString:@"preview.local"] && !url.user.length && !url.password.length && !url.port;
    decision(allowed ? WKNavigationActionPolicyAllow : WKNavigationActionPolicyCancel);
}
- (WKWebView*)webView:(WKWebView*)webView createWebViewWithConfiguration:(WKWebViewConfiguration*)configuration forNavigationAction:(WKNavigationAction*)action windowFeatures:(WKWindowFeatures*)features {
    (void)webView; (void)configuration; (void)action; (void)features; return nil;
}
- (void)webView:(WKWebView*)webView didStartProvisionalNavigation:(WKNavigation*)navigation {
    (void)webView;
    if (finished_) return;
    navigation_ = navigation;
    ++navigationGeneration_;
}
- (void)webView:(WKWebView*)webView didFinishNavigation:(WKNavigation*)navigation {
    if (finished_ || navigation != navigation_) return;
    const NSUInteger generation = navigationGeneration_;
    // Give WebKit a rendering turn and allow immediate script-driven navigation
    // to begin before capturing. This is not a guarantee that asynchronous page
    // work has completed; callers must treat the snapshot as a point in time.
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
        if (self->finished_ || self->navigationGeneration_ != generation) return;
        [webView takeSnapshotWithConfiguration:nil completionHandler:^(NSImage* image, NSError* error) {
            if (self->finished_ || self->navigationGeneration_ != generation) return;
            if (error || !image) { [self finish:{ {}, "Could not capture web preview" }]; return; }
            NSData* tiff = image.TIFFRepresentation;
            NSBitmapImageRep* bitmap = tiff ? [NSBitmapImageRep imageRepWithData:tiff] : nil;
            NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
            if (!png || png.length > 16U * 1024U * 1024U) { [self finish:{ {}, "Could not encode preview PNG" }]; return; }
            [self finish:{std::string(static_cast<const char*>(png.bytes), png.length), {}}];
        }];
    });
}
- (void)webView:(WKWebView*)webView didFailNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView; (void)error; if (navigation == navigation_) [self finish:{{}, "Web preview navigation failed"}];
}
- (void)webView:(WKWebView*)webView didFailProvisionalNavigation:(WKNavigation*)navigation withError:(NSError*)error {
    (void)webView; (void)error; if (navigation == navigation_) [self finish:{{}, "Web preview navigation failed"}];
}
- (void)webViewWebContentProcessDidTerminate:(WKWebView*)webView { (void)webView; [self finish:{{}, "Web preview process terminated"}]; }
@end

namespace lucid::platform {
struct MacWebPreview::Impl { __strong LucidWebPreviewSession* session = nil; };
MacWebPreview::MacWebPreview() : impl_(std::make_unique<Impl>()) {}
MacWebPreview::~MacWebPreview() { cancel(); }
void MacWebPreview::cancel() { [impl_->session stop]; impl_->session = nil; }
bool MacWebPreview::start(modules::WebPreviewAssets assets, std::string target, unsigned width,
                          unsigned height, Completion completion, std::string& error) {
    error.clear();
    if (![NSThread isMainThread] || !NSApp) { error = "Web preview requires the macOS main thread and NSApplication"; return false; }
    if (!completion || width < 128 || width > 2048 || height < 128 || height > 2048) { error = "Invalid preview size or callback"; return false; }
    auto initial = assets.respond("GET", std::string_view(target).substr(0, target.find('#')));
    if (initial.status != 200 || !initial.contentType.starts_with("text/html")) { error = "Preview entry must be an available HTML asset"; return false; }
    NSString* path = [NSString stringWithUTF8String:target.c_str()];
    NSURL* url = path ? [NSURL URLWithString:[@"lucid-preview://preview.local" stringByAppendingString:path]] : nil;
    if (!url) { error = "Invalid preview URL"; return false; }
    cancel();
    LucidWebPreviewSession* session = [[LucidWebPreviewSession alloc] init];
    session->assets_ = std::move(assets); session->completion_ = std::move(completion);
    WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
    config.websiteDataStore = WKWebsiteDataStore.nonPersistentDataStore;
    [config setURLSchemeHandler:session forURLScheme:@"lucid-preview"];
    NSRect frame = NSMakeRect(0, 0, width, height);
    session->window_ = [[NSWindow alloc] initWithContentRect:frame styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    session->view_ = [[WKWebView alloc] initWithFrame:frame configuration:config];
    session->view_.navigationDelegate = session; session->view_.UIDelegate = session;
    session->window_.contentView = session->view_;
    impl_->session = session;
    [session->view_ loadRequest:[NSURLRequest requestWithURL:url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:15]];
    __weak LucidWebPreviewSession* weakSession = session;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 20 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
        LucidWebPreviewSession* active = weakSession;
        if (active && !active->finished_) [active finish:{{}, "Web preview timed out"}];
    });
    return true;
}
}
