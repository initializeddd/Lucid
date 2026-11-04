#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#import <Cocoa/Cocoa.h>

bool triggerCompletionMenuShortcut() {
    NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSZeroPoint
                                modifierFlags:NSEventModifierFlagCommand | NSEventModifierFlagShift
                                    timestamp:0
                                 windowNumber:0
                                      context:nil
                                   characters:@" "
                  charactersIgnoringModifiers:@" "
                                    isARepeat:NO
                                      keyCode:49];
    return [[NSApp mainMenu] performKeyEquivalent:event];
}

bool triggerReplaceMenuShortcut() {
    NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSZeroPoint
                                modifierFlags:NSEventModifierFlagCommand | NSEventModifierFlagOption
                                    timestamp:0
                                 windowNumber:0
                                      context:nil
                                   characters:@"f"
                  charactersIgnoringModifiers:@"f"
                                    isARepeat:NO
                                      keyCode:3];
    return [[NSApp mainMenu] performKeyEquivalent:event];
}

#import <Carbon/Carbon.h>
bool sendOpenFileEvent(const char* path) {
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    NSAppleEventDescriptor* list = [NSAppleEventDescriptor listDescriptor];
    [list insertDescriptor:[NSAppleEventDescriptor descriptorWithFileURL:url] atIndex:1];
    NSAppleEventDescriptor* event = [NSAppleEventDescriptor appleEventWithEventClass:kCoreEventClass
        eventID:kAEOpenDocuments targetDescriptor:[NSAppleEventDescriptor currentProcessDescriptor]
        returnID:kAutoGenerateReturnID transactionID:kAnyTransactionID];
    [event setParamDescriptor:list forKeyword:keyDirectObject];
    AppleEvent reply = {typeNull, nullptr};
    int refcon = 0;
    const OSErr error = [[NSAppleEventManager sharedAppleEventManager]
        dispatchRawAppleEvent:event.aeDesc withRawReply:&reply handlerRefCon:&refcon];
    AEDisposeDesc(&reply);
    return error == noErr;
}

bool triggerSwitchFileShortcut() {
    NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSZeroPoint
                                modifierFlags:NSEventModifierFlagCommand
                                    timestamp:0 windowNumber:0 context:nil
                                   characters:@"e" charactersIgnoringModifiers:@"e"
                                    isARepeat:NO keyCode:14];
    return [[NSApp mainMenu] performKeyEquivalent:event];
}

bool triggerCloseFileShortcut() {
    NSEvent* event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSZeroPoint modifierFlags:NSEventModifierFlagCommand
                                    timestamp:0 windowNumber:0 context:nil
                                   characters:@"w" charactersIgnoringModifiers:@"w"
                                    isARepeat:NO keyCode:13];
    return [[NSApp mainMenu] performKeyEquivalent:event];
}

bool checkAccessibility(void* nativeWindow, const char* expected, unsigned long start, unsigned long length) {
    NSWindow* window = glfwGetCocoaWindow(static_cast<GLFWwindow*>(nativeWindow));
    NSView* view = window.contentView;
    NSString* value = [view accessibilityValue];
    const NSRange range = [view accessibilitySelectedTextRange];
    return [[view accessibilityRole] isEqualToString:NSAccessibilityTextAreaRole] &&
        [value isEqualToString:[NSString stringWithUTF8String:expected]] &&
        range.location == start && range.length == length;
}

bool accessibilityLabelContains(void* nativeWindow, const char* expected) {
    NSWindow* window = glfwGetCocoaWindow(static_cast<GLFWwindow*>(nativeWindow));
    NSString* label = [window.contentView accessibilityLabel];
    return [label containsString:[NSString stringWithUTF8String:expected]];
}
