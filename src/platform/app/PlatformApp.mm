#include "platform/app/PlatformApp.hpp"

#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <algorithm>
#include <string>
#include <cstring>
#include <vector>

namespace lucid::platform {
namespace {

enum LucidMenuCommand : NSInteger {
    CommandOpenProject = 1,
    CommandOpenFile,
    CommandSave,
    CommandCloseFile,
    CommandSettings,
    CommandRunCompile,
    CommandRunFunction,
    CommandCloseTerminal,
    CommandUndo,
    CommandRedo,
    CommandCut,
    CommandCopy,
    CommandPaste,
    CommandSelectAll,
    CommandDeleteFile,
    CommandDeleteProject,
    CommandComplete,
    CommandAskAi,
    CommandAiCleanup,
    CommandFind,
    CommandReplace,
    CommandSwitchFile,
    CommandTasks,
    CommandGitReview,
    CommandModules,
    CommandRecovery,
    CommandZoomIn, CommandZoomOut, CommandZoomReset, CommandContrast,
    CommandDebugger, CommandBreakpoint,
};

MenuActions g_actions;
id g_menuTarget = nil;
std::function<void(std::string)> g_openFileHandler;
std::vector<std::string> g_pendingFiles;
id g_openFilesTarget = nil;

NSMenuItem* customItem(NSString* title, LucidMenuCommand command, NSString* key,
                       NSEventModifierFlags modifiers) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:@selector(performMenuCommand:)
                                           keyEquivalent:key];
    [item setTarget:g_menuTarget];
    [item setTag:command];
    [item setKeyEquivalentModifierMask:modifiers];
    return item;
}

NSMenuItem* standardItem(NSString* title, SEL action, NSString* key,
                         NSEventModifierFlags modifiers) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:key];
    [item setKeyEquivalentModifierMask:modifiers];
    return item;
}

NSMenu* submenu(NSString* title, NSMenuItem* parent) {
    NSMenu* menu = [[NSMenu alloc] initWithTitle:title];
    [parent setSubmenu:menu];
    return menu;
}

} // namespace
} // namespace lucid::platform

@interface LucidOpenFilesTarget : NSObject
- (void)handleOpenDocuments:(NSAppleEventDescriptor*)event reply:(NSAppleEventDescriptor*)reply;
@end
@implementation LucidOpenFilesTarget
- (void)handleOpenDocuments:(NSAppleEventDescriptor*)event reply:(NSAppleEventDescriptor*)reply {
    (void)reply;
    NSAppleEventDescriptor* files = [event paramDescriptorForKeyword:keyDirectObject];
    for (NSInteger i = 1; i <= [files numberOfItems]; ++i) {
        NSURL* url = [[files descriptorAtIndex:i] fileURLValue];
        if (!url.isFileURL || !url.path.UTF8String) continue;
        std::string path(url.path.UTF8String);
        if (lucid::platform::g_openFileHandler) lucid::platform::g_openFileHandler(std::move(path));
        else lucid::platform::g_pendingFiles.push_back(std::move(path));
    }
}
@end

@interface LucidMenuTarget : NSObject
- (void)performMenuCommand:(id)sender;
@end

@implementation LucidMenuTarget
- (void)performMenuCommand:(id)sender {
    using namespace lucid::platform;
    const LucidMenuCommand command = static_cast<LucidMenuCommand>([sender tag]);
    switch (command) {
    case CommandDebugger: if (g_actions.debugger) g_actions.debugger(); break;
    case CommandBreakpoint: if (g_actions.breakpoint) g_actions.breakpoint(); break;
    case CommandZoomIn: if (g_actions.zoomIn) g_actions.zoomIn(); break;
    case CommandZoomOut: if (g_actions.zoomOut) g_actions.zoomOut(); break;
    case CommandZoomReset: if (g_actions.zoomReset) g_actions.zoomReset(); break;
    case CommandContrast: if (g_actions.contrast) g_actions.contrast(); break;
    case CommandRecovery:
        if (g_actions.recovery) g_actions.recovery();
        break;
    case CommandModules:
        if (g_actions.modules) g_actions.modules();
        break;
    case CommandGitReview:
        if (g_actions.gitReview) g_actions.gitReview();
        break;
    case CommandTasks:
        if (g_actions.tasks) g_actions.tasks();
        break;
    case CommandSwitchFile:
        if (g_actions.switchFile) g_actions.switchFile();
        break;
    case CommandReplace:
        if (g_actions.replace) g_actions.replace();
        break;
    case CommandFind:
        if (g_actions.find) g_actions.find();
        break;
    case CommandComplete:
        if (g_actions.complete) g_actions.complete();
        break;
    case CommandAiCleanup:
        if (g_actions.aiCleanup) g_actions.aiCleanup();
        break;
    case CommandAskAi:
        if (g_actions.askAi) g_actions.askAi();
        break;
    case CommandOpenProject:
        if (g_actions.openProject) g_actions.openProject();
        break;
    case CommandOpenFile:
        if (g_actions.openFile) g_actions.openFile();
        break;
    case CommandSave:
        if (g_actions.save) g_actions.save();
        break;
    case CommandCloseFile:
        if (g_actions.closeFile) g_actions.closeFile();
        break;
    case CommandDeleteFile:
        if (g_actions.deleteFile) g_actions.deleteFile();
        break;
    case CommandDeleteProject:
        if (g_actions.deleteProject) g_actions.deleteProject();
        break;
    case CommandSettings:
        if (g_actions.settings) g_actions.settings();
        break;
    case CommandRunCompile:
        if (g_actions.runCompile) g_actions.runCompile();
        break;
    case CommandRunFunction:
        if (g_actions.runFunction) g_actions.runFunction();
        break;
    case CommandCloseTerminal:
        if (g_actions.closeTerminal) g_actions.closeTerminal();
        break;
    case CommandUndo:
        if (g_actions.undo) g_actions.undo();
        break;
    case CommandRedo:
        if (g_actions.redo) g_actions.redo();
        break;
    case CommandCut:
        if (g_actions.cut) g_actions.cut();
        break;
    case CommandCopy:
        if (g_actions.copy) g_actions.copy();
        break;
    case CommandPaste:
        if (g_actions.paste) g_actions.paste();
        break;
    case CommandSelectAll:
        if (g_actions.selectAll) g_actions.selectAll();
        break;
    }
}
@end

namespace lucid::platform {

void setNativeAppIcon(const unsigned char* rgba, int width, int height) {
    @autoreleasepool {
        NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nil pixelsWide:width pixelsHigh:height
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
            bytesPerRow:width * 4 bitsPerPixel:32];
        if (!bitmap) return;
        std::memcpy(bitmap.bitmapData, rgba, static_cast<std::size_t>(width * height * 4));
        NSImage* image = [[NSImage alloc] initWithSize:NSMakeSize(width, height)];
        [image addRepresentation:bitmap];
        [NSApp setApplicationIconImage:image];
    }
}

void prepareOpenFiles() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        if (!g_openFilesTarget) g_openFilesTarget = [[LucidOpenFilesTarget alloc] init];
        [[NSAppleEventManager sharedAppleEventManager] setEventHandler:g_openFilesTarget
            andSelector:@selector(handleOpenDocuments:reply:)
            forEventClass:kCoreEventClass andEventID:kAEOpenDocuments];
    }
}

void setOpenFileHandler(std::function<void(std::string)> handler) {
    g_openFileHandler = std::move(handler);
    if (g_openFileHandler) {
        auto pending = std::move(g_pendingFiles);
        g_pendingFiles.clear();
        for (auto& path : pending) g_openFileHandler(std::move(path));
    }
}

void installApplicationMenu(const MenuActions& actions) {
    @autoreleasepool {
        g_actions = actions;
        if (g_menuTarget == nil) {
            g_menuTarget = [[LucidMenuTarget alloc] init];
        }

        NSMenu* mainMenu = [[NSMenu alloc] initWithTitle:@""];

        NSMenuItem* appItem = [[NSMenuItem alloc] initWithTitle:@""
                                                         action:nil
                                                  keyEquivalent:@""];
        [mainMenu addItem:appItem];
        NSMenu* appMenu = submenu(@"Lucid", appItem);
        [appMenu addItem:standardItem(@"About Lucid",
                                      @selector(orderFrontStandardAboutPanel:),
                                      @"", 0)];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:customItem(@"Settings…", CommandSettings, @",",
                                    NSEventModifierFlagCommand)];
        [appMenu addItem:[NSMenuItem separatorItem]];

        NSMenuItem* servicesItem = [[NSMenuItem alloc] initWithTitle:@"Services"
                                                              action:nil
                                                       keyEquivalent:@""];
        NSMenu* servicesMenu = submenu(@"Services", servicesItem);
        [appMenu addItem:servicesItem];
        [NSApp setServicesMenu:servicesMenu];

        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:standardItem(@"Hide Lucid", @selector(hide:), @"h",
                                      NSEventModifierFlagCommand)];
        [appMenu addItem:standardItem(@"Hide Others", @selector(hideOtherApplications:),
                                      @"h", NSEventModifierFlagCommand |
                                                NSEventModifierFlagOption)];
        [appMenu addItem:standardItem(@"Show All", @selector(unhideAllApplications:),
                                      @"", 0)];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:standardItem(@"Quit Lucid", @selector(terminate:), @"q",
                                      NSEventModifierFlagCommand)];

        NSMenuItem* fileItem = [[NSMenuItem alloc] initWithTitle:@"File"
                                                          action:nil
                                                   keyEquivalent:@""];
        [mainMenu addItem:fileItem];
        NSMenu* fileMenu = submenu(@"File", fileItem);
        [fileMenu addItem:customItem(@"Open Project…", CommandOpenProject, @"o",
                                     NSEventModifierFlagCommand)];
        [fileMenu addItem:customItem(@"Open File…", CommandOpenFile, @"n",
                                     NSEventModifierFlagCommand)];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItem:customItem(@"Switch Open File…", CommandSwitchFile, @"e", NSEventModifierFlagCommand)];
        [fileMenu addItem:customItem(@"Save", CommandSave, @"s",
                                     NSEventModifierFlagCommand)];
        [fileMenu addItem:customItem(@"Close File", CommandCloseFile, @"w",
                                     NSEventModifierFlagCommand)];
        [fileMenu addItem:[NSMenuItem separatorItem]];
        [fileMenu addItem:customItem(@"Delete File…", CommandDeleteFile, @"",
                                     0)];
        [fileMenu addItem:customItem(@"Delete Project…", CommandDeleteProject, @"",
                                     0)];

        NSMenuItem* editItem = [[NSMenuItem alloc] initWithTitle:@"Edit"
                                                          action:nil
                                                   keyEquivalent:@""];
        [mainMenu addItem:editItem];
        NSMenu* editMenu = submenu(@"Edit", editItem);
        [editMenu addItem:customItem(@"Undo", CommandUndo, @"z",
                                     NSEventModifierFlagCommand)];
        [editMenu addItem:customItem(@"Redo", CommandRedo, @"z",
                                     NSEventModifierFlagCommand |
                                         NSEventModifierFlagShift)];
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItem:customItem(@"Cut", CommandCut, @"x",
                                     NSEventModifierFlagCommand)];
        [editMenu addItem:customItem(@"Copy", CommandCopy, @"c",
                                     NSEventModifierFlagCommand)];
        [editMenu addItem:customItem(@"Paste", CommandPaste, @"v",
                                     NSEventModifierFlagCommand)];
        [editMenu addItem:customItem(@"Select All", CommandSelectAll, @"a",
                                     NSEventModifierFlagCommand)];
        [editMenu addItem:[NSMenuItem separatorItem]];
        [editMenu addItem:customItem(@"Find…", CommandFind, @"f", NSEventModifierFlagCommand)];
        [editMenu addItem:customItem(@"Replace…", CommandReplace, @"f", NSEventModifierFlagCommand | NSEventModifierFlagOption)];
        [editMenu addItem:customItem(@"Complete Functions", CommandComplete, @" ",
                                     NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [editMenu addItem:customItem(@"Clean Up Selection with AI…", CommandAiCleanup, @"l", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [editMenu addItem:customItem(@"Ask AI…", CommandAskAi, @"a",
                                     NSEventModifierFlagCommand | NSEventModifierFlagShift)];

        NSMenuItem* viewItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
        [mainMenu addItem:viewItem];
        NSMenu* viewMenu = submenu(@"View", viewItem);
        [viewMenu addItem:customItem(@"Increase Text Size", CommandZoomIn, @"=", NSEventModifierFlagCommand)];
        [viewMenu addItem:customItem(@"Decrease Text Size", CommandZoomOut, @"-", NSEventModifierFlagCommand)];
        [viewMenu addItem:customItem(@"Reset Text Size", CommandZoomReset, @"0", NSEventModifierFlagCommand)];
        [viewMenu addItem:customItem(@"Toggle High Contrast", CommandContrast, @"u", NSEventModifierFlagCommand | NSEventModifierFlagShift)];

        NSMenuItem* runItem = [[NSMenuItem alloc] initWithTitle:@"Run"
                                                         action:nil
                                                  keyEquivalent:@""];
        [mainMenu addItem:runItem];
        NSMenu* runMenu = submenu(@"Run", runItem);
        [runMenu addItem:customItem(@"Run Compile Function", CommandRunCompile, @"r",
                                    NSEventModifierFlagCommand)];
        [runMenu addItem:customItem(@"Search Project Functions…",
                                    CommandRunFunction, @"r",
                                    NSEventModifierFlagCommand |
                                        NSEventModifierFlagShift)];
        [runMenu addItem:[NSMenuItem separatorItem]];
        [runMenu addItem:customItem(@"Debugger…", CommandDebugger, @"d", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Toggle Breakpoint", CommandBreakpoint, @"b", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Recovery…", CommandRecovery, @"y", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Modules…", CommandModules, @"m", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Git Review…", CommandGitReview, @"g", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Tasks…", CommandTasks, @"t", NSEventModifierFlagCommand | NSEventModifierFlagShift)];
        [runMenu addItem:customItem(@"Close Terminal", CommandCloseTerminal, @"",
                                    0)];

        NSMenuItem* windowItem = [[NSMenuItem alloc] initWithTitle:@"Window"
                                                            action:nil
                                                     keyEquivalent:@""];
        [mainMenu addItem:windowItem];
        NSMenu* windowMenu = submenu(@"Window", windowItem);
        [windowMenu addItem:standardItem(@"Minimize", @selector(performMiniaturize:),
                                         @"m", NSEventModifierFlagCommand)];
        [windowMenu addItem:standardItem(@"Zoom", @selector(performZoom:), @"", 0)];
        [windowMenu addItem:[NSMenuItem separatorItem]];
        [windowMenu addItem:standardItem(@"Bring All to Front",
                                         @selector(arrangeInFront:), @"", 0)];
        [NSApp setWindowsMenu:windowMenu];

        [NSApp setMainMenu:mainMenu];
    }
}

void configureApplicationWindow(void* nativeWindow, std::string_view title) {
    @autoreleasepool {
        if (nativeWindow == nullptr) {
            return;
        }

        auto* window = static_cast<GLFWwindow*>(nativeWindow);
        NSWindow* cocoaWindow = glfwGetCocoaWindow(window);
        if (cocoaWindow == nil) {
            return;
        }

        const std::string titleText(title);
        [cocoaWindow setTitle:[NSString stringWithUTF8String:titleText.c_str()]];
        [cocoaWindow setTitleVisibility:NSWindowTitleHidden];
        [cocoaWindow setTitlebarAppearsTransparent:YES];
        [cocoaWindow setMovableByWindowBackground:NO];
        [cocoaWindow setStyleMask:[cocoaWindow styleMask] |
                                  NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable |
                                  NSWindowStyleMaskFullSizeContentView];

    }
}

void updateAccessibility(void* nativeWindow, std::string_view label, std::string_view value,
                         std::size_t selectionStart, std::size_t selectionLength) {
    if (nativeWindow == nullptr) return;
    @autoreleasepool {
        NSWindow* window = glfwGetCocoaWindow(static_cast<GLFWwindow*>(nativeWindow));
        NSView* view = window.contentView;
        if (!view) return;
        NSString* name = [[NSString alloc] initWithBytes:label.data() length:label.size() encoding:NSUTF8StringEncoding];
        NSString* text = [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding];
        if (!name || !text) return;
        [view setAccessibilityElement:YES];
        [view setAccessibilityRole:NSAccessibilityTextAreaRole];
        [view setAccessibilityLabel:name];
        [view setAccessibilityValue:text];
        const NSUInteger start = std::min<NSUInteger>(selectionStart, text.length);
        const NSUInteger length = std::min<NSUInteger>(selectionLength, text.length - start);
        [view setAccessibilitySelectedTextRange:NSMakeRange(start, length)];
        NSAccessibilityPostNotification(view, NSAccessibilityValueChangedNotification);
        NSAccessibilityPostNotification(view, NSAccessibilitySelectedTextChangedNotification);
    }
}

bool openBinaryFile(std::string_view path) {
    if (!confirmDestructiveAction("Open binary file?", "Open this binary file with the default application?", "Open")) return false;
    @autoreleasepool {
        NSString* file = [NSString stringWithUTF8String:std::string(path).c_str()];
        return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:file]];
    }
}

bool confirmDestructiveAction(std::string_view title, std::string_view message,
                              std::string_view confirmLabel) {
    @autoreleasepool {
        NSAlert* alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSAlertStyleWarning];
        [alert setMessageText:[NSString stringWithUTF8String:std::string(title).c_str()]];
        [alert setInformativeText:[NSString
                                      stringWithUTF8String:std::string(message).c_str()]];
        [alert addButtonWithTitle:[NSString
                                      stringWithUTF8String:std::string(confirmLabel).c_str()]];
        [alert addButtonWithTitle:@"Cancel"];
        return [alert runModal] == NSAlertFirstButtonReturn;
    }
}

} // namespace lucid::platform
