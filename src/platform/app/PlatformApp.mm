#include "platform/app/PlatformApp.hpp"

#import <Cocoa/Cocoa.h>

#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include <string>

namespace lucid::platform {
namespace {

enum MenuCommand : NSInteger {
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
};

MenuActions g_actions;
id g_menuTarget = nil;

NSMenuItem* customItem(NSString* title, MenuCommand command, NSString* key,
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

@interface LucidMenuTarget : NSObject
- (void)performMenuCommand:(id)sender;
@end

@implementation LucidMenuTarget
- (void)performMenuCommand:(id)sender {
    using namespace lucid::platform;
    const MenuCommand command = static_cast<MenuCommand>([sender tag]);
    switch (command) {
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
        NSMenu* appMenu = submenu(@"Lucid (Beta)", appItem);
        [appMenu addItem:standardItem(@"About Lucid (Beta)",
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
        [appMenu addItem:standardItem(@"Hide Lucid (Beta)", @selector(hide:), @"h",
                                      NSEventModifierFlagCommand)];
        [appMenu addItem:standardItem(@"Hide Others", @selector(hideOtherApplications:),
                                      @"h", NSEventModifierFlagCommand |
                                                NSEventModifierFlagOption)];
        [appMenu addItem:standardItem(@"Show All", @selector(unhideAllApplications:),
                                      @"", 0)];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:standardItem(@"Quit Lucid (Beta)", @selector(terminate:), @"q",
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
