// Debug-only checks run inside the actual iOS runtime with --native-checks.
#import "NativeHost.h"
#import "PhotoProcessing.h"
#import "PoseGuideView.h"
#ifdef DEBUG
@interface CFCaptureController (RecipeCheck)
- (NSString *)creatureText:(NSString *)name;
@end
void CFRunNativeChecks(void) {
    NSMutableArray *failures=[NSMutableArray array],*checks=[NSMutableArray array];
    void (^check)(BOOL,NSString *)=^(BOOL ok,NSString *name){[checks addObject:name];if(!ok)[failures addObject:name];};
    @try {
        UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=1;format.opaque=YES;
        UIImage *fixture=[[[UIGraphicsImageRenderer alloc]initWithSize:CGSizeMake(640,480) format:format]imageWithActions:^(UIGraphicsImageRendererContext *ctx){[UIColor.whiteColor setFill];[ctx fillRect:CGRectMake(0,0,640,480)];[[UIColor colorWithRed:.75 green:.15 blue:.22 alpha:1]setFill];[ctx fillRect:CGRectMake(270,100,100,330)];[ctx fillRect:CGRectMake(355,220,135,30)];[UIColor.whiteColor setFill];[ctx fillRect:CGRectMake(300,125,15,15)];}];
        UIImage *portraitFixture=[[[UIGraphicsImageRenderer alloc]initWithSize:CGSizeMake(640,480) format:format]imageWithActions:^(UIGraphicsImageRendererContext *ctx){[UIColor.whiteColor setFill];[ctx fillRect:CGRectMake(0,0,640,480)];[UIColor.redColor setFill];[ctx fillRect:CGRectMake(240,80,160,250)];[ctx fillRect:CGRectMake(180,300,280,180)];}];
        NSError *portraitError=nil;
        CFPhotoCutout *portrait=CFCutoutPortrait(portraitFixture,38,&portraitError);
        check(portrait&&CGRectGetMaxY(portrait.bounds)==480,@"close portrait permits shoulders at the bottom edge");
        check(CFCutoutPhoto(portraitFixture,38,&portraitError)==nil,@"full body capture still rejects cropped feet");
        check(CFNormalizePhoto(portrait,1,0,@{@"zoom":@100,@"shift":@0},NO,YES,&portraitError)!=nil,@"close portrait normalizes independently of body scale");
        UIImage *clippedHead=[[[UIGraphicsImageRenderer alloc]initWithSize:CGSizeMake(640,480) format:format]imageWithActions:^(UIGraphicsImageRendererContext *ctx){[UIColor.whiteColor setFill];[ctx fillRect:CGRectMake(0,0,640,480)];[portraitFixture drawAtPoint:CGPointMake(0,-80)];}];
        check(CFCutoutPortrait(clippedHead,38,&portraitError)==nil,@"portrait still rejects a cropped head");
        NSError *error;CFPhotoCutout *cut=CFCutoutPhoto(CFCanonicalPhoto(fixture),38,&error);
        check(cut!=nil,@"native segmentation finds the clay figure");
        check(cut.bounds.origin.y==100&&CGRectGetMaxY(cut.bounds)==430,@"UIKit image orientation and foot coordinates");
        check(CGRectGetMaxX(cut.bounds)==490,@"extended punch stays intact");
        CFDataRef pixels=CGDataProviderCopyData(CGImageGetDataProvider(cut.image.CGImage));const UInt8 *bytes=CFDataGetBytePtr(pixels);size_t row=CGImageGetBytesPerRow(cut.image.CGImage);
        check(bytes[130*row+305*4+3]==255,@"white eye stays opaque");check(bytes[3]==0,@"background becomes transparent");CFRelease(pixels);
        NSDictionary *alignment=@{@"zoom":@100,@"shift":@0,@"headWidth":@80};
        UIImage *normalized=CFNormalizePhoto(cut,300/cut.bounds.size.height,80,alignment,YES,NO,&error);
        check(normalized!=nil&&normalized.size.width==640,@"handheld normalization runs natively");
        CFPhotoCutout *placed=CFCutoutPhoto(normalized,38,&error);check(fabs(CGRectGetMaxY(placed.bounds)-430)<=2,@"normalized feet share the baseline");
        NSError *missing;check(CFNormalizePhoto(cut,1,80,@{@"zoom":@100},YES,NO,&missing)==nil,@"handheld requires a head measurement");
        NSString *dir=[CFStoragePath()stringByAppendingPathComponent:@"native-qa"],*photos=[dir stringByAppendingPathComponent:@"photos"];
        [NSFileManager.defaultManager createDirectoryAtPath:photos withIntermediateDirectories:YES attributes:nil error:&error];
        NSArray *poses=[NSJSONSerialization JSONObjectWithData:[NSData dataWithContentsOfURL:[NSBundle.mainBundle URLForResource:@"poses" withExtension:@"json" subdirectory:@"data"]] options:0 error:&error];
        NSString *art=[CFStoragePath()stringByAppendingPathComponent:@"guide-qa"];
        [NSFileManager.defaultManager createDirectoryAtPath:art withIntermediateDirectories:YES attributes:nil error:nil];
        UIImage *sheet=[[[UIGraphicsImageRenderer alloc]initWithSize:CGSizeMake(1280,1040) format:format]imageWithActions:^(UIGraphicsImageRendererContext *ctx){
            for(NSInteger i=0;i<poses.count;i++){
                CGContextSaveGState(ctx.CGContext);CGContextTranslateCTM(ctx.CGContext,(i%4)*320,(i/4)*260);
                [[UIColor colorWithWhite:.16 alpha:1]setFill];[ctx fillRect:CGRectMake(0,0,320,260)];
                [poses[i][@"title"]drawAtPoint:CGPointMake(16,8) withAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:15 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:UIColor.whiteColor}];
                CGContextTranslateCTM(ctx.CGContext,0,20);CGContextScaleCTM(ctx.CGContext,.5,.5);
                CFGuideView *guide=[[CFGuideView alloc]initWithFrame:CGRectMake(0,0,640,480)];guide.pose=poses[i][@"id"];[guide drawRect:guide.bounds];
                CGContextRestoreGState(ctx.CGContext);
            }
        }];
        [UIImagePNGRepresentation(sheet)writeToFile:[art stringByAppendingPathComponent:@"all-poses.png"]atomically:YES];
        NSData *png=UIImagePNGRepresentation(normalized);
        for(NSDictionary *p in poses)[png writeToFile:[photos stringByAppendingPathComponent:[p[@"id"] stringByAppendingPathExtension:@"png"]]atomically:YES];
        NSString *text=[[CFCaptureController new]creatureText:@"NATIVEQA"],*path=[dir stringByAppendingPathComponent:@"fighter.txt"];
        [text writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:&error];fighterdef_t *fighter=P_LoadCreature(path.fileSystemRepresentation);
        check(P_Playable(fighter)&&fighter->photoport!=NULL,@"native photos and separate portrait load through the real C engine");
        check(fighter->anims[ANIM_WALKF].count==4&&fighter->moves[0].valid,@"native recipe produces walking and attacks");
        int before=numroster;
        check(P_RegisterCreature(fighter,path.fileSystemRepresentation)&&numroster==before+1,@"validated native fighter registers in the selection roster");
        check(!P_RegisterCreature(fighter,path.fileSystemRepresentation)&&numroster==before+1,@"duplicate publication does not duplicate the roster");
        numroster=before; // QA fixture is never retained in the user's roster.
        bool cpu[2]={true,true};G_InitNew(fighter,roster[0],cpu,&stages[0]);for(int i=0;i<1200;i++){G_Ticker();if(gameaction!=ga_nothing){gameaction=ga_nothing;break;}}
        check(level.leveltime>0,@"native custom fighter fights Sauron");
        [NSFileManager.defaultManager removeItemAtPath:dir error:nil];M_StartMenu();
    }@catch(NSException *exception){[failures addObject:exception.reason?:exception.name];}
    NSDictionary *report=@{@"ok":@(failures.count==0),@"checks":checks,@"failures":failures};
    [[NSJSONSerialization dataWithJSONObject:report options:NSJSONWritingPrettyPrinted error:nil]writeToFile:[CFStoragePath() stringByAppendingPathComponent:@"native-checks.json"] atomically:YES];
    NSLog(@"Native checks: %@",report);
}
#endif
