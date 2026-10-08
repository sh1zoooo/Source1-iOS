#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

// Platform service called by ClientMod's original VGUI ISystem implementation.
extern "C" void ClientModIOSOpenURL(const char *text) {
    if (!text) return;
    NSString *string = [NSString stringWithUTF8String:text];
    if ([string hasPrefix:@"/"]) {
        NSURL *directory = [NSURL fileURLWithPath:string isDirectory:YES];
        dispatch_async(dispatch_get_main_queue(), ^{
            for (UIScene *scene in [UIApplication sharedApplication].connectedScenes) {
                if (![scene isKindOfClass:[UIWindowScene class]] || scene.activationState != UISceneActivationStateForegroundActive) continue;
                for (UIWindow *window in ((UIWindowScene *)scene).windows) {
                    if (!window.isKeyWindow) continue;
                    UIViewController *presenter = window.rootViewController;
                    while (presenter.presentedViewController) presenter = presenter.presentedViewController;
                    UIDocumentPickerViewController *picker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:@[UTTypeFolder]];
                    picker.directoryURL = directory;
                    [presenter presentViewController:picker animated:YES completion:nil];
                    return;
                }
            }
        });
        return;
    }
    NSURL *url = string ? [NSURL URLWithString:string] : nil;
    if (!url || !url.scheme) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] openURL:url options:@{} completionHandler:nil];
    });
}
