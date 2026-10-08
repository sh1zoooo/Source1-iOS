#import <UIKit/UIKit.h>

// Platform service called by ClientMod's original VGUI ISystem implementation.
extern "C" void ClientModIOSOpenURL(const char *text) {
    if (!text) return;
    NSString *string = [NSString stringWithUTF8String:text];
    NSURL *url = string ? [NSURL URLWithString:string] : nil;
    if (!url || !url.scheme) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
    });
}
