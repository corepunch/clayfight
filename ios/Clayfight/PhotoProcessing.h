#import <UIKit/UIKit.h>
NS_ASSUME_NONNULL_BEGIN
@interface CFPhotoCutout : NSObject
@property UIImage *image;
@property CGRect bounds;
@property CGFloat foot;
@end
UIImage *CFCanonicalPhoto(UIImage *image);
CFPhotoCutout *_Nullable CFCutoutPhoto(UIImage *image, CGFloat threshold, NSError **error);
CFPhotoCutout *_Nullable CFCutoutPortrait(UIImage *image, CGFloat threshold, NSError **error);
UIImage *_Nullable CFNormalizePhoto(CFPhotoCutout *cutout, CGFloat referenceScale, CGFloat referenceHead,
                                    NSDictionary *alignment, BOOL handheld, BOOL portrait, NSError **error);
NS_ASSUME_NONNULL_END
