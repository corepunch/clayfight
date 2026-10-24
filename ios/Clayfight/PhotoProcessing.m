#import "PhotoProcessing.h"
#import <math.h>
@implementation CFPhotoCutout
@end
static void CFPhotoError(NSError **error, NSString *text) { if(error)*error=[NSError errorWithDomain:@"ClayfightCapture" code:1 userInfo:@{NSLocalizedDescriptionKey:text}]; }
UIImage *CFCanonicalPhoto(UIImage *image) {
    UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=1;format.opaque=YES;
    UIGraphicsImageRenderer *renderer=[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(640,480) format:format];
    return [renderer imageWithActions:^(UIGraphicsImageRendererContext *c){
        [UIColor.whiteColor setFill];[c fillRect:CGRectMake(0,0,640,480)];
        CGFloat scale=MIN(640/image.size.width,480/image.size.height);CGSize size=CGSizeMake(image.size.width*scale,image.size.height*scale);
        [image drawInRect:CGRectMake((640-size.width)/2,(480-size.height)/2,size.width,size.height)];
    }];
}
static int CFByteCompare(const void *a,const void *b){return *(const unsigned char *)a-*(const unsigned char *)b;}
static CFPhotoCutout *CFCutout(UIImage *image, CGFloat threshold, BOOL portrait, NSError **error) {
    const int w=640,h=480,n=w*h;
    unsigned char *rgba=calloc(n,4),*seen=calloc(n,1);int *queue=malloc(n*sizeof(int)),*largest=malloc(n*sizeof(int));
    if(!rgba||!seen||!queue||!largest){free(rgba);free(seen);free(queue);free(largest);CFPhotoError(error,@"Не хватает памяти для обработки фотографии");return nil;}
    CGColorSpaceRef colors=CGColorSpaceCreateDeviceRGB();
    CGContextRef context=CGBitmapContextCreate(rgba,w,h,8,w*4,colors,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);
    CGContextDrawImage(context,CGRectMake(0,0,w,h),image.CGImage);
    unsigned char edges[3][80];int background[3];
    for(int i=0;i<80;i++){
        int x=(i%20)*(w-1)/19,y=(i%20)*(h-1)/19;
        int p=i<20?x:i<40?(portrait?x:(h-1)*w+x):i<60?y*w:y*w+w-1;
        for(int c=0;c<3;c++)edges[c][i]=rgba[p*4+c];
    }
    for(int c=0;c<3;c++){qsort(edges[c],80,1,CFByteCompare);background[c]=edges[c][40];}
    unsigned char *back=calloc(n,1);
    if(!back){CGContextRelease(context);CGColorSpaceRelease(colors);free(rgba);free(seen);free(queue);free(largest);CFPhotoError(error,@"Не хватает памяти для обработки фотографии");return nil;}
    for(int p=0;p<n;p++){
        double r=rgba[p*4]-background[0],g=rgba[p*4+1]-background[1],b=rgba[p*4+2]-background[2];
        back[p]=hypot(hypot((r-g)*.75,(b-g)*.75),(r+g+b)/6)<threshold||rgba[p*4+3]<128;
    }
    int begin=0,end=0;
    for(int p=0;p<n;p++)if((p<w||p>=n-w||p%w==0||p%w==w-1)&&back[p]){seen[p]=1;queue[end++]=p;}
    while(begin<end){int p=queue[begin++],neighbors[]={p-1,p+1,p-w,p+w};for(int i=0;i<4;i++){int q=neighbors[i];if(q<0||q>=n||(i==0&&p%w==0)||(i==1&&p%w==w-1))continue;if(!seen[q]&&back[q]){seen[q]=1;queue[end++]=q;}}}
    int count=0;
    for(int p=0;p<n;p++){
        if(seen[p])continue;begin=0;end=1;queue[0]=p;seen[p]=1;
        while(begin<end){int q=queue[begin++],neighbors[]={q-1,q+1,q-w,q+w};for(int i=0;i<4;i++){int r=neighbors[i];if(r<0||r>=n||(i==0&&q%w==0)||(i==1&&q%w==w-1))continue;if(!seen[r]){seen[r]=1;queue[end++]=r;}}}
        if(end>count){count=end;memcpy(largest,queue,count*sizeof(int));}
    }
    int x0=w,y0=h,x1=0,y1=0;memset(seen,0,n);
    for(int i=0;i<count;i++){int p=largest[i],x=p%w,y=p/w;seen[p]=1;x0=MIN(x0,x);x1=MAX(x1,x);y0=MIN(y0,y);y1=MAX(y1,y);}
    NSString *failure=nil;
    if(count<n*.006)failure=@"Фигурка не найдена. Нужен контрастный однотонный фон. Попробуйте уменьшить отделение фона.";
    else if(x0<3||y0<3||x1>w-4||(!portrait&&y1>h-4))failure=@"Фигурка касается края. Отодвиньте камеру: голова, ноги, руки и хвост должны целиком попасть в кадр.";
    CFPhotoCutout *result=nil;
    if(!failure){
        long foot=0,feet=0;for(int i=0;i<count;i++){int p=largest[i];if(p/w>y1-MAX(2,(y1-y0)/16)){foot+=p%w;feet++;}}
        for(int p=0;p<n;p++)if(!seen[p])memset(rgba+p*4,0,4);
        CGImageRef cut=CGBitmapContextCreateImage(context);result=[CFPhotoCutout new];result.image=[UIImage imageWithCGImage:cut];CGImageRelease(cut);
        result.bounds=CGRectMake(x0,y0,x1-x0+1,y1-y0+1);result.foot=feet?(CGFloat)foot/feet:(x0+x1)/2.;
    }else CFPhotoError(error,portrait&&count>=n*.006?@"Оставьте запас над головой и по бокам. Плечи можно обрезать снизу.":failure);
    CGContextRelease(context);CGColorSpaceRelease(colors);free(rgba);free(seen);free(back);free(queue);free(largest);return result;
}
CFPhotoCutout *CFCutoutPhoto(UIImage *image,CGFloat threshold,NSError **error){return CFCutout(image,threshold,NO,error);}
CFPhotoCutout *CFCutoutPortrait(UIImage *image,CGFloat threshold,NSError **error){return CFCutout(image,threshold,YES,error);}
UIImage *CFNormalizePhoto(CFPhotoCutout *cutout,CGFloat referenceScale,CGFloat referenceHead,NSDictionary *alignment,BOOL handheld,BOOL portrait,NSError **error){
    CGFloat head=[alignment[@"headWidth"] doubleValue];
    if(handheld&&!portrait&&(head<8||referenceHead<8)){CFPhotoError(error,@"Отметьте левый и правый края головы: это линейка для съёмки с рук.");return nil;}
    CGRect b=cutout.bounds;CGFloat scale=portrait?MIN(390/b.size.width,360/b.size.height):referenceScale;
    if(handheld&&!portrait)scale*=referenceHead/head;
    scale*=[alignment[@"zoom"] doubleValue]/100.;
    CGFloat x=(portrait?320-CGRectGetMidX(b)*scale:320-cutout.foot*scale)+[alignment[@"shift"] doubleValue];
    CGFloat y=430-(CGRectGetMaxY(b)-1)*scale;
    if(x+b.origin.x*scale<12||x+(CGRectGetMaxX(b)-1)*scale>628||y+b.origin.y*scale<12){CFPhotoError(error,@"Поза выходит за границы после выравнивания. Уменьшите масштаб или переснимите с большим запасом по краям.");return nil;}
    UIGraphicsImageRendererFormat *format=[UIGraphicsImageRendererFormat defaultFormat];format.scale=1;format.opaque=YES;
    return [[[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(640,480) format:format] imageWithActions:^(UIGraphicsImageRendererContext *ctx){[UIColor.whiteColor setFill];[ctx fillRect:CGRectMake(0,0,640,480)];[cutout.image drawInRect:CGRectMake(x,y,640*scale,480*scale)];}];
}
