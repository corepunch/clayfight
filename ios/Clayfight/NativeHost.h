#import <UIKit/UIKit.h>
#import "game/game.h"
NS_ASSUME_NONNULL_BEGIN
NSString *CFStoragePath(void);
NSString *CFCreaturesPath(void);
void CFPresentCreator(void);
void CFGameFrame(const pixel_t *pixels);
void CFSuspendGame(BOOL suspended);
@interface CFGameController : UIViewController
@end
@interface CFCaptureController : UIViewController
@end
NS_ASSUME_NONNULL_END
