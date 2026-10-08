#import <UIKit/UIKit.h>
#import <MetalKit/MetalKit.h>
#include "Runtime.hpp"
#include <chrono>
#include <cmath>

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
    bool model=in.material<0;
    if (model && in.materialCount==1u) return float4(in.color,1) * modelTexture.sample(repeatSample,in.uv);
    constexpr sampler clampSample(coord::normalized,address::clamp_to_edge,filter::linear);
    uint columns=min(16u,in.materialCount),rows=(in.materialCount+columns-1u)/columns;
    float tile=float(model?modelTexture.get_width():texture.get_width())/float(columns);
    float2 local=(fract(in.uv)*(tile-1.0)+0.5)/tile;
    uint slot=model?uint(-in.material-1):uint(in.material);
    float2 atlasUV=float2((float(slot%columns)+local.x)/float(columns),(float(slot/columns)+local.y)/float(rows));
    float3 light=in.lightmap.z > 0.5 ? lightmapTexture.sample(clampSample,in.lightmap.xy).rgb : float3(1);
    return float4(in.color*light,1) * (model?modelTexture.sample(clampSample,atlasUV):texture.sample(clampSample,atlasUV));
}
)metal";

@interface LabController : UIViewController <MTKViewDelegate, UITextFieldDelegate, UIGestureRecognizerDelegate, UITableViewDelegate, UITableViewDataSource> {
    source1ios::Runtime _runtime;
    std::chrono::steady_clock::time_point _previous;
    BOOL _hasPrevious;
    BOOL _hostStarted;
    BOOL _submittedFirstFrame;
    CGPoint _movement;
    BOOL _smokeRequested;
    BOOL _menuSmokeRequested;
    BOOL _thirdPerson;
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
@property(nonatomic, strong) UILabel *hud;
@property(nonatomic, strong) UITextView *diagnostics;
@property(nonatomic, strong) UIView *menu;
@property(nonatomic, strong) UITableView *mapTable;
@property(nonatomic, strong) NSArray<NSString *> *mapNames;
@property(nonatomic, strong) UIButton *shareButton;
@property(nonatomic, strong) UIButton *runButton;
@property(nonatomic, strong) NSMutableArray<UIButton *> *gameButtons;
@property(nonatomic, strong) NSArray<NSLayoutConstraint *> *diagnosticLayout;
@property(nonatomic, strong) NSArray<NSLayoutConstraint *> *gameLayout;
@property(nonatomic, strong) UILabel *crosshair;
@property(nonatomic, strong) UIButton *play;
@end

@implementation LabController
- (void)viewDidLoad {
    [super viewDidLoad];
    _smokeRequested = [NSProcessInfo.processInfo.arguments containsObject:@"--port-smoke"];
    _menuSmokeRequested=[NSProcessInfo.processInfo.arguments containsObject:@"--port-menu-smoke"];
    self.view.backgroundColor = [UIColor colorWithRed:0.035 green:0.045 blue:0.065 alpha:1];
    self.status = [[UILabel alloc] init];
    self.status.textColor = UIColor.whiteColor;
    self.status.numberOfLines = 0;
    self.status.font = [UIFont monospacedSystemFontOfSize:13 weight:UIFontWeightRegular];
    self.status.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.status];
    UIButton *share = [UIButton buttonWithType:UIButtonTypeSystem];self.shareButton=share;
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
    UIButton *run = [UIButton buttonWithType:UIButtonTypeSystem];self.runButton=run;
    [run setTitle:@"Run" forState:UIControlStateNormal];
    run.translatesAutoresizingMaskIntoConstraints = NO;
    [run addTarget:self action:@selector(runCommand:) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:run];
    self.gameButtons=[NSMutableArray array];
    UIStackView *controls=[[UIStackView alloc] init];
    controls.axis=UILayoutConstraintAxisHorizontal;controls.distribution=UIStackViewDistributionFillEqually;controls.spacing=4;
    controls.translatesAutoresizingMaskIntoConstraints=NO;
    NSArray<NSString *> *titles=@[@"Play",@"Jump",@"Duck",@"Fire",@"Reload",@"3P"];
    const unsigned flags[]={0,source1ios::PlayerJump,source1ios::PlayerDuck,source1ios::PlayerAttack,source1ios::PlayerReload,0};
    for(NSUInteger i=0;i<titles.count;++i){UIButton *button=[UIButton buttonWithType:UIButtonTypeSystem];
        [button setTitle:titles[i] forState:UIControlStateNormal];button.titleLabel.font=[UIFont systemFontOfSize:12 weight:UIFontWeightSemibold];
        button.backgroundColor=[UIColor colorWithWhite:.1 alpha:.85];button.layer.cornerRadius=6;button.tag=flags[i];
        button.accessibilityIdentifier=[@"practice_" stringByAppendingString:titles[i].lowercaseString];
        if(i==0){self.play=button;[button addTarget:self action:@selector(togglePractice:) forControlEvents:UIControlEventTouchUpInside];}
        else if(i==5)[button addTarget:self action:@selector(togglePerspective:) forControlEvents:UIControlEventTouchUpInside];
        else{[button addTarget:self action:@selector(pressPlayer:) forControlEvents:UIControlEventTouchDown];
            [button addTarget:self action:@selector(releasePlayer:) forControlEvents:UIControlEventTouchUpInside|UIControlEventTouchUpOutside|UIControlEventTouchCancel];}
        if(i==0)[controls addArrangedSubview:button];else{[self.gameButtons addObject:button];button.translatesAutoresizingMaskIntoConstraints=NO;button.layer.cornerRadius=32;button.layer.borderWidth=1;button.layer.borderColor=[UIColor colorWithWhite:1 alpha:.45].CGColor;button.backgroundColor=[UIColor colorWithWhite:.1 alpha:.25];[self.view addSubview:button];}

    }
    [self.view addSubview:controls];
    self.hud=[[UILabel alloc] init];self.hud.textColor=UIColor.whiteColor;self.hud.numberOfLines=2;
    self.hud.font=[UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightSemibold];self.hud.translatesAutoresizingMaskIntoConstraints=NO;
    self.hud.backgroundColor=[UIColor colorWithWhite:0 alpha:.65];[self.view addSubview:self.hud];
    self.crosshair=[[UILabel alloc] init];self.crosshair.text=@"+";self.crosshair.textColor=UIColor.whiteColor;
    self.crosshair.font=[UIFont monospacedSystemFontOfSize:20 weight:UIFontWeightRegular];self.crosshair.hidden=YES;
    self.crosshair.translatesAutoresizingMaskIntoConstraints=NO;[self.view addSubview:self.crosshair];
    UILayoutGuide *safe = self.view.safeAreaLayoutGuide;
    self.diagnosticLayout=@[
        [controls.bottomAnchor constraintEqualToAnchor:self.commandInput.topAnchor constant:-8],
        [controls.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [controls.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-12],
        [controls.heightAnchor constraintEqualToConstant:42],
        [self.hud.bottomAnchor constraintEqualToAnchor:controls.topAnchor constant:-8],
        [self.hud.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [self.hud.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-12],
    ];
    [NSLayoutConstraint activateConstraints:self.diagnosticLayout];
    self.gameLayout=@[
        [controls.topAnchor constraintEqualToAnchor:safe.topAnchor constant:8],
        [controls.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [controls.widthAnchor constraintEqualToConstant:120], [controls.heightAnchor constraintEqualToConstant:36],
        [self.hud.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor constant:-12],
        [self.hud.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [self.hud.trailingAnchor constraintLessThanOrEqualToAnchor:safe.trailingAnchor constant:-170]
    ];
    [NSLayoutConstraint activateConstraints:@[
        [self.crosshair.centerXAnchor constraintEqualToAnchor:safe.centerXAnchor],
        [self.crosshair.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor],
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
    // Gameplay controls occupy the right edge, leaving the left movement surface free.
    for(NSUInteger i=0;i<self.gameButtons.count;++i){UIButton *button=self.gameButtons[i];
        [NSLayoutConstraint activateConstraints:@[
            [button.widthAnchor constraintEqualToConstant:64], [button.heightAnchor constraintEqualToConstant:64],
            [button.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:(i%2==0?-16:-88)],
            [button.centerYAnchor constraintEqualToAnchor:safe.centerYAnchor constant:(CGFloat(i/2)-1)*76]
        ]];
    }
    self.diagnostics=[[UITextView alloc] init];self.diagnostics.editable=NO;
    self.diagnostics.backgroundColor=self.view.backgroundColor;self.diagnostics.textColor=[UIColor colorWithWhite:.8 alpha:1];
    self.diagnostics.font=[UIFont monospacedSystemFontOfSize:11 weight:UIFontWeightRegular];self.diagnostics.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:self.diagnostics];
    self.diagnosticLayout=[self.diagnosticLayout arrayByAddingObjectsFromArray:@[
        [self.diagnostics.topAnchor constraintEqualToAnchor:self.status.bottomAnchor constant:8],
        [self.diagnostics.bottomAnchor constraintEqualToAnchor:controls.topAnchor constant:-8],
        [self.diagnostics.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor constant:12],
        [self.diagnostics.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor constant:-12]
    ]];
    [NSLayoutConstraint activateConstraints:self.diagnosticLayout];
        NSURL *documents = [[[NSFileManager defaultManager] URLsForDirectory:NSDocumentDirectory
        inDomains:NSUserDomainMask] firstObject];
    _hostStarted = documents && _runtime.start(documents.path.UTF8String);
    if (!_hostStarted) {
        self.status.text = @"Startup failed. Export the log for diagnostics.";
        share.enabled = !_runtime.logPath().empty();
        return;
    }
    if (!_smokeRequested && !_runtime.executeSource("source_bsp_skins")) { [self fail:@"Skin/HDR/PHY prop demo failed"]; return; }
    if (!_smokeRequested && !_runtime.executeSource("source_model_load models/__source1ios_multimat_probe.mdl")) { [self fail:@"MDL48 multi-material demo failed"]; return; }
    if (!_smokeRequested && !_runtime.executeSource("source_model_skin 1")) { [self fail:@"MDL48 skin variant failed"]; return; }
    _runtime.log(std::string("iOS ") + UIDevice.currentDevice.systemVersion.UTF8String);
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) { [self fail:@"Metal device unavailable"]; return; }
    self.metalView = [[MTKView alloc] initWithFrame:CGRectZero device:device];
    self.metalView.translatesAutoresizingMaskIntoConstraints = NO;
    self.metalView.multipleTouchEnabled = YES;
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
#ifdef SOURCE_GAME_LINK
    self.status.text = @"Source 1 iOS · CS:S offline practice\nImport cm / cstrike / hl2 in Files, then Play\nSource self-tests: 122 PASS\nLeft move / right look";
#else
    self.status.text = @"Source 1 iOS · minimal milestone ~92%\nBSP bodygroups · ClientMod VPK\nSource self-tests: 118 PASS\nLeft move / right look";
#endif
    for(NSString *name in @[@"move",@"look"]){
        UIPanGestureRecognizer *pan=[[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(cameraPan:)];
        pan.name=name;pan.delegate=self;pan.maximumNumberOfTouches=1;[self.metalView addGestureRecognizer:pan];
    }
    self.metalView.delegate = self;
    self.metalView.paused = NO;
    [self showDiagnostics];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(pauseHost)
        name:UIApplicationWillResignActiveNotification object:nil];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(resumeHost)
        name:UIApplicationDidBecomeActiveNotification object:nil];
}
- (void)showDiagnostics {
    [NSLayoutConstraint deactivateConstraints:self.gameLayout];[NSLayoutConstraint activateConstraints:self.diagnosticLayout];
    self.metalView.hidden=!_smokeRequested;
    self.status.hidden=NO;self.diagnostics.hidden=_smokeRequested;
    self.commandInput.hidden=NO;self.shareButton.hidden=NO;self.runButton.hidden=NO;
    self.hud.hidden=YES;self.crosshair.hidden=YES;
    for(UIButton *button in self.gameButtons)button.hidden=YES;
    [self.play setTitle:@"Play" forState:UIControlStateNormal];
    self.status.text=@"Source 1 iOS · diagnostics\nPlay → local server / map selection";
    [self refreshDiagnostics];
}
- (void)refreshDiagnostics {
    if(self.diagnostics.hidden||_runtime.logPath().empty())return;
    // Read only the tail: imported cache diagnostics can grow to many megabytes.
    NSFileHandle *file=[NSFileHandle fileHandleForReadingAtPath:[NSString stringWithUTF8String:_runtime.logPath().c_str()]];
    if(!file)return;@try{unsigned long long size=[file seekToEndOfFile];[file seekToFileOffset:size>65536?size-65536:0];
        NSData *data=[file readDataToEndOfFile];NSString *text=[[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        if(!text)text=[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding];self.diagnostics.text=text;
    }@catch(NSException *exception){self.diagnostics.text=@"Use Share diagnostic log to export the full log.";}@finally{[file closeFile];}
}
- (void)closeMenu:(UIButton *)sender { [self.menu removeFromSuperview];self.menu=nil; }
- (void)togglePractice:(UIButton *)sender {
    [self.commandInput resignFirstResponder];_movement=CGPointZero;
    if(_runtime.playerState().active){_runtime.stopGame();[self showDiagnostics];_hasPrevious=NO;return;}
    NSMutableArray *names=[NSMutableArray array];for(const auto& map:_runtime.maps())[names addObject:[NSString stringWithUTF8String:map.c_str()]];self.mapNames=names;
    self.menu=[[UIView alloc] init];self.menu.backgroundColor=[UIColor colorWithRed:.07 green:.09 blue:.1 alpha:1];self.menu.translatesAutoresizingMaskIntoConstraints=NO;
    [self.view addSubview:self.menu];UILayoutGuide *safe=self.view.safeAreaLayoutGuide;
    [NSLayoutConstraint activateConstraints:@[[self.menu.topAnchor constraintEqualToAnchor:safe.topAnchor],[self.menu.bottomAnchor constraintEqualToAnchor:safe.bottomAnchor],[self.menu.leadingAnchor constraintEqualToAnchor:safe.leadingAnchor],[self.menu.trailingAnchor constraintEqualToAnchor:safe.trailingAnchor]]];
    UILabel *title=[[UILabel alloc] init];title.text=@"CLIENTMOD · LOCAL SERVER";title.textColor=UIColor.whiteColor;title.font=[UIFont boldSystemFontOfSize:22];title.translatesAutoresizingMaskIntoConstraints=NO;[self.menu addSubview:title];
    UIButton *back=[UIButton buttonWithType:UIButtonTypeSystem];[back setTitle:@"Back" forState:UIControlStateNormal];back.translatesAutoresizingMaskIntoConstraints=NO;[back addTarget:self action:@selector(closeMenu:) forControlEvents:UIControlEventTouchUpInside];[self.menu addSubview:back];
    UILabel *hint=[[UILabel alloc] init];hint.numberOfLines=0;hint.font=[UIFont systemFontOfSize:13];hint.textColor=UIColor.lightGrayColor;hint.translatesAutoresizingMaskIntoConstraints=NO;
    hint.text=names.count?@"Choose an installed map. Checked: awp_lego_2. Other maps are listed but still need compatibility testing.":@"No maps found. Import cm, cstrike, hl2, platform into Source1IOS/content using Files, then reopen this menu.";[self.menu addSubview:hint];
    self.mapTable=[[UITableView alloc] initWithFrame:CGRectZero style:UITableViewStylePlain];self.mapTable.backgroundColor=UIColor.clearColor;self.mapTable.delegate=self;self.mapTable.dataSource=self;self.mapTable.translatesAutoresizingMaskIntoConstraints=NO;[self.menu addSubview:self.mapTable];
    [NSLayoutConstraint activateConstraints:@[[title.topAnchor constraintEqualToAnchor:self.menu.topAnchor constant:16],[title.leadingAnchor constraintEqualToAnchor:self.menu.leadingAnchor constant:16],[title.trailingAnchor constraintLessThanOrEqualToAnchor:back.leadingAnchor constant:-8],[back.topAnchor constraintEqualToAnchor:self.menu.topAnchor constant:16],[back.trailingAnchor constraintEqualToAnchor:self.menu.trailingAnchor constant:-16],[hint.topAnchor constraintEqualToAnchor:title.bottomAnchor constant:12],[hint.leadingAnchor constraintEqualToAnchor:title.leadingAnchor],[hint.trailingAnchor constraintEqualToAnchor:self.menu.trailingAnchor constant:-16],[self.mapTable.topAnchor constraintEqualToAnchor:hint.bottomAnchor constant:12],[self.mapTable.leadingAnchor constraintEqualToAnchor:self.menu.leadingAnchor],[self.mapTable.trailingAnchor constraintEqualToAnchor:self.menu.trailingAnchor],[self.mapTable.bottomAnchor constraintEqualToAnchor:self.menu.bottomAnchor]]];
}
- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section { return self.mapNames.count; }
- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath {
    UITableViewCell *cell=[tableView dequeueReusableCellWithIdentifier:@"map"];
    if(!cell)cell=[[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle reuseIdentifier:@"map"];
    NSString *map=self.mapNames[indexPath.row];cell.textLabel.text=map;cell.textLabel.textColor=UIColor.whiteColor;cell.backgroundColor=UIColor.clearColor;
    cell.detailTextLabel.text=[map isEqualToString:@"awp_lego_2"]?@"Start local server · checked":@"Compatibility testing pending";cell.detailTextLabel.textColor=UIColor.lightGrayColor;return cell;
}
- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
    NSString *map=self.mapNames[indexPath.row];
    if(!_runtime.startGame(map.UTF8String)){
        UIAlertController *alert=[UIAlertController alertControllerWithTitle:@"Map unavailable" message:[NSString stringWithUTF8String:_runtime.gameError().c_str()] preferredStyle:UIAlertControllerStyleAlert];
        [alert addAction:[UIAlertAction actionWithTitle:@"OK" style:UIAlertActionStyleDefault handler:nil]];[self presentViewController:alert animated:YES completion:nil];return;
    }
    [self closeMenu:nil];[NSLayoutConstraint deactivateConstraints:self.diagnosticLayout];[NSLayoutConstraint activateConstraints:self.gameLayout];self.metalView.hidden=NO;self.status.hidden=YES;self.diagnostics.hidden=YES;
    self.commandInput.hidden=YES;self.runButton.hidden=YES;self.shareButton.hidden=YES;self.hud.hidden=NO;
    for(UIButton *button in self.gameButtons)button.hidden=NO;
    [self.play setTitle:@"Exit server" forState:UIControlStateNormal];_runtime.thirdPerson(_thirdPerson);_hasPrevious=NO;
}
- (void)togglePerspective:(UIButton *)sender {
    _thirdPerson=!_thirdPerson;_runtime.thirdPerson(_thirdPerson);
    [sender setTitle:_thirdPerson?@"1P":@"3P" forState:UIControlStateNormal];
}
- (void)pressPlayer:(UIButton *)sender { _runtime.playerButton(unsigned(sender.tag),true); }
- (void)releasePlayer:(UIButton *)sender { _runtime.playerButton(unsigned(sender.tag),false); }
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
    if(_runtime.frames()%60==0)[self refreshDiagnostics];
    if(_menuSmokeRequested&&_runtime.frames()==30){
        [self.view layoutIfNeeded];
        bool good=self.metalView.hidden&&!self.diagnostics.hidden&&!self.status.hidden&&!self.commandInput.hidden&&self.hud.hidden;
        for(UIButton *button in self.gameButtons)good=good&&button.hidden;
        _runtime.log(good?"iOS diagnostic UI checks: PASS":"iOS diagnostic UI checks: FAIL");
    }
    if(_menuSmokeRequested&&_runtime.frames()==120){
        [self togglePractice:self.play];[self.view layoutIfNeeded];
        const bool good=self.menu.window!=nil&&self.mapTable.window!=nil&&self.metalView.hidden&&!_runtime.playerState().active;
        _runtime.log(good?"iOS menu UI checks: PASS":"iOS menu UI checks: FAIL");
    }
    const auto& player=_runtime.playerState();self.crosshair.hidden=!player.active||_thirdPerson;
    if(player.active){
        NSString *weapon=[NSString stringWithUTF8String:player.weapon];
        self.hud.text=[NSString stringWithFormat:@"HP %d   Armor %d   $%d   Ammo %d / %d\n%@ · %.0f u/s · %@",player.health,player.armor,player.money,player.clip,player.reserve,weapon,std::hypot(player.velocity[0],player.velocity[1]),player.crouched?@"duck":player.grounded?@"ground":@"air"];
    }
    if (_smokeRequested && _runtime.frames() == 60) {
        if (!_runtime.executeSource("source_selftest") || !_runtime.executeSource("source_physics_reset")
            || !_runtime.executeSource("source_physics_impulse") || !_runtime.executeSource("source_bsp_terrain")
            || !_runtime.executeSource("source_bsp_materials")
            || !_runtime.executeSource("source_model_load models/__source1ios_external_probe.mdl") || !_runtime.executeSource("source_anim_play 0")) {
            [self fail:@"Simulator runtime contracts FAIL"]; return;
        }
        const auto root=_runtime.logPath().parent_path();std::error_code error;
        std::filesystem::create_directories(root/"content/smoke/maps",error);std::filesystem::create_directories(root/"content/smoke/packed",error);
        if(!error)std::filesystem::copy_file(root/"selftest/__source1ios_material_grid.bsp",root/"content/smoke/maps/cache_smoke.bsp",std::filesystem::copy_options::overwrite_existing,error);
        if(!error)std::filesystem::copy_file(root/"selftest/fixture2_dir.vpk",root/"content/smoke/packed/cache_dir.vpk",std::filesystem::copy_options::overwrite_existing,error);
        if(error || !_runtime.executeSource("source_content_mount smoke") || !_runtime.executeSource("source_content_selftest") || !_runtime.executeSource("source_bsp_load maps/cache_smoke.bsp")
            || !_runtime.executeSource("source_content_unmount smoke")){[self fail:@"Simulator content import FAIL"];return;}
        if (!_runtime.executeSource("source_bsp_props")) { [self fail:@"Simulator static props FAIL"]; return; }
        if (!_runtime.executeSource("source_physics_reset") || !_runtime.executeSource("source_props_selftest")) { [self fail:@"Simulator static prop collision FAIL"]; return; }
        if (!_runtime.executeSource("source_bsp_phy") || !_runtime.executeSource("source_physics_reset") || !_runtime.executeSource("source_phy_selftest")) { [self fail:@"Simulator exact PHY collision FAIL"]; return; }
        if (!_runtime.executeSource("source_bsp_hdr") || !_runtime.executeSource("source_hdr_selftest")) { [self fail:@"Simulator HDR-only scene FAIL"]; return; }
        if (!_runtime.executeSource("source_bsp_entities") || !_runtime.executeSource("source_entities_selftest")) { [self fail:@"Simulator entity model scene FAIL"]; return; }
        if (!_runtime.executeSource("source_bsp_skins") || !_runtime.executeSource("source_physics_reset") || !_runtime.executeSource("source_props_skin_selftest")) { [self fail:@"Simulator static prop skins FAIL"]; return; }
        if (!_runtime.executeSource("source_model_load models/__source1ios_external48_probe.mdl") || !_runtime.executeSource("source_anim_play 0")) { [self fail:@"Simulator MDL48 FAIL"]; return; }
        if (!_runtime.executeSource("source_model_load models/__source1ios_multimat_probe.mdl") || !_runtime.executeSource("source_anim_play 0") || !_runtime.executeSource("source_model_materials_selftest")) { [self fail:@"Simulator multi-material model FAIL"]; return; }
        if (!_runtime.executeSource("source_model_skin 1") || !_runtime.executeSource("source_skin_selftest")) { [self fail:@"Simulator animated skin variant FAIL"]; return; }
        _runtime.cameraLook(10, 0);
        _runtime.cameraMove(1, 0, .1f);
        _runtime.executeSource("source_camera_reset");
    }
    if (_smokeRequested && _runtime.frames() == 180) _runtime.log("Source simulator runtime contracts: PASS");
    if(view.hidden)return; // Diagnostics do not draw the hidden fixture scene.
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
        self.mapTexture=staged;self.lightmapTexture=stagedLight;_mapTextureRevision=_runtime.textureRevision();_runtime.log("Source BSP VMT/VTF base texture uploaded to Metal");_runtime.log("Source BSP preview lightmap atlas uploaded to Metal");
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
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gesture shouldReceiveTouch:(UITouch *)touch {
    const BOOL left=[touch locationInView:self.metalView].x<self.metalView.bounds.size.width/2;
    return [gesture.name isEqualToString:@"move"]?left:!left;
}
- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gesture shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)other { return YES; }
- (void)cameraPan:(UIPanGestureRecognizer *)gesture {
    const BOOL movement=[gesture.name isEqualToString:@"move"];
    if(gesture.state==UIGestureRecognizerStateEnded||gesture.state==UIGestureRecognizerStateCancelled||gesture.state==UIGestureRecognizerStateFailed){
        if(movement)_movement=CGPointZero;return;
    }
    CGPoint delta=[gesture translationInView:self.metalView];
    if(movement)_movement=CGPointMake(MAX(-1,MIN(1,delta.x/70)),MAX(-1,MIN(1,-delta.y/70)));
    else{_runtime.cameraLook((float)-delta.x*.18f,(float)delta.y*.18f);[gesture setTranslation:CGPointZero inView:self.metalView];}
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField {
    [self runCommand:nil];
    return YES;
}
- (void)runCommand:(UIButton *)sender {
    NSString *command = self.commandInput.text ?: @"";
    [self.commandInput resignFirstResponder];
    BOOL accepted = _runtime.executeSource(command.UTF8String);
    self.status.text = [NSString stringWithFormat:@"Source 1 iOS · CS:S offline practice\n%@: %@", accepted ? @"Executed" : @"Rejected", command];
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
