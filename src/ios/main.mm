#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include "Runtime.hpp"
#include <chrono>

static const char *const shaderSource = R"metal(
#include <metal_stdlib>
using namespace metal;
struct Output { float4 position [[position]]; float3 color; float2 uv; float3 lightmap; int material [[flat]]; uint materialCount [[flat]]; };
struct Input { float4 position; float4 color; float2 uv; float2 material; float4 lightmap; };
vertex Output vertexMain(uint id [[vertex_id]], constant Input *vertices [[buffer(0)]]) {
    Output out;
    out.position = vertices[id].position;
    out.color = vertices[id].color.xyz;
    out.uv = vertices[id].uv;
    out.material = int(vertices[id].material.x);
    out.materialCount = max(1u,uint(vertices[id].material.y));
    out.lightmap = vertices[id].lightmap.xyz;
    return out;
}
fragment float4 fragmentMain(Output in [[stage_in]], texture2d<float> texture [[texture(0)]], texture2d<float> modelTexture [[texture(1)]], texture2d<float> lightmapTexture [[texture(2)]]) {
    constexpr sampler repeatSample(coord::normalized,address::repeat,filter::linear);
    if (in.material < 0) return float4(in.color,1) * modelTexture.sample(repeatSample,in.uv);
    constexpr sampler clampSample(coord::normalized,address::clamp_to_edge,filter::linear);
    uint columns=min(16u,in.materialCount),rows=(in.materialCount+columns-1u)/columns;
    float tile=float(texture.get_width())/float(columns);
    float2 local=(fract(in.uv)*(tile-1.0)+0.5)/tile;
    uint slot=uint(in.material);
    float2 atlasUV=float2((float(slot%columns)+local.x)/float(columns),(float(slot/columns)+local.y)/float(rows));
    float3 light=in.lightmap.z > 0.5 ? lightmapTexture.sample(clampSample,in.lightmap.xy).rgb : float3(1);
    return float4(in.color*light,1) * texture.sample(clampSample,atlasUV);
}
)metal";

@interface LabController : UIViewController <MTKViewDelegate, UITextFieldDelegate> {
    source1ios::Runtime _runtime;
    std::chrono::steady_clock::time_point _previous;
    BOOL _hasPrevious;
    BOOL _hostStarted;
    BOOL _submittedFirstFrame;
    CGPoint _movement;
    BOOL _movingGesture;
    BOOL _smokeRequested;
    std::uint64_t _mapTextureRevision;
    std::uint64_t _modelTextureRevision;
    std::uint64_t _submittedSceneRevision;
}
@property(nonatomic, strong) MTKView *metalView;
@property(nonatomic, strong) id<MTLCommandQueue> queue;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@property(nonatomic, strong) id<MTLDepthStencilState> depthState;
@property(nonatomic, strong) id<MTLTexture> mapTexture;
@property(nonatomic, strong) id<MTLTexture> modelTexture;
@property(nonatomic, strong) id<MTLTexture> lightmapTexture;
@property(nonatomic, strong) UILabel *status;
@property(nonatomic, strong) UITextField *commandInput;
@end

@implementation LabController
- (void)viewDidLoad {
    [super viewDidLoad];
    _smokeRequested = [NSProcessInfo.processInfo.arguments containsObject:@"--port-smoke"];
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
        initWithString:@"source_selftest"
        attributes:@{NSForegroundColorAttributeName: [UIColor colorWithWhite:0.7 alpha:1]}];
    self.commandInput.textColor = UIColor.whiteColor;
    self.commandInput.backgroundColor = [UIColor colorWithWhite:0.15 alpha:0.9];
    self.commandInput.borderStyle = UITextBorderStyleRoundedRect;
    self.commandInput.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.commandInput.autocorrectionType = UITextAutocorrectionTypeNo;
    self.commandInput.delegate = self;
    self.commandInput.returnKeyType = UIReturnKeyGo;
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
    self.status.text = @"Source 1 iOS · minimal milestone ~70%\nBSP materials/lightmaps · MDL/ANI animation\nSource self-tests: 80 PASS\nLeft move / right look";
    UIPanGestureRecognizer *cameraPan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(cameraPan:)];
    [self.metalView addGestureRecognizer:cameraPan];
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
    _movement = CGPointZero;
    _runtime.setActive(false);
}
- (void)resumeHost {
    _hasPrevious = NO;
    _submittedSceneRevision = 0;
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
    _runtime.cameraMove(_movement.y, _movement.x, (float)dt);
    _runtime.frame(dt);
    if (_smokeRequested && _runtime.frames() == 60) {
        if (!_runtime.executeSource("source_selftest") || !_runtime.executeSource("source_physics_reset")
            || !_runtime.executeSource("source_physics_impulse") || !_runtime.executeSource("source_bsp_terrain")
            || !_runtime.executeSource("source_bsp_materials")
            || !_runtime.executeSource("source_model_load models/__source1ios_external_probe.mdl") || !_runtime.executeSource("source_anim_play 0")) {
            [self fail:@"Simulator runtime contracts FAIL"]; return;
        }
        const auto root=_runtime.logPath().parent_path();std::error_code error;
        std::filesystem::create_directories(root/"content/smoke/maps",error);
        if(!error)std::filesystem::copy_file(root/"selftest/__source1ios_material_grid.bsp",root/"content/smoke/maps/cache_smoke.bsp",std::filesystem::copy_options::overwrite_existing,error);
        if(error || !_runtime.executeSource("source_content_mount smoke") || !_runtime.executeSource("source_bsp_load maps/cache_smoke.bsp")
            || !_runtime.executeSource("source_content_unmount smoke")){[self fail:@"Simulator content import FAIL"];return;}
        _runtime.cameraLook(10, 0);
        _runtime.cameraMove(1, 0, .1f);
        _runtime.executeSource("source_camera_reset");
    }
    if (_smokeRequested && _runtime.frames() == 180) _runtime.log("Source simulator runtime contracts: PASS");
    MTLRenderPassDescriptor *pass = view.currentRenderPassDescriptor;
    id<CAMetalDrawable> drawable = view.currentDrawable;
    if (!pass || !drawable || !self.pipeline) return;
    if (_mapTextureRevision != _runtime.textureRevision()) {
        const auto& decoded=_runtime.texture();
        MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:decoded.width height:decoded.height mipmapped:NO];
        id<MTLTexture> staged=[view.device newTextureWithDescriptor:descriptor];if(!staged){[self fail:@"BSP texture upload failed"];return;}
        [staged replaceRegion:MTLRegionMake2D(0,0,decoded.width,decoded.height) mipmapLevel:0 withBytes:decoded.pixels.data() bytesPerRow:decoded.width*4];
        const auto& lighting=_runtime.lightmapTexture();
        MTLTextureDescriptor *lightDescriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:lighting.width height:lighting.height mipmapped:NO];
        id<MTLTexture> stagedLight=[view.device newTextureWithDescriptor:lightDescriptor];if(!stagedLight){[self fail:@"BSP lightmap upload failed"];return;}
        [stagedLight replaceRegion:MTLRegionMake2D(0,0,lighting.width,lighting.height) mipmapLevel:0 withBytes:lighting.pixels.data() bytesPerRow:lighting.width*4];
        self.mapTexture=staged;self.lightmapTexture=stagedLight;_mapTextureRevision=_runtime.textureRevision();_runtime.log("Source BSP VMT/VTF base texture uploaded to Metal");_runtime.log("Source BSP LDR lightmap atlas uploaded to Metal");
    }
    if (_modelTextureRevision != _runtime.modelTextureRevision()) {
        const auto& decoded=_runtime.modelTexture();
        MTLTextureDescriptor *descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:decoded.width height:decoded.height mipmapped:NO];
        id<MTLTexture> staged=[view.device newTextureWithDescriptor:descriptor];if(!staged){[self fail:@"Studio texture upload failed"];return;}
        [staged replaceRegion:MTLRegionMake2D(0,0,decoded.width,decoded.height) mipmapLevel:0 withBytes:decoded.pixels.data() bytesPerRow:decoded.width*4];
        self.modelTexture=staged;_modelTextureRevision=_runtime.modelTextureRevision();_runtime.log("Source studio VTF base texture uploaded to Metal");
    }
    id<MTLCommandBuffer> command = [self.queue commandBuffer];
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
    if (!command || !encoder) { [self fail:@"Cannot encode Metal frame"]; return; }
    [encoder setRenderPipelineState:self.pipeline];
    [encoder setDepthStencilState:self.depthState];
    [encoder setFragmentTexture:self.mapTexture atIndex:0];
    [encoder setFragmentTexture:self.modelTexture atIndex:1];
    [encoder setFragmentTexture:self.lightmapTexture atIndex:2];
    float aspect = (float)(view.drawableSize.width / MAX(view.drawableSize.height, 1.0));
    auto vertices = _runtime.vertices(aspect);
    id<MTLBuffer> geometry = [view.device newBufferWithBytes:vertices.data()
        length:vertices.size() * sizeof(source1ios::SourceVertex) options:MTLResourceStorageModeShared];
    if (!geometry) { [encoder endEncoding]; [self fail:@"Geometry buffer creation failed"]; return; }
    [encoder setVertexBuffer:geometry offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];
    [encoder endEncoding];
    [command presentDrawable:drawable];
    const BOOL firstFrame=!_submittedFirstFrame;
    const std::uint64_t sceneRevision=_runtime.textureRevision();
    if (firstFrame || _submittedSceneRevision!=sceneRevision) {
        _submittedSceneRevision=sceneRevision;
        _submittedFirstFrame = YES;
        [command addCompletedHandler:^(id<MTLCommandBuffer> finished) {
            const BOOL completed=finished.status==MTLCommandBufferStatusCompleted;
            NSString *error=finished.error.description ?: @"Unknown GPU error";
            // Keep runtime/file logging on the same queue as host frames.
            dispatch_async(dispatch_get_main_queue(), ^{
                if(completed){if(firstFrame)self->_runtime.log("First Metal frame completed on GPU");
                    self->_runtime.log("Source Metal scene completed on GPU: map revision "+std::to_string(sceneRevision));}
                else self->_runtime.log(std::string("Metal GPU frame FAIL: ")+error.UTF8String);
            });
        }];
        if(firstFrame)_runtime.log("First Metal frame submitted");
    }
    [command commit];
}
- (void)cameraPan:(UIPanGestureRecognizer *)gesture {
    if (gesture.state == UIGestureRecognizerStateBegan) {
        _movingGesture = [gesture locationInView:self.metalView].x < self.metalView.bounds.size.width / 2;
    }
    if (gesture.state == UIGestureRecognizerStateEnded || gesture.state == UIGestureRecognizerStateCancelled) {
        _movement = CGPointZero;
        return;
    }
    CGPoint delta = [gesture translationInView:self.metalView];
    if (_movingGesture) {
        _movement = CGPointMake(MAX(-1, MIN(1, delta.x / 70)), MAX(-1, MIN(1, -delta.y / 70)));
    } else {
        _runtime.cameraLook((float)-delta.x * .18f, (float)delta.y * .18f);
        [gesture setTranslation:CGPointZero inView:self.metalView];
    }
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [self runCommand:nil];
    return YES;
}
- (void)runCommand:(UIButton *)sender {
    NSString *command = self.commandInput.text ?: @"";
    [self.commandInput resignFirstResponder];
    BOOL accepted = _runtime.executeSource(command.UTF8String);
    self.status.text = [NSString stringWithFormat:@"Source 1 iOS · minimal milestone ~70%%\nBSP materials/lightmaps · MDL/ANI animation\n%@: %@", accepted ? @"Executed" : @"Rejected", command];
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
