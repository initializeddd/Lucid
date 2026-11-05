#import <Cocoa/Cocoa.h>
#include "platform/browser/MacWebPreview.hpp"
#include <string>

int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        lucid::modules::WebPreviewAssets assets; std::string error;
        if (!assets.load({{"/index.html", "<!doctype html><html><body><h1>Lucid preview</h1></body></html>"}}, error)) return 1;
        lucid::platform::MacWebPreview preview;
        bool completed = false, valid = false;
        if (!preview.start(assets, "/index.html", 320, 240, [&](auto result) {
            completed = true;
            valid = result.error.empty() && result.png.size() > 8 && result.png.substr(0, 8) == std::string("\x89PNG\r\n\x1a\n", 8);
        }, error)) return 2;
        NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:15];
        while (!completed && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        if (!completed || !valid) return 3;
        bool cancelledCallback = false;
        if (!preview.start(assets, "/index.html", 320, 240, [&](auto) { cancelledCallback = true; }, error)) return 4;
        preview.cancel();
        [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.2]];
        if (cancelledCallback) return 5;
        if (preview.start(assets, "/missing.html", 320, 240, [](auto) {}, error) || error.empty()) return 6;
        if (preview.start(assets, "/index.html", 0, 240, [](auto) {}, error) || error.empty()) return 7;
        bool firstCalled = false, secondCalled = false, secondValid = false;
        if (!preview.start(assets, "/index.html", 320, 240, [&](auto) { firstCalled = true; }, error)) return 8;
        if (!preview.start(assets, "/index.html", 320, 240, [&](auto result) {
            secondCalled = true; secondValid = result.error.empty() && result.png.size() > 8;
        }, error)) return 9;
        deadline = [NSDate dateWithTimeIntervalSinceNow:15];
        while (!secondCalled && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        if (firstCalled || !secondCalled || !secondValid) return 10;
        lucid::modules::WebPreviewAssets redirected;
        if (!redirected.load({
                {"/start.html", "<!doctype html><html><head><script src='/redirect.js'></script></head><body style='background:green'></body></html>"},
                {"/redirect.js", "location.replace('/final.html');"},
                {"/final.html", "<!doctype html><html><body style='background:rgb(255,0,0)'></body></html>"}}, error)) return 11;
        bool redirectedCalled = false, redirectedValid = false;
        if (!preview.start(redirected, "/start.html", 320, 240, [&](auto result) {
            redirectedCalled = true;
            NSData* png = [NSData dataWithBytes:result.png.data() length:result.png.size()];
            NSBitmapImageRep* image = [NSBitmapImageRep imageRepWithData:png];
            NSColor* color = [[image colorAtX:160 y:120] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
            redirectedValid = result.error.empty() && color && color.redComponent > 0.8 && color.greenComponent < 0.2;
        }, error)) return 12;
        deadline = [NSDate dateWithTimeIntervalSinceNow:15];
        while (!redirectedCalled && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        if (!redirectedCalled || !redirectedValid) return 13;
        lucid::modules::WebPreviewAssets svgAssets;
        if (!svgAssets.load({
                {"/index.html", "<!doctype html><html><body style='margin:0'><img src='/image.svg' width='320' height='240'></body></html>"},
                {"/image.svg", "<svg xmlns='http://www.w3.org/2000/svg' width='320' height='240'><rect width='320' height='240' fill='#ff0000'/></svg>"}}, error)) return 14;
        bool svgCalled = false, svgValid = false;
        if (!preview.start(svgAssets, "/index.html", 320, 240, [&](auto result) {
            svgCalled = true;
            NSData* png = [NSData dataWithBytes:result.png.data() length:result.png.size()];
            NSBitmapImageRep* image = [NSBitmapImageRep imageRepWithData:png];
            NSColor* color = [[image colorAtX:160 y:120] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
            svgValid = result.error.empty() && color && color.redComponent > 0.8 && color.greenComponent < 0.2;
        }, error)) return 15;
        deadline = [NSDate dateWithTimeIntervalSinceNow:15];
        while (!svgCalled && [deadline timeIntervalSinceNow] > 0)
            [[NSRunLoop mainRunLoop] runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.05]];
        if (!svgCalled || !svgValid) return 16;
        return 0;
    }
}
