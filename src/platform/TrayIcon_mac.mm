#ifdef __APPLE__

#include "TrayIcon.hpp"

#include <AppKit/AppKit.h>

@class RDTrayTarget;

namespace rd {
struct TrayIcon::Impl {
    NSStatusItem* statusItem = nil;
    NSMenuItem* closeItem = nil;
    RDTrayTarget* target = nil;
};
} // namespace rd

// Target for the two menu items (NSMenuItem.target is weak, so Impl above keeps the strong ref).
// Clicks just set `pending` here for TrayIcon::Pump() to consume next frame, matching the Windows
// backend's poll-once-per-frame shape. State lives on this object, not TrayIcon::Impl, since that
// type is private and unreachable from a free-standing ObjC class.
typedef NS_ENUM(NSInteger, RDTrayPending) { RDTrayPendingNone, RDTrayPendingClose, RDTrayPendingConfig };

@interface RDTrayTarget : NSObject
@property(nonatomic, assign) BOOL closeEnabled;
@property(nonatomic, assign) RDTrayPending pending;
- (void)onClose:(id)sender;
- (void)onConfig:(id)sender;
@end

@implementation RDTrayTarget

- (instancetype)init {
    self = [super init];
    if (self) _closeEnabled = YES;
    return self;
}

- (void)onClose:(id)sender {
    if (self.closeEnabled) self.pending = RDTrayPendingClose;
}

- (void)onConfig:(id)sender {
    self.pending = RDTrayPendingConfig;
}

@end

namespace rd {

TrayIcon::TrayIcon(const std::string& tooltip) : impl_(new Impl()) {
    RDTrayTarget* target = [[RDTrayTarget alloc] init];
    impl_->target = target;

    impl_->statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];

    // not a template image - it's a colored asset (red + white), template images must be monochrome
    NSString* imagePath = [[NSBundle mainBundle] pathForResource:@"stop_sign" ofType:@"png"
                                                       inDirectory:@"assets/images"];
    NSImage* icon = imagePath ? [[NSImage alloc] initWithContentsOfFile:imagePath] : nil;
    if (icon) {
        icon.size = NSMakeSize(18, 18); // standard menu bar glyph size
        impl_->statusItem.button.image = icon;
    } else {
        impl_->statusItem.button.title = @"\U0001F6D1"; // stop-sign emoji fallback if the asset didn't load
    }
    impl_->statusItem.button.toolTip = @(tooltip.c_str());

    NSMenu* menu = [[NSMenu alloc] init];
    impl_->closeItem = [menu addItemWithTitle:@"Close" action:@selector(onClose:) keyEquivalent:@""];
    impl_->closeItem.target = target;
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* configItem = [menu addItemWithTitle:@"Config..." action:@selector(onConfig:) keyEquivalent:@""];
    configItem.target = target;

    impl_->statusItem.menu = menu;
}

TrayIcon::~TrayIcon() {
    if (impl_->statusItem) [[NSStatusBar systemStatusBar] removeStatusItem:impl_->statusItem];
    delete impl_;
}

void TrayIcon::SetCloseEnabled(bool enabled) {
    impl_->target.closeEnabled = enabled;
    impl_->closeItem.enabled = enabled;
}

void TrayIcon::Pump() {
    RDTrayPending pending = impl_->target.pending;
    impl_->target.pending = RDTrayPendingNone;
    if (pending == RDTrayPendingClose && impl_->target.closeEnabled && onClose) onClose();
    if (pending == RDTrayPendingConfig && onOpenConfig) onOpenConfig();
}

} // namespace rd

#endif // __APPLE__
