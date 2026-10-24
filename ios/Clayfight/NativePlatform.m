// UIKit / AVFoundation platform adapter. No SDL, browser or subprocess on iOS.
#import "NativeHost.h"
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

int I_GetTime(void) { static CFTimeInterval origin; CFTimeInterval now=CACurrentMediaTime(); if(!origin)origin=now; return (int)((now-origin)*TICRATE); }
void I_Sleep(int ms) { usleep(ms*1000); }
void *I_Malloc(size_t size) { void *p=calloc(1,size?:1); if(!p)I_Error("Out of memory"); return p; }
void I_Log(const char *fmt, ...) { va_list a;va_start(a,fmt);vprintf(fmt,a);putchar('\n');va_end(a); }
void I_Error(const char *fmt, ...) { char msg[1024];va_list a;va_start(a,fmt);vsnprintf(msg,sizeof msg,fmt,a);va_end(a);@throw [NSException exceptionWithName:@"ClayfightDataError" reason:@(msg) userInfo:nil]; }
void I_InitGraphics(bool headless) { }
void I_ShutdownGraphics(void) { }
void I_FinishUpdate(const pixel_t *pixels) { CFGameFrame(pixels); }
void I_StartTic(void) { }
bool I_QuitRequested(void) { return false; }
void I_SaveScreenshot(const pixel_t *pixels,const char *path) {
    NSData *data=[NSData dataWithBytes:pixels length:SCREENWIDTH*SCREENHEIGHT*4];
    CGDataProviderRef provider=CGDataProviderCreateWithCFData((__bridge CFDataRef)data);
    CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
    CGImageRef image=CGImageCreate(SCREENWIDTH,SCREENHEIGHT,8,32,SCREENWIDTH*4,colors,kCGBitmapByteOrder32Little|kCGImageAlphaNoneSkipFirst,provider,NULL,false,kCGRenderingIntentDefault);
    CGImageDestinationRef out=CGImageDestinationCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath:@(path)],(__bridge CFStringRef)UTTypePNG.identifier,1,NULL);
    if(out){CGImageDestinationAddImage(out,image,NULL);CGImageDestinationFinalize(out);CFRelease(out);}
    CGImageRelease(image);CGColorSpaceRelease(colors);CGDataProviderRelease(provider);
}
static AVAudioEngine *audio;
static NSArray<AVAudioPlayerNode *> *players;
static int nextvoice;
void I_InitSound(void) {
    NSError *error;
    [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryAmbient error:&error];
    [[AVAudioSession sharedInstance] setActive:YES error:&error];
    audio=[AVAudioEngine new];NSMutableArray *nodes=[NSMutableArray array];
    AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:1];
    for(int i=0;i<16;i++){AVAudioPlayerNode *node=[AVAudioPlayerNode new];[audio attachNode:node];[audio connect:node to:audio.mainMixerNode format:format];[nodes addObject:node];}
    players=nodes;if(![audio startAndReturnError:&error]){NSLog(@"Audio: %@",error);audio=nil;}
}
void I_ShutdownSound(void) { [audio stop];players=nil;audio=nil; }
void I_StartSound(const sfxdata_t *sfx,float vol,float sep) {
    if(!audio.running||!sfx||sfx->length<=0||sfx->rate<=0)return;
    AVAudioFormat *format=[[AVAudioFormat alloc] initStandardFormatWithSampleRate:44100 channels:1];
    AVAudioFrameCount n=(AVAudioFrameCount)((uint64_t)sfx->length*44100/sfx->rate);
    AVAudioPCMBuffer *buffer=[[AVAudioPCMBuffer alloc] initWithPCMFormat:format frameCapacity:n];buffer.frameLength=n;
    for(AVAudioFrameCount i=0;i<n;i++)buffer.floatChannelData[0][i]=(sfx->data[MIN(sfx->length-1,(int)((uint64_t)i*sfx->rate/44100))]-128)/128.f;
    AVAudioPlayerNode *node=players[nextvoice++%players.count];[node stop];node.volume=CLAMP(vol,0,1);node.pan=CLAMP(sep,-1,1);[node scheduleBuffer:buffer completionHandler:nil];[node play];
}
void I_Shutdown(void) { I_ShutdownSound(); }
bool I_StartCreator(const char *creatures,const char *data) { return true; }
int I_CreatorStatus(void) { return 1; }
const char *I_CreatorURL(bool phone) { return ""; }
void I_OpenCreator(void) { CFPresentCreator(); }
void I_CloseCreator(void) { }
