#import "NativeHost.h"
#import <QuartzCore/QuartzCore.h>
#include <unistd.h>
#include <string.h>

static __weak CFGameController *gameController;
static BOOL gameSuspended;
NSString *CFStoragePath(void) { return [NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,NSUserDomainMask,YES).firstObject stringByAppendingPathComponent:@"Clayfight"]; }
NSString *CFCreaturesPath(void) { return [CFStoragePath() stringByAppendingPathComponent:@"creatures"]; }
void CFSuspendGame(BOOL value) { gameSuspended=value;G_ClearInput(); }
void CFPresentCreator(void) {
    if(gameController.presentedViewController)return;
    CFSuspendGame(YES);
    CFCaptureController *capture=[CFCaptureController new];capture.modalPresentationStyle=UIModalPresentationFullScreen;
    [gameController presentViewController:capture animated:YES completion:nil];
}
@interface CFGameController ()
@property UIImageView *frameView;
@property CADisplayLink *displayLink;
@property CFTimeInterval lastFrame;
@property double accumulator;
@property BOOL initialized, ranQA;
@property NSMutableArray<UIButton *> *controls;
@end
void CFGameFrame(const pixel_t *pixels) {
    NSData *data=[NSData dataWithBytes:pixels length:SCREENWIDTH*SCREENHEIGHT*4];
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)data);CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(SCREENWIDTH,SCREENHEIGHT,8,32,SCREENWIDTH*4,space,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,NULL,false,kCGRenderingIntentDefault);
    gameController.frameView.image=[UIImage imageWithCGImage:image];CGImageRelease(image);CGDataProviderRelease(provider);CGColorSpaceRelease(space);
}
@implementation CFGameController
- (void)viewDidLoad {
    [super viewDidLoad];gameController=self;self.view.backgroundColor=UIColor.blackColor;self.view.multipleTouchEnabled=YES;self.controls=[NSMutableArray array];
    self.frameView=[UIImageView new];self.frameView.contentMode=UIViewContentModeScaleAspectFit;self.frameView.layer.magnificationFilter=kCAFilterNearest;self.frameView.layer.minificationFilter=kCAFilterNearest;[self.view addSubview:self.frameView];
    NSArray *labels=@[@"↑",@"↓",@"←",@"→",@"HQ",@"HF",@"LQ",@"LF",@"ВЫБРАТЬ",@"НАЗАД",@"МЕНЮ"];
    int keys[]={'w','s','a','d','r','t','f','g',KEY_ENTER,KEY_ESCAPE,'q'};
    for(int i=0;i<11;i++){
        UIButton *b=[UIButton buttonWithType:UIButtonTypeSystem];b.tag=keys[i];[b setTitle:labels[i] forState:UIControlStateNormal];b.accessibilityLabel=labels[i];
        b.backgroundColor=[UIColor colorWithWhite:.11 alpha:.82];b.tintColor=[UIColor colorWithRed:.95 green:.75 blue:.39 alpha:1];b.layer.cornerRadius=12;b.layer.borderWidth=1;b.layer.borderColor=[UIColor colorWithWhite:.5 alpha:.4].CGColor;b.titleLabel.font=[UIFont systemFontOfSize:i<4?26:13 weight:UIFontWeightSemibold];
        [b addTarget:self action:@selector(down:) forControlEvents:UIControlEventTouchDown];[b addTarget:self action:@selector(up:) forControlEvents:UIControlEventTouchUpInside|UIControlEventTouchUpOutside|UIControlEventTouchCancel];[self.view addSubview:b];[self.controls addObject:b];
    }
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(inactive) name:UIApplicationWillResignActiveNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(active) name:UIApplicationDidBecomeActiveNotification object:nil];
}
- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    if(!self.initialized){self.initialized=YES;[self initializeGame];}
    CFSuspendGame(NO);self.lastFrame=0;
#ifdef DEBUG
    if(!self.ranQA){
        self.ranQA=YES;NSArray *arguments=NSProcessInfo.processInfo.arguments;
        if([arguments containsObject:@"--native-checks"]){extern void CFRunNativeChecks(void);CFRunNativeChecks();}
        if([arguments containsObject:@"--capture"]){dispatch_async(dispatch_get_main_queue(),^{M_Create();});}
    }
#endif
    if(!self.displayLink){self.displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(tick:)];self.displayLink.preferredFrameRateRange=CAFrameRateRangeMake(60,60,60);[self.displayLink addToRunLoop:NSRunLoop.mainRunLoop forMode:NSRunLoopCommonModes];}
}
- (void)initializeGame {
    NSFileManager *fm=NSFileManager.defaultManager;NSString *root=CFStoragePath();[fm createDirectoryAtPath:root withIntermediateDirectories:YES attributes:nil error:nil];
    [fm createDirectoryAtPath:[root stringByAppendingPathComponent:@"build"] withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *bundled=[NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"GameData"];
    NSString *defaults=[bundled stringByAppendingPathComponent:@"creatures"];
    [fm createDirectoryAtPath:CFCreaturesPath() withIntermediateDirectories:YES attributes:nil error:nil];
    for(NSString *name in [fm contentsOfDirectoryAtPath:defaults error:nil]){
        NSString *destination=[CFCreaturesPath() stringByAppendingPathComponent:name];
        if(![fm fileExistsAtPath:destination])[fm copyItemAtPath:[defaults stringByAppendingPathComponent:name] toPath:destination error:nil];
    }
    chdir(root.fileSystemRepresentation);
    static char *args[7];args[0]="clayfight";args[1]="-data";args[2]=strdup([bundled stringByAppendingPathComponent:@"primalrage"].fileSystemRepresentation);args[3]="-creatures";args[4]=strdup(CFCreaturesPath().fileSystemRepresentation);args[5]="-warp";args[6]="menu";
    @try { D_InitGame(7,args); }
    @catch(NSException *e){UIAlertController *a=[UIAlertController alertControllerWithTitle:@"Не удалось загрузить игру" message:e.reason preferredStyle:UIAlertControllerStyleAlert];[a addAction:[UIAlertAction actionWithTitle:@"Закрыть" style:UIAlertActionStyleCancel handler:nil]];[self presentViewController:a animated:YES completion:nil];self.initialized=NO;}
}
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];UIEdgeInsets safe=self.view.safeAreaInsets;CGRect bounds=UIEdgeInsetsInsetRect(self.view.bounds,safe);BOOL wide=bounds.size.width>bounds.size.height;
    CGFloat size=MIN(wide?54:58,bounds.size.width/7),gap=5,y=CGRectGetMaxY(bounds)-size*2-gap-8,left=bounds.origin.x+12,right=CGRectGetMaxX(bounds)-size*2-gap-12;
    CGPoint p[]={{left+size+gap,y},{left+size+gap,y+size+gap},{left,y+size+gap},{left+2*(size+gap),y+size+gap},{right,y},{right+size+gap,y},{right,y+size+gap},{right+size+gap,y+size+gap}};
    for(int i=0;i<8;i++)self.controls[i].frame=CGRectMake(p[i].x,p[i].y,size,size);
    CGFloat top=bounds.origin.y+6;self.controls[8].frame=CGRectMake(CGRectGetMidX(bounds)-54,(wide?CGRectGetMaxY(bounds)-48:y-50),108,40);
    self.controls[9].frame=CGRectMake(bounds.origin.x+8,top,82,36);self.controls[10].frame=CGRectMake(CGRectGetMaxX(bounds)-90,top,82,36);
    CGRect content=CGRectMake(bounds.origin.x,top+42,bounds.size.width,MAX(100,y-top-48));
    if(wide)content=CGRectInset(bounds,50,0);self.frameView.frame=content;
}
- (void)down:(UIButton *)button {
    if(button.tag=='q'){C_Abandon();endurance=false;M_StartMenu();return;}
    event_t e={ev_keydown,(int)button.tag};D_PostEvent(&e);
}
- (void)up:(UIButton *)button {event_t e={ev_keyup,(int)button.tag};D_PostEvent(&e);}
- (void)tick:(CADisplayLink *)link {
    if(!self.initialized||gameSuspended||self.presentedViewController){self.lastFrame=0;return;}
    if(!self.lastFrame)self.lastFrame=link.timestamp;
    self.accumulator+=MIN(link.timestamp-self.lastFrame,.133);self.lastFrame=link.timestamp;
    @try {while(self.accumulator>=1.0/60){D_RunTic();self.accumulator-=1.0/60;}D_Display();}
    @catch(NSException *e){self.displayLink.paused=YES;NSLog(@"Game error: %@",e);}
}
- (void)inactive { if(gamestate==GS_FIGHT)paused=true;G_ClearInput();self.displayLink.paused=YES; }
- (void)active { self.lastFrame=0;self.accumulator=0;self.displayLink.paused=NO; }
- (BOOL)prefersStatusBarHidden { return YES; }
@end
@interface CFSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property(nonatomic,strong) UIWindow *window;
@end
@implementation CFSceneDelegate
- (void)scene:(UIScene *)scene willConnectToSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    if(![scene isKindOfClass:UIWindowScene.class])return;
    self.window=[[UIWindow alloc]initWithWindowScene:(UIWindowScene *)scene];self.window.rootViewController=[CFGameController new];[self.window makeKeyAndVisible];
}
@end
@interface CFAppDelegate : UIResponder <UIApplicationDelegate>
@end
@implementation CFAppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)options {return YES;}
- (UISceneConfiguration *)application:(UIApplication *)application configurationForConnectingSceneSession:(UISceneSession *)session options:(UISceneConnectionOptions *)options {
    UISceneConfiguration *configuration=[[UISceneConfiguration alloc]initWithName:@"Default" sessionRole:session.role];configuration.delegateClass=CFSceneDelegate.class;return configuration;
}
@end
int main(int argc,char **argv){@autoreleasepool{return UIApplicationMain(argc,argv,nil,NSStringFromClass(CFAppDelegate.class));}}
