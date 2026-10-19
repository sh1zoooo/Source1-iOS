#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#include "StartupOverlay.hpp"
#include "../source/IOSStartupState.hpp"
#include <cstdio>

@interface CMStartupController : UIViewController
@end
@implementation CMStartupController
- (UIInterfaceOrientationMask)supportedInterfaceOrientations { return UIInterfaceOrientationMaskLandscape; }
- (BOOL)prefersStatusBarHidden { return YES; }
@end

static UIWindow *startupWindow;
static UILabel *phaseLabel;
static IOSStartupState startupState;

static void applySignal(NSString *phase, int event) {
    if (startupState.done()) return;
    if (event == 1) startupState.menuReady();
    if (event == 2 && startupState.presented()) {
        startupWindow.hidden = YES;
        startupWindow.rootViewController = nil;
#if !__has_feature(objc_arc)
        [startupWindow release];
#endif
        startupWindow = nil;
        phaseLabel = nil;
        std::fprintf(stderr, "iOS startup overlay: menu presented, dismissed\n");
    } else if (phase.length && startupWindow && ![phaseLabel.text isEqualToString:phase]) {
        phaseLabel.text = phase;
        [phaseLabel.superview layoutIfNeeded];
        [CATransaction flush];
    }
}

extern "C" __attribute__((visibility("default"), used))
void ClientModIOSStartupSignal(const char *phase, int event) {
    // Copy strings before a worker/module can invalidate their storage.
    NSString *text = phase ? [NSString stringWithUTF8String:phase] : nil;
    if ([NSThread isMainThread]) applySignal(text, event);
    else dispatch_async(dispatch_get_main_queue(), ^{ applySignal(text, event); });
}

void ClientModStartupPhase(const char *phase) { ClientModIOSStartupSignal(phase, 0); }

void ClientModShowStartup() {
    if (startupWindow || startupState.done()) return;
    UIWindowScene *scene = nil;
    for (UIScene *candidate in UIApplication.sharedApplication.connectedScenes) {
        if ([candidate isKindOfClass:[UIWindowScene class]] &&
            candidate.activationState != UISceneActivationStateUnattached) {
            scene = (UIWindowScene *)candidate; break;
        }
    }
    startupWindow = scene ? [[UIWindow alloc] initWithWindowScene:scene] :
                            [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    startupWindow.frame = scene ? scene.coordinateSpace.bounds : UIScreen.mainScreen.bounds;
    startupWindow.windowLevel = UIWindowLevelNormal + 10;
    startupWindow.userInteractionEnabled = NO;
    CMStartupController *controller = [[CMStartupController alloc] init];
    controller.view.backgroundColor = [UIColor colorWithRed:.035 green:.045 blue:.065 alpha:1];
    UILabel *title = [[UILabel alloc] init];
    title.text = @"CLIENTMOD";
    title.textColor = UIColor.whiteColor;
    title.font = [UIFont systemFontOfSize:34 weight:UIFontWeightBold];
    title.textAlignment = NSTextAlignmentCenter;
    phaseLabel = [[UILabel alloc] init];
    phaseLabel.text = @"Подготовка запуска…";
    phaseLabel.textColor = [UIColor colorWithWhite:.8 alpha:1];
    phaseLabel.font = [UIFont systemFontOfSize:17 weight:UIFontWeightMedium];
    phaseLabel.textAlignment = NSTextAlignmentCenter;
    phaseLabel.numberOfLines = 2;
    UIActivityIndicatorView *spinner = [[UIActivityIndicatorView alloc]
        initWithActivityIndicatorStyle:UIActivityIndicatorViewStyleLarge];
    spinner.color = [UIColor colorWithRed:.45 green:.75 blue:1 alpha:1];
    [spinner startAnimating];
    UILabel *hint = [[UILabel alloc] init];
    hint.text = @"Первый запуск может занять больше времени";
    hint.font = [UIFont systemFontOfSize:12];
    hint.textColor = [UIColor colorWithWhite:.5 alpha:1];
    hint.textAlignment = NSTextAlignmentCenter;
    hint.numberOfLines = 2;
    UIStackView *stack = [[UIStackView alloc] initWithArrangedSubviews:@[title, spinner, phaseLabel, hint]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.alignment = UIStackViewAlignmentCenter;
    stack.spacing = 18;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [controller.view addSubview:stack];
    [NSLayoutConstraint activateConstraints:@[
        [stack.centerXAnchor constraintEqualToAnchor:controller.view.centerXAnchor],
        [stack.centerYAnchor constraintEqualToAnchor:controller.view.centerYAnchor],
        [stack.widthAnchor constraintLessThanOrEqualToAnchor:controller.view.safeAreaLayoutGuide.widthAnchor multiplier:.9],
        [phaseLabel.widthAnchor constraintEqualToAnchor:stack.widthAnchor],
        [hint.widthAnchor constraintEqualToAnchor:stack.widthAnchor]
    ]];
    startupWindow.rootViewController = controller;
    // Never make this window key: SDL keeps keyboard and touch ownership.
    startupWindow.hidden = NO;
    [controller.view layoutIfNeeded];
    [CATransaction flush];
    // Only before entering the engine; no nested UIKit runloop during Host_Init.
    [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:.02]];
#if !__has_feature(objc_arc)
    [controller release]; [title release]; [spinner release];
    [phaseLabel release]; [hint release]; [stack release];
#endif
}
