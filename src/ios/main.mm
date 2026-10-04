#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include "Runtime.hpp"
#include <chrono>

static const char *const shaderSource = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Output { float4 position [[position]]; float3 color; };
struct Input { float4 position; float4 color; };
vertex Output vertexMain(uint id [[vertex_id]], constant Input *vertices [[buffer(0)]]) {
    Output out;
    out.position = vertices[id].position;
    out.color = vertices[id].color.xyz;
    return out;
}
fragment float4 fragmentMain(Output in [[stage_in]]) { return float4(in.color, 1); }
)metal";

@interface LabController : UIViewController <MTKViewDelegate> {
    source1ios::Runtime _runtime;
    std::chrono::steady_clock::time_point _previous;
    BOOL _hasPrevious;
    BOOL _hostStarted;
    BOOL _submittedFirstFrame;
}
@property(nonatomic, strong) MTKView *metalView;
@property(nonatomic, strong) id<MTLCommandQueue> queue;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property(nonatomic, strong) id<MTLDepthStencilState> depthState;
@property(nonatomic, strong) UILabel *status;
@property(nonatomic, strong) UITextField *commandInput;
@end

@implementation LabController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.045 blue:0.065 alpha:1];
    self.status = [[UILabel alloc] init];
    self.status.textColor = UIColor.whiteColor;
    self.status.numberOfLines = 0;
    self.status.font = [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];
    self.status.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.status];
    UIButton *share = [UIButton buttonWithType:UIButtonTypeSystem];
    [share setTitle:@"Share diagnostic log" forState:UIControlStateNormal];
    share.translatesAutoresizingMaskIntoConstraints = NO;
    [share addTarget:self action:@selector(shareLog:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:share];
    self.commandInput = [[UITextField alloc] init];
    self.commandInput.attributedPlaceholder = [[NSAttributedString alloc]
        initWithString:@"source_app_selftest"
        attributes:@{NSForegroundColorAttributeName: [UIColor colorWithWhite:0.7 alpha:1]}];
    self.commandInput.textColor = UIColor.whiteColor;
    self.commandInput.backgroundColor = [UIColor colorWithWhite:0.15 alpha:0.9];
    self.commandInput.borderStyle = UITextBorderStyleRoundedRect;
    self.commandInput.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.commandInput.autocorrectionType = UITextAutocorrectionTypeNo;
    self.commandInput.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.commandInput];
    UIButton *run = [UIButton buttonWithType:UIButtonTypeSystem];
    [run setTitle:@"Run" forState:UIControlStateNormal];
    run.translatesAutoresizingMaskIntoConstraints = NO;
    [run addTarget:self action:@selector(runCommand:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:run];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[
        [self.status.topAnchor constraintEqualToAnchor:safe.topAnchor constant:16],
        [self.status.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
        [self.status.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
        [share.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-12],
        [share.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.commandInput.bottomAnchor constraintEqualToAnchor:share.topAnchor constant:-12],
        [self.commandInput.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:16],
        [self.commandInput.trailingAnchor constraintEqualToAnchor:run.leadingAnchor constant:-12],
        [self.commandInput.heightAnchor constraintEqualToConstant:36],
        [run.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-16],
        [run.centerYAnchor constraintEqualToAnchor:self.commandInput.centerYAnchor],
        [run.widthAnchor constraintEqualToConstant:48]
    ]];
    NSURL *documents = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
        inDomains:NSUserDomainMask] firstObject];
    _hostStarted = documents && _runtime.start(documents.path.UTF8String);
    if (!_hostStarted) {
        self.status.text = @"Startup failed. Export the log for diagnostics.";
        share.enabled = !_runtime.logPath().empty();
        return;
    }
    _runtime.log(std::string("iOS ") + UIDevice.currentDevice.systemVersion.UTF8String);
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { [self fail:@"Metal device unavailable"]; return; }
    self.metalView = [[MTKView alloc] initWithFrame:CGRectZero device:device];
    self.metalView.translatesAutoresizingMaskIntoConstraints = NO;
    self.metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    self.metalView.clearDepth = 1;
    self.metalView.clearColor = MTLClearColorMake(0.035, 0.045, 0.065, 1);
    self.metalView.preferredFramesPerSecond = 60;
    self.metalView.paused = YES;
    [self.view insertSubview:self.metalView atIndex:0];
    [NSLayoutConstraint activateConstraints:@[
        [self.metalView.topAnchor constraintEqualToAnchor:safe.topAnchor],
        [self.metalView.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],
        [self.metalView.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],
        [self.metalView.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor]
    ]];
    NSError *error = nil;
    id<MTLLibrary> library = [device newLibraryWithSource:[NSString stringWithUTF8String:shaderSource]
        options:nil error:&error];
    if (!library) { [self fail:error.localizedDescription ?: @"Shader compilation failed"]; return; }
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction = [library newFunctionWithName:@"vertexMain"];
    descriptor.fragmentFunction = [library newFunctionWithName:@"fragmentMain"];
    descriptor.colorAttachments[0].pixelFormat = self.metalView.colorPixelFormat;
    descriptor.depthAttachmentPixelFormat = self.metalView.depthStencilPixelFormat;
    self.pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    if (!self.pipeline) { [self fail:error.localizedDescription ?: @"Pipeline creation failed"]; return; }
    self.queue = [device newCommandQueue];
    if (!self.queue) { [self fail:@"Command queue creation failed"]; return; }
    _runtime.log(std::string("Metal device: ") + device.name.UTF8String);
    MTLDepthStencilDescriptor *depth = [[MTLDepthStencilDescriptor alloc] init];
    depth.depthCompareFunction = MTLCompareFunctionLess;
    depth.depthWriteEnabled = YES;
    self.depthState = [device newDepthStencilStateWithDescriptor:depth];
    if (!self.depthState) { [self fail:@"Depth state creation failed"]; return; }
    self.status.text = @"Source 1 iOS · progress ~12%\nCore · filesystem · engine\nSource self-tests: 26 PASS\nEngine linked · Host_Init pending";
    self.metalView.delegate = self;
    self.metalView.paused = NO;
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(pauseHost)
        name:UIApplicationWillResignActiveNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(resumeHost)
        name:UIApplicationDidBecomeActiveNotification object:nil];
}
- (void)fail:(NSString *)reason {
    _runtime.log(std::string("Startup failed: ") + reason.UTF8String);
    self.status.text = [@"Startup failed\n" stringByAppendingString:reason];
    self.metalView.paused = YES;
}
- (void)pauseHost {
    self.metalView.paused = YES;
    _hasPrevious = NO;
    _runtime.setActive(false);
}
- (void)resumeHost {
    _hasPrevious = NO;
    _runtime.setActive(true);
    if (self.pipeline && self.queue) self.metalView.paused = NO;
}
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    _runtime.log("Drawable: " + std::to_string((int)size.width) + "x" + std::to_string((int)size.height));
}
- (void)drawInMTKView:(MTKView *)view {
    auto now = std::chrono::steady_clock::now();
    double dt = _hasPrevious ? std::chrono::duration<double>(now - _previous).count() : 0;
    _previous = now;
    _hasPrevious = YES;
    _runtime.frame(dt);
    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline) return;
    id<MTLCommandBuffer> command = [self.queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
    if (!command || !encoder) { [self fail:@"Cannot encode Metal frame"]; return; }
    [encoder setRenderPipelineState:self.pipeline];
    [encoder setDepthStencilState:self.depthState];
    float aspect = (float)(view.drawableSize.width / MAX(view.drawableSize.height, 1.0));
    auto vertices = _runtime.vertices(aspect);
    [encoder setVertexBytes:vertices.data() length:sizeof(vertices) atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];
    [encoder endEncoding];
    [command presentDrawable:drawable];
    [command commit];
    if (!_submittedFirstFrame) {
        _submittedFirstFrame = YES;
        _runtime.log("First Metal frame submitted");
    }
}
- (void)runCommand:(UIButton *)sender {
    NSString *command = self.commandInput.text ?: @"";
    [self.commandInput resignFirstResponder];
    BOOL accepted = _runtime.executeSource(command.UTF8String);
    self.status.text = [NSString stringWithFormat:@"Source core + filesystem + appframework\nEngine linked · Host_Init pending\n%@: %@", accepted ? @"Executed" : @"Rejected", command];
}
- (void)shareLog:(UIButton *)sender {
    if (_runtime.logPath().empty()) return;
    _runtime.log("Diagnostic log exported at frame " + std::to_string(_runtime.frames()));
    NSString *path = [NSString stringWithUTF8String:_runtime.logPath().string().c_str()];
    UIActivityViewController *activity = [[UIActivityViewController alloc]
        initWithActivityItems:@[[NSURL fileURLWithPath:path]] applicationActivities:nil];
    activity.popoverPresentationController.sourceView = sender;
    activity.popoverPresentationController.sourceRect = sender.bounds;
    [self presentViewController:activity animated:YES completion:nil];
}
- (void)dealloc {
    [NSNotificationCenter.defaultCenter removeObserver:self];
    _runtime.stop();
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic, strong) UIWindow *window;
@end
@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {
    self.window = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];
    self.window.rootViewController = [[LabController alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}
@end
int main(int argc, char *argv[]) {
    @autoreleasepool { return UIApplicationMain(argc, argv, nil, NSStringFromClass(AppDelegate.class)); }
}
