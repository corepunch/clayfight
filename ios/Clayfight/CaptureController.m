#import "NativeHost.h"
#import "PhotoProcessing.h"
#import "PoseGuideView.h"
#import <AVFoundation/AVFoundation.h>
#import <AVKit/AVKit.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

static UIColor *CFGold(void){return [UIColor colorWithRed:.92 green:.69 blue:.35 alpha:1];}
static NSString *CFDraft(void){return [CFStoragePath() stringByAppendingPathComponent:@"capture-draft"];}
static NSDictionary *CFDefaultAlignment(void){return @{@"zoom":@100,@"shift":@0,@"threshold":@38,@"headWidth":@0};}
static UIButton *CFButton(NSString *title,id target,SEL action){UIButton *b=[UIButton buttonWithType:UIButtonTypeSystem];[b setTitle:title forState:UIControlStateNormal];b.tintColor=CFGold();b.backgroundColor=[UIColor colorWithWhite:.16 alpha:1];b.layer.cornerRadius=10;b.titleLabel.font=[UIFont systemFontOfSize:15 weight:UIFontWeightSemibold];[b.heightAnchor constraintGreaterThanOrEqualToConstant:48].active=YES;[b addTarget:target action:action forControlEvents:UIControlEventTouchUpInside];return b;}

@interface CFCaptureController () <AVCapturePhotoCaptureDelegate,AVCaptureFileOutputRecordingDelegate,PHPickerViewControllerDelegate,UITextFieldDelegate>
@property NSArray<NSDictionary *> *poses;
@property NSMutableDictionary *frames;
@property NSInteger step;
@property UITextField *nameField;
@property UISegmentedControl *mode;
@property UILabel *stepLabel,*hint,*status,*headStatus;
@property UIView *preview;
@property UIImageView *imageView;
@property CFGuideView *guide;
@property UISlider *zoom,*shift,*threshold,*opacity;
@property UIButton *cameraButton,*photoButton,*videoButton,*acceptButton,*publishButton,*closeButton,*headButton;
@property UIStackView *stack,*steps;
@property UIScrollView *scroll;
@property AVCaptureSession *session;
@property AVCapturePhotoOutput *photoOutput;
@property AVCaptureMovieFileOutput *movieOutput;
@property AVCaptureVideoPreviewLayer *previewLayer;
@property dispatch_queue_t sessionQueue;
@property UIImage *currentImage;
@property NSData *originalData;
@property NSMutableDictionary *alignment;
@property NSString *pendingVideo;
@property NSMutableArray<NSValue *> *headPoints;
@property(nonatomic) BOOL permissionRequest,busy;
@property(atomic) BOOL closed;
@property NSUInteger revision;
@end
@implementation CFCaptureController
- (UILabel *)label:(NSString *)text size:(CGFloat)size {UILabel *l=[UILabel new];l.text=text;l.font=[UIFont systemFontOfSize:size weight:size>20?UIFontWeightBold:UIFontWeightRegular];l.textColor=UIColor.whiteColor;l.numberOfLines=0;return l;}
- (UIStackView *)row:(NSArray<UIView *> *)views {UIStackView *s=[[UIStackView alloc]initWithArrangedSubviews:views];s.spacing=8;s.distribution=UIStackViewDistributionFillEqually;return s;}
- (UISlider *)slider:(NSString *)title min:(float)min max:(float)max value:(float)value action:(SEL)action {
    [self.stack addArrangedSubview:[self label:title size:13]];UISlider *s=[UISlider new];s.minimumValue=min;s.maximumValue=max;s.value=value;s.tintColor=CFGold();[s addTarget:self action:action forControlEvents:UIControlEventValueChanged];[self.stack addArrangedSubview:s];return s;
}
- (void)viewDidLoad {
    [super viewDidLoad];self.overrideUserInterfaceStyle=UIUserInterfaceStyleDark;self.view.backgroundColor=[UIColor colorWithRed:.08 green:.075 blue:.06 alpha:1];self.sessionQueue=dispatch_queue_create("clayfight.camera",DISPATCH_QUEUE_SERIAL);
    NSURL *posesURL=[NSBundle.mainBundle URLForResource:@"poses" withExtension:@"json" subdirectory:@"data"];self.poses=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfURL:posesURL] options:0 error:nil];self.frames=[NSMutableDictionary dictionary];
    [[NSFileManager defaultManager] createDirectoryAtPath:[CFDraft() stringByAppendingPathComponent:@"sources"] withIntermediateDirectories:YES attributes:nil error:nil];
    UIScrollView *scroll=[UIScrollView new];self.scroll=scroll;scroll.translatesAutoresizingMaskIntoConstraints=NO;scroll.keyboardDismissMode=UIScrollViewKeyboardDismissModeOnDrag;[self.view addSubview:scroll];
    self.stack=[UIStackView new];self.stack.axis=UILayoutConstraintAxisVertical;self.stack.spacing=14;self.stack.translatesAutoresizingMaskIntoConstraints=NO;[scroll addSubview:self.stack];
    [NSLayoutConstraint activateConstraints:@[[scroll.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],[scroll.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor],[scroll.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],[scroll.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [self.stack.topAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.topAnchor constant:20],[self.stack.bottomAnchor constraintEqualToAnchor:scroll.contentLayoutGuide.bottomAnchor constant:-24],
        [self.stack.centerXAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.centerXAnchor],[self.stack.widthAnchor constraintEqualToAnchor:scroll.frameLayoutGuide.widthAnchor constant:-32]]];
    self.closeButton=CFButton(@"Закрыть",self,@selector(close));[self.stack addArrangedSubview:[self row:@[[self label:@"Создать бойца" size:27],self.closeButton]]];
    self.nameField=[UITextField new];self.nameField.placeholder=@"Имя на арене: GROM";self.nameField.borderStyle=UITextBorderStyleRoundedRect;self.nameField.autocapitalizationType=UITextAutocapitalizationTypeAllCharacters;self.nameField.autocorrectionType=UITextAutocorrectionTypeNo;self.nameField.keyboardType=UIKeyboardTypeASCIICapable;self.nameField.delegate=self;self.nameField.accessibilityIdentifier=@"fighter-name";[self.stack addArrangedSubview:self.nameField];
    self.mode=[[UISegmentedControl alloc]initWithItems:@[@"На подставке",@"С рук"]];self.mode.selectedSegmentIndex=0;[self.mode addTarget:self action:@selector(modeChanged) forControlEvents:UIControlEventValueChanged];[self.stack addArrangedSubview:self.mode];
    self.stepLabel=[self label:@"" size:23];self.stepLabel.accessibilityIdentifier=@"capture-step";[self.stack addArrangedSubview:self.stepLabel];
    self.preview=[UIView new];self.preview.backgroundColor=[UIColor colorWithWhite:.18 alpha:1];self.preview.clipsToBounds=YES;self.preview.layer.cornerRadius=12;self.preview.accessibilityIdentifier=@"native-camera-preview";[self.stack addArrangedSubview:self.preview];[self.preview.heightAnchor constraintEqualToAnchor:self.preview.widthAnchor multiplier:.75].active=YES;
    self.imageView=[UIImageView new];self.imageView.contentMode=UIViewContentModeScaleAspectFit;[self.preview addSubview:self.imageView];self.guide=[CFGuideView new];self.guide.backgroundColor=UIColor.clearColor;self.guide.alpha=.72;self.guide.userInteractionEnabled=NO;[self.preview addSubview:self.guide];[self.preview addGestureRecognizer:[[UITapGestureRecognizer alloc]initWithTarget:self action:@selector(markTap:)]];
    self.hint=[self label:@"" size:15];[self.stack addArrangedSubview:self.hint];
    self.cameraButton=CFButton(@"Включить камеру",self,@selector(startCamera));self.photoButton=CFButton(@"Снимок",self,@selector(takePhoto));self.videoButton=CFButton(@"Видео · 3 сек",self,@selector(recordVideo));[self.stack addArrangedSubview:[self row:@[self.cameraButton,self.photoButton]]];[self.stack addArrangedSubview:[self row:@[self.videoButton,CFButton(@"Выбрать фото / видео",self,@selector(importMedia))]]];
    self.headButton=CFButton(@"Отметить ширину головы",self,@selector(markHead));[self.stack addArrangedSubview:self.headButton];self.headStatus=[self label:@"" size:13];[self.stack addArrangedSubview:self.headStatus];
    self.opacity=[self slider:@"Видимость силуэта" min:0 max:1 value:.72 action:@selector(opacityChanged)];
    self.zoom=[self slider:@"Масштаб" min:50 max:200 value:100 action:@selector(alignmentChanged)];self.shift=[self slider:@"Влево / вправо" min:-120 max:120 value:0 action:@selector(alignmentChanged)];self.threshold=[self slider:@"Отделение фона" min:12 max:100 value:38 action:@selector(alignmentChanged)];
    [self.stack addArrangedSubview:[self row:@[CFButton(@"Переснять",self,@selector(retake)),CFButton(@"Исходник",self,@selector(showOriginal))]]];
    self.status=[self label:@"" size:14];self.status.textColor=CFGold();self.status.accessibilityIdentifier=@"capture-status";[self.stack addArrangedSubview:self.status];
    self.acceptButton=CFButton(@"Сохранить позу →",self,@selector(acceptPose));self.acceptButton.accessibilityIdentifier=@"accept-pose";[self.stack addArrangedSubview:self.acceptButton];
    [self.stack addArrangedSubview:[self row:@[CFButton(@"← Назад",self,@selector(previousPose)),CFButton(@"Следующая →",self,@selector(nextPose))]]];
    self.steps=[UIStackView new];self.steps.axis=UILayoutConstraintAxisVertical;self.steps.spacing=6;[self.stack addArrangedSubview:self.steps];
    for(NSInteger i=0;i<self.poses.count;i++){UIButton *b=CFButton(self.poses[i][@"title"],self,@selector(selectPose:));b.tag=i;[self.steps addArrangedSubview:b];}
    self.publishButton=CFButton(@"Добавить бойца в игру",self,@selector(publish));self.publishButton.accessibilityIdentifier=@"publish-fighter";[self.stack addArrangedSubview:self.publishButton];
    [self.stack addArrangedSubview:[self label:@"Фото, видео и черновик хранятся на этом устройстве. Однотонный контрастный фон и мягкий свет помогут сохранить пальцы, глаза и хвост." size:13]];
    NSDictionary *draft=[NSDictionary dictionaryWithContentsOfFile:[CFDraft() stringByAppendingPathComponent:@"draft.plist"]];
    if(draft){self.frames=[draft[@"frames"] mutableCopy]?:[NSMutableDictionary dictionary];self.nameField.text=draft[@"name"];self.mode.selectedSegmentIndex=[draft[@"handheld"] boolValue]?1:0;for(NSInteger i=0;i<self.poses.count;i++)if(!self.frames[self.poses[i][@"id"]]){self.step=i;break;}}
    [self enterStep:self.step];
    [[NSNotificationCenter defaultCenter]addObserver:self selector:@selector(background) name:UIApplicationWillResignActiveNotification object:nil];
}
- (void)viewDidLayoutSubviews {[super viewDidLayoutSubviews];self.imageView.frame=self.preview.bounds;self.guide.frame=self.preview.bounds;self.previewLayer.frame=self.preview.bounds;[self updateRotation];}
- (void)updateRotation {
    UIInterfaceOrientation o=self.view.window.windowScene.interfaceOrientation;CGFloat angle=o==UIInterfaceOrientationLandscapeLeft?180:o==UIInterfaceOrientationLandscapeRight?0:o==UIInterfaceOrientationPortraitUpsideDown?270:90;
    AVCaptureConnection *connection=self.previewLayer.connection;if([connection isVideoRotationAngleSupported:angle])connection.videoRotationAngle=angle;
}
- (void)startCamera {
    [self.view endEditing:YES];if(self.session.running){self.imageView.hidden=YES;self.status.text=@"Силуэт показывает нужную позу. Уберите руки из кадра.";return;}
    AVAuthorizationStatus auth=[AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if(auth==AVAuthorizationStatusNotDetermined){if(self.permissionRequest)return;self.permissionRequest=YES;[AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted){dispatch_async(dispatch_get_main_queue(),^{self.permissionRequest=NO;if(!self.closed)[self startCamera];});}];return;}
    if(auth!=AVAuthorizationStatusAuthorized){self.status.text=@"Разрешите камеру для Clayfight в Настройках. Или выберите готовые фотографии.";return;}
    if(self.busy)return;self.busy=YES;
    if(self.session){dispatch_async(self.sessionQueue,^{if(!self.closed)[self.session startRunning];dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;self.imageView.hidden=YES;self.status.text=@"Камера готова.";});});return;}
    dispatch_async(self.sessionQueue,^{
        AVCaptureSession *s=[AVCaptureSession new];s.sessionPreset=AVCaptureSessionPresetHigh;s.automaticallyConfiguresApplicationAudioSession=NO;
        AVCaptureDevice *device=[AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera mediaType:AVMediaTypeVideo position:AVCaptureDevicePositionBack];NSError *error;
        AVCaptureDeviceInput *input=device?[AVCaptureDeviceInput deviceInputWithDevice:device error:&error]:nil;
        if(!input||![s canAddInput:input]){dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;self.status.text=@"Камера недоступна. В симуляторе выберите фотографию; съёмка доступна на iPhone и iPad.";});return;}
        [s addInput:input];self.photoOutput=[AVCapturePhotoOutput new];self.movieOutput=[AVCaptureMovieFileOutput new];self.movieOutput.maxRecordedDuration=CMTimeMake(3,1);
        if([s canAddOutput:self.photoOutput])[s addOutput:self.photoOutput];if([s canAddOutput:self.movieOutput])[s addOutput:self.movieOutput];self.session=s;
        dispatch_async(dispatch_get_main_queue(),^{self.previewLayer=[AVCaptureVideoPreviewLayer layerWithSession:s];self.previewLayer.videoGravity=AVLayerVideoGravityResizeAspect;self.previewLayer.frame=self.preview.bounds;[self.preview.layer insertSublayer:self.previewLayer atIndex:0];[self updateRotation];self.imageView.hidden=YES;});
        if(!self.closed)[s startRunning];dispatch_async(dispatch_get_main_queue(),^{self.busy=NO;self.status.text=@"Камера готова. Снимите текущую позу под силуэтом.";});
    });
}
- (void)alignOutput:(AVCaptureOutput *)output {
    AVCaptureConnection *c=[output connectionWithMediaType:AVMediaTypeVideo];CGFloat angle=self.previewLayer.connection.videoRotationAngle;if([c isVideoRotationAngleSupported:angle])c.videoRotationAngle=angle;
}
- (void)setBusy:(BOOL)value {_busy=value;self.closeButton.enabled=self.photoButton.enabled=self.videoButton.enabled=self.cameraButton.enabled=!value;self.acceptButton.enabled=!value&&self.currentImage!=nil;self.publishButton.enabled=!value&&self.frames.count==self.poses.count;self.mode.enabled=!value;self.steps.userInteractionEnabled=!value;}
- (void)takePhoto {
    if(self.busy)return;if(!self.session.running){[self startCamera];return;}[self alignOutput:self.photoOutput];[self setBusy:YES];
    AVCapturePhotoSettings *settings=[AVCapturePhotoSettings photoSettingsWithFormat:@{AVVideoCodecKey:AVVideoCodecTypeJPEG}];settings.flashMode=AVCaptureFlashModeOff;[self.photoOutput capturePhotoWithSettings:settings delegate:self];
}
- (void)captureOutput:(AVCapturePhotoOutput *)output didFinishProcessingPhoto:(AVCapturePhoto *)photo error:(NSError *)error {
    NSData *data=photo.fileDataRepresentation;dispatch_async(dispatch_get_main_queue(),^{[self setBusy:NO];if(error||!data){self.status.text=error.localizedDescription?:@"Не удалось сделать снимок";return;}self.pendingVideo=nil;[self usePhoto:data];});
}
- (void)recordVideo {
    if(self.busy)return;if(!self.session.running){[self startCamera];return;}[self alignOutput:self.movieOutput];[self setBusy:YES];self.status.text=@"Запись · 3 секунды. Удерживайте позу и уберите руки.";
    NSString *path=[CFDraft() stringByAppendingPathComponent:[NSString stringWithFormat:@"record-%@.mov",NSUUID.UUID.UUIDString]];[self.movieOutput startRecordingToOutputFileURL:[NSURL fileURLWithPath:path] recordingDelegate:self];
}
- (void)captureOutput:(AVCaptureFileOutput *)output didFinishRecordingToOutputFileAtURL:(NSURL *)url fromConnections:(NSArray<AVCaptureConnection *> *)connections error:(NSError *)error {
    BOOL success=!error||[error.userInfo[AVErrorRecordingSuccessfullyFinishedKey]boolValue];
    dispatch_async(dispatch_get_main_queue(),^{if(self.closed)return;if(!success){[self setBusy:NO];self.status.text=error.localizedDescription;return;}[self useVideo:url time:CMTimeMake(2,1)];});
}
- (void)useVideo:(NSURL *)url time:(CMTime)time {
    [self setBusy:YES];AVAssetImageGenerator *generator=[[AVAssetImageGenerator alloc]initWithAsset:[AVURLAsset URLAssetWithURL:url options:nil]];generator.appliesPreferredTrackTransform=YES;
    [generator generateCGImageAsynchronouslyForTime:time completionHandler:^(CGImageRef image,CMTime actual,NSError *error){
        UIImage *photo=image?[UIImage imageWithCGImage:image]:nil;
        dispatch_async(dispatch_get_main_queue(),^{[self setBusy:NO];if(!photo){self.status.text=error.localizedDescription?:@"Не удалось выбрать кадр видео";return;}self.pendingVideo=url.path;[self usePhoto:UIImageJPEGRepresentation(photo,.95)];});
    }];
}
- (void)importMedia {if(self.busy)return;PHPickerConfiguration *config=[[PHPickerConfiguration alloc]init];config.selectionLimit=1;config.filter=[PHPickerFilter anyFilterMatchingSubfilters:@[PHPickerFilter.imagesFilter,PHPickerFilter.videosFilter]];PHPickerViewController *picker=[[PHPickerViewController alloc]initWithConfiguration:config];picker.delegate=self;[self presentViewController:picker animated:YES completion:nil];}
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray<PHPickerResult *> *)results {
    [picker dismissViewControllerAnimated:YES completion:nil];NSItemProvider *provider=results.firstObject.itemProvider;if(!provider)return;
    if([provider hasItemConformingToTypeIdentifier:UTTypeMovie.identifier]){
        [self setBusy:YES];[provider loadFileRepresentationForTypeIdentifier:UTTypeMovie.identifier completionHandler:^(NSURL *url,NSError *error){
            NSString *path=[CFDraft() stringByAppendingPathComponent:[NSString stringWithFormat:@"import-%@.mov",NSUUID.UUID.UUIDString]];NSError *copyError;if(url)[NSFileManager.defaultManager copyItemAtURL:url toURL:[NSURL fileURLWithPath:path] error:&copyError];
            dispatch_async(dispatch_get_main_queue(),^{[self setBusy:NO];if(!url||copyError){self.status.text=copyError.localizedDescription?:error.localizedDescription;return;}
                AVPlayerViewController *video=[AVPlayerViewController new];video.player=[AVPlayer playerWithURL:[NSURL fileURLWithPath:path]];
                UINavigationController *navigation=[[UINavigationController alloc]initWithRootViewController:video];video.navigationItem.title=@"Выберите момент видео";__weak AVPlayerViewController *weakVideo=video;__weak UINavigationController *weakNavigation=navigation;
                video.navigationItem.rightBarButtonItem=[[UIBarButtonItem alloc]initWithPrimaryAction:[UIAction actionWithTitle:@"Взять кадр" image:nil identifier:nil handler:^(UIAction *action){CMTime time=weakVideo.player.currentTime;[weakVideo.player pause];[weakNavigation dismissViewControllerAnimated:YES completion:^{[self useVideo:[NSURL fileURLWithPath:path] time:time];}];}] menu:nil];[self presentViewController:navigation animated:YES completion:nil];
            });
        }];
    }else if([provider canLoadObjectOfClass:UIImage.class]){[self setBusy:YES];[provider loadObjectOfClass:UIImage.class completionHandler:^(id<NSItemProviderReading> object,NSError *error){dispatch_async(dispatch_get_main_queue(),^{[self setBusy:NO];self.pendingVideo=nil;if([object isKindOfClass:UIImage.class])[self usePhoto:UIImageJPEGRepresentation((UIImage *)object,.95)];else self.status.text=error.localizedDescription;});}];}
}
- (void)usePhoto:(NSData *)data {UIImage *image=[UIImage imageWithData:data];if(!image){self.status.text=@"Не удалось прочитать фотографию";return;}self.originalData=data;self.currentImage=CFCanonicalPhoto(image);self.alignment=[CFDefaultAlignment() mutableCopy];self.headPoints=nil;self.zoom.value=100;self.shift.value=0;self.threshold.value=38;self.headStatus.text=@"";[self render];}
- (void)enterStep:(NSInteger)step {
    if(self.busy)return;self.step=MAX(0,MIN(step,(NSInteger)self.poses.count-1));self.revision++;NSDictionary *pose=self.poses[self.step];self.stepLabel.text=[NSString stringWithFormat:@"%ld / %lu · %@",(long)self.step+1,(unsigned long)self.poses.count,pose[@"title"]];self.hint.text=pose[@"hint"];self.guide.pose=pose[@"id"];[self.guide setNeedsDisplay];self.headPoints=nil;self.pendingVideo=nil;
    NSDictionary *frame=self.frames[pose[@"id"]];self.pendingVideo=frame[@"video"]?[CFDraft() stringByAppendingPathComponent:frame[@"video"]]:nil;self.originalData=frame?[NSData dataWithContentsOfFile:[CFDraft() stringByAppendingPathComponent:frame[@"source"]]]:nil;self.currentImage=self.originalData?CFCanonicalPhoto([UIImage imageWithData:self.originalData]):nil;self.alignment=frame?[frame[@"alignment"] mutableCopy]:[CFDefaultAlignment() mutableCopy];
    self.zoom.value=[self.alignment[@"zoom"]floatValue];self.shift.value=[self.alignment[@"shift"]floatValue];self.threshold.value=[self.alignment[@"threshold"]floatValue];self.headButton.hidden=self.mode.selectedSegmentIndex==0||[pose[@"id"]isEqual:@"portrait"];self.headStatus.text=[self.alignment[@"headWidth"]doubleValue]>0?[NSString stringWithFormat:@"Ширина головы: %.0f",[self.alignment[@"headWidth"]doubleValue]]:@"";
    self.imageView.image=nil;self.imageView.hidden=self.currentImage==nil;self.acceptButton.enabled=self.currentImage!=nil;self.publishButton.enabled=self.frames.count==self.poses.count;self.status.text=self.currentImage?@"":@"Снимите позу камерой или выберите фото / видео.";
    for(UIButton *b in self.steps.arrangedSubviews)[b setTitle:[NSString stringWithFormat:@"%@ %@",self.frames[self.poses[b.tag][@"id"]]?@"✓":@(b.tag+1),self.poses[b.tag][@"title"]] forState:UIControlStateNormal];
    if(self.currentImage)[self render];
}
- (void)render {
    if(!self.currentImage)return;self.imageView.hidden=NO;self.acceptButton.enabled=NO;NSError *error;BOOL portrait=[self.poses[self.step][@"id"]isEqual:@"portrait"];CFPhotoCutout *cut=portrait?CFCutoutPortrait(self.currentImage,self.threshold.value,&error):CFCutoutPhoto(self.currentImage,self.threshold.value,&error);if(!cut){self.imageView.image=self.currentImage;self.status.text=error.localizedDescription;return;}
    CFPhotoCutout *reference=cut;NSDictionary *standing=self.frames[@"stand"];
    if(self.step!=0&&!portrait){if(!standing){self.imageView.image=self.currentImage;self.status.text=@"Сначала сохраните позу «Готовность»: она задаёт масштаб остальных поз.";return;}UIImage *source=CFCanonicalPhoto([UIImage imageWithContentsOfFile:[CFDraft() stringByAppendingPathComponent:standing[@"source"]]]);reference=CFCutoutPhoto(source,[standing[@"alignment"][@"threshold"]doubleValue],&error);}
    if(!reference){self.status.text=error.localizedDescription;return;}
    CGFloat head=self.step==0?[self.alignment[@"headWidth"]doubleValue]:[standing[@"alignment"][@"headWidth"]doubleValue];
    UIImage *normalized=CFNormalizePhoto(cut,300/reference.bounds.size.height,head,self.alignment,self.mode.selectedSegmentIndex==1,[self.poses[self.step][@"id"]isEqual:@"portrait"],&error);
    self.imageView.image=normalized?:self.currentImage;self.acceptButton.enabled=normalized!=nil;self.status.text=normalized?(portrait?@"Проверьте лицо и рога. Этот кадр станет портретом на экране выбора бойца.":@"Проверьте контур, глаза и пальцы. Ноги выровнены по линии опоры."):error.localizedDescription;
}
- (void)markHead {if(!self.currentImage){self.status.text=@"Сначала сделайте снимок.";return;}self.headPoints=[NSMutableArray array];self.imageView.hidden=NO;self.imageView.image=self.currentImage;self.acceptButton.enabled=NO;self.status.text=@"Коснитесь левого и правого края головы без рогов.";[self.scroll scrollRectToVisible:[self.preview convertRect:self.preview.bounds toView:self.scroll] animated:YES];}
- (void)markTap:(UITapGestureRecognizer *)tap {
    if(!self.headPoints)return;CGPoint p=[tap locationInView:self.preview];p.x*=640/self.preview.bounds.size.width;p.y*=480/self.preview.bounds.size.height;[self.headPoints addObject:[NSValue valueWithCGPoint:p]];
    if(self.headPoints.count==2){CGFloat width=fabs(self.headPoints[1].CGPointValue.x-self.headPoints[0].CGPointValue.x);if(width<8){[self.headPoints removeAllObjects];self.status.text=@"Точки слишком близко. Отметьте всю ширину головы.";return;}self.alignment[@"headWidth"]=@(width);self.headPoints=nil;self.headStatus.text=[NSString stringWithFormat:@"Ширина головы: %.0f",width];[self render];}
}
- (void)alignmentChanged {self.alignment[@"zoom"]=@(self.zoom.value);self.alignment[@"shift"]=@(self.shift.value);self.alignment[@"threshold"]=@(self.threshold.value);self.headPoints=nil;[self render];}
- (void)opacityChanged {self.guide.alpha=self.opacity.value;}
- (void)modeChanged {self.headButton.hidden=self.mode.selectedSegmentIndex==0||[self.poses[self.step][@"id"]isEqual:@"portrait"];self.headStatus.text=self.mode.selectedSegmentIndex==1?@"С рук: отметьте ширину головы на каждом кадре, чтобы убрать разницу дистанции.":@"На подставке: сохраняйте одну дистанцию и зум для всех поз.";[self saveDraft];[self render];}
- (void)showOriginal {if(self.currentImage){self.imageView.hidden=NO;self.imageView.image=self.currentImage;}}
- (void)retake {if(self.busy)return;NSString *key=self.poses[self.step][@"id"];[self.frames removeObjectForKey:key];self.currentImage=nil;self.originalData=nil;[self saveDraft];[self enterStep:self.step];}
- (void)saveDraft {[@{@"name":self.nameField.text?:@"",@"handheld":@(self.mode.selectedSegmentIndex==1),@"frames":self.frames} writeToFile:[CFDraft() stringByAppendingPathComponent:@"draft.plist"] atomically:YES];}
- (void)acceptPose {
    if(self.busy||!self.currentImage||!self.acceptButton.enabled)return;NSString *key=self.poses[self.step][@"id"],*relative=[NSString stringWithFormat:@"sources/%@.jpg",key];NSError *error;
    if(![self.originalData writeToFile:[CFDraft() stringByAppendingPathComponent:relative] options:NSDataWritingAtomic error:&error]){self.status.text=error.localizedDescription;return;}
    NSString *videoPath=[CFDraft() stringByAppendingPathComponent:[NSString stringWithFormat:@"sources/%@.mov",key]];
    NSMutableDictionary *frame=[@{@"source":relative,@"alignment":[self.alignment copy]}mutableCopy];
    if(self.pendingVideo){
        if(![self.pendingVideo isEqualToString:videoPath]){[NSFileManager.defaultManager removeItemAtPath:videoPath error:nil];if(![NSFileManager.defaultManager copyItemAtPath:self.pendingVideo toPath:videoPath error:&error]){self.status.text=error.localizedDescription;return;}}
        frame[@"video"]=[NSString stringWithFormat:@"sources/%@.mov",key];
    }else [NSFileManager.defaultManager removeItemAtPath:videoPath error:nil];
    self.frames[key]=frame;[self saveDraft];if(self.step+1<self.poses.count)[self enterStep:self.step+1];else{[self enterStep:self.step];self.status.text=@"Все позы и портрет готовы. Проверьте кадры и нажмите «Добавить бойца в игру».";}
}
- (void)previousPose {[self enterStep:self.step-1];}
- (void)nextPose {[self enterStep:self.step+1];}
- (void)selectPose:(UIButton *)b {[self enterStep:b.tag];}
- (BOOL)textFieldShouldReturn:(UITextField *)field {[field resignFirstResponder];[self saveDraft];return YES;}
- (NSString *)creatureText:(NSString *)name {
    NSMutableString *text=[NSMutableString stringWithFormat:@"name %@\nart photo right\nheight 92\nportrait photos/portrait.png\nspeed 1.4 1.1 7.5\nwidth 26\n\n",name];
    void (^anim)(NSString *,NSArray *,int,NSString *)=^(NSString *key,NSArray *frames,int ticks,NSString *suffix){[text appendFormat:@"anim %@ %@\n",key,suffix];for(NSString *f in frames)[text appendFormat:@"  photos/%@.png %d\n",f,ticks];[text appendString:@"end\n\n"];};
    anim(@"stand",@[@"stand",@"breathe"],12,@"loop");anim(@"walkf",@[@"walk1",@"walk2",@"walk3",@"walk4"],7,@"loop");anim(@"walkb",@[@"walk4",@"walk3",@"walk2",@"walk1"],8,@"loop");
    NSDictionary *single=@{@"crouch":@"crouch",@"jump":@"jump",@"land":@"crouch",@"block":@"block",@"crouchblock":@"block",@"hit":@"hit",@"crouchhit":@"hit",@"hithard":@"hit",@"fall":@"down",@"down":@"down",@"getup":@"crouch",@"dizzy":@"hit",@"win":@"win"};
    for(NSString *key in [[single allKeys]sortedArrayUsingSelector:@selector(compare:)])anim(key,@[single[key]],12,[@[@"crouch",@"block",@"crouchblock",@"down",@"win"]containsObject:key]?@"hold":@"");
    anim(@"punch",@[@"stand",@"punch1",@"punch2",@"stand"],6,@"");anim(@"kick",@[@"stand",@"kick",@"kick",@"stand"],6,@"");
    NSArray *buttons=@[@"hq",@"hf",@"lq",@"lf"],*damage=@[@6,@11,@7,@11];for(NSString *g in @[@"stand",@"crouch",@"jump",@"air"])for(int b=0;b<4;b++)[text appendFormat:@"move %@.%@ %@ damage %@ stun 18 strength 2 level %@\nend\n\n",g,buttons[b],b<2?@"punch":@"kick",damage[b],b<2?@"high":@"low"];
    return text;
}
- (void)publish {
    if(self.busy||self.frames.count!=self.poses.count)return;NSString *name=[[self.nameField.text stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet]uppercaseString];
    NSRegularExpression *valid=[NSRegularExpression regularExpressionWithPattern:@"^[A-Z][A-Z0-9_]{0,15}$" options:0 error:nil];if([valid numberOfMatchesInString:name options:0 range:NSMakeRange(0,name.length)]!=1){self.status.text=@"Имя: 1–16 латинских букв, цифр или _, первая — буква.";return;}
    for(int i=0;i<numroster;i++)if(!strcasecmp(roster[i]->name,name.UTF8String)){self.status.text=@"Такое имя уже есть. Выберите другое.";return;}if(numroster>=MAXROSTER){self.status.text=@"Список бойцов заполнен.";return;}
    [self.view endEditing:YES];[self saveDraft];[self setBusy:YES];self.status.text=@"Убираем фон, выравниваем позы и готовим бойца на устройстве…";
    NSDictionary *frames=[self.frames copy];BOOL handheld=self.mode.selectedSegmentIndex==1;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED,0),^{
        NSFileManager *fm=NSFileManager.defaultManager;NSError *error;NSString *staging=[CFCreaturesPath() stringByAppendingPathComponent:[@".capture-" stringByAppendingString:NSUUID.UUID.UUIDString]],*photos=[staging stringByAppendingPathComponent:@"photos"];
        [fm createDirectoryAtPath:photos withIntermediateDirectories:YES attributes:nil error:&error];
        CFPhotoCutout *standing=CFCutoutPhoto(CFCanonicalPhoto([UIImage imageWithContentsOfFile:[CFDraft() stringByAppendingPathComponent:frames[@"stand"][@"source"]]]),[frames[@"stand"][@"alignment"][@"threshold"]doubleValue],&error);
        CGFloat scale=standing?300/standing.bounds.size.height:0,head=[frames[@"stand"][@"alignment"][@"headWidth"]doubleValue];
        for(NSDictionary *pose in self.poses){if(error)break;NSString *key=pose[@"id"];NSDictionary *frame=frames[key];UIImage *raw=CFCanonicalPhoto([UIImage imageWithContentsOfFile:[CFDraft() stringByAppendingPathComponent:frame[@"source"]]]);CFPhotoCutout *cut=[key isEqual:@"portrait"]?CFCutoutPortrait(raw,[frame[@"alignment"][@"threshold"]doubleValue],&error):CFCutoutPhoto(raw,[frame[@"alignment"][@"threshold"]doubleValue],&error);if(!cut)break;UIImage *normalized=CFNormalizePhoto(cut,scale,head,frame[@"alignment"],handheld,[key isEqual:@"portrait"],&error);if(!normalized)break;[UIImagePNGRepresentation(normalized)writeToFile:[photos stringByAppendingPathComponent:[key stringByAppendingPathExtension:@"png"]] options:NSDataWritingAtomic error:&error];}
        if(!error)[[self creatureText:name]writeToFile:[staging stringByAppendingPathComponent:@"fighter.txt"] atomically:YES encoding:NSUTF8StringEncoding error:&error];
        if(!error)[fm copyItemAtPath:[CFDraft() stringByAppendingPathComponent:@"sources"] toPath:[staging stringByAppendingPathComponent:@"sources"] error:&error];
        if(!error)[@{@"name":name,@"handheld":@(handheld),@"frames":frames}writeToFile:[staging stringByAppendingPathComponent:@"capture.plist"] atomically:YES];
        NSString *destination=[CFCreaturesPath()stringByAppendingPathComponent:[NSString stringWithFormat:@"custom-%@-%@",name.lowercaseString,NSUUID.UUID.UUIDString]];
        if(error)[fm removeItemAtPath:staging error:nil];
        dispatch_async(dispatch_get_main_queue(),^{[self setBusy:NO];if(error){self.status.text=error.localizedDescription;return;}
            fighterdef_t *fighter=NULL;
            @try {fighter=P_LoadCreature([staging stringByAppendingPathComponent:@"fighter.txt"].fileSystemRepresentation);}
            @catch(NSException *e){[fm removeItemAtPath:staging error:nil];self.status.text=e.reason;return;}
            NSError *publishError;
            if(!P_Playable(fighter)||![fm moveItemAtPath:staging toPath:destination error:&publishError]){[fm removeItemAtPath:staging error:nil];self.status.text=publishError.localizedDescription?:@"Не удалось подготовить бойца";return;}
            if(!P_RegisterCreature(fighter,[destination stringByAppendingPathComponent:@"fighter.txt"].fileSystemRepresentation)){[fm removeItemAtPath:destination error:nil];self.status.text=@"Не удалось добавить бойца в список";return;}
            [fm removeItemAtPath:CFDraft() error:nil];self.closed=YES;dispatch_async(self.sessionQueue,^{[self.session stopRunning];});[self dismissViewControllerAnimated:YES completion:^{SEL_Start();CFSuspendGame(NO);}];
        });
    });
}
- (void)background {[self saveDraft];if(self.movieOutput.recording)[self.movieOutput stopRecording];dispatch_async(self.sessionQueue,^{[self.session stopRunning];});}
- (void)close {if(self.busy)return;[self saveDraft];self.closed=YES;dispatch_async(self.sessionQueue,^{[self.session stopRunning];});[self dismissViewControllerAnimated:YES completion:^{M_StartMenu();CFSuspendGame(NO);}];}
- (void)dealloc {[[NSNotificationCenter defaultCenter]removeObserver:self];}
- (BOOL)prefersStatusBarHidden {return YES;}
@end
