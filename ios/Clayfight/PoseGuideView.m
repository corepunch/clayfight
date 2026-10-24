#import "PoseGuideView.h"
#import <math.h>

static CGPoint P(CGFloat x,CGFloat y){return CGPointMake(x,y);}
static CGPoint Add(CGPoint a,CGPoint b){return P(a.x+b.x,a.y+b.y);}
static CGPoint Normal(CGPoint a,CGPoint b,CGFloat width){CGFloat d=MAX(1,hypot(b.x-a.x,b.y-a.y));return P(-(b.y-a.y)*width/d,(b.x-a.x)*width/d);}
static UIColor *Mint(void){return [UIColor colorWithRed:.62 green:1 blue:.86 alpha:1];}

// A dark outer edge remains legible on a light backdrop; a fine white inner
// edge remains legible on dark clay. Interiors stay transparent to the camera.
static void Outline(UIBezierPath *path,BOOL far,BOOL active){
    // Cut hidden contours out of this isolated artwork layer before painting
    // the next body part. Camera pixels below the guide are never affected.
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextSaveGState(c);
    CGContextSetBlendMode(c,kCGBlendModeClear);[path fill];CGContextRestoreGState(c);
    [[UIColor colorWithWhite:far?.12:.25 alpha:far?.20:.12]setFill];[path fill];
    path.lineJoinStyle=kCGLineJoinRound;path.lineCapStyle=kCGLineCapRound;
    [[UIColor colorWithWhite:0 alpha:.62]setStroke];path.lineWidth=6;[path stroke];
    [(active?Mint():[UIColor colorWithWhite:1 alpha:far?.62:1])setStroke];path.lineWidth=2.8;[path stroke];
}
static void Detail(UIBezierPath *p){p.lineWidth=2;p.lineCapStyle=kCGLineCapRound;[[UIColor colorWithWhite:1 alpha:.7]setStroke];[p stroke];}
static UIBezierPath *Line(CGPoint a,CGPoint b){UIBezierPath *p=[UIBezierPath bezierPath];[p moveToPoint:a];[p addLineToPoint:b];return p;}

// Tapered, closed limbs with a shaped elbow/knee rather than stroked bones.
static void Limb(CGPoint root,CGPoint joint,CGPoint end,BOOL leg,BOOL far,BOOL active){
    CGPoint a=Normal(root,joint,leg?24:20),b=Normal(root,end,leg?18:14),d=Normal(joint,end,leg?12:10);
    UIBezierPath *p=[UIBezierPath bezierPath];[p moveToPoint:Add(root,a)];
    [p addQuadCurveToPoint:Add(joint,b) controlPoint:Add(root,Normal(root,joint,leg?30:26))];
    [p addQuadCurveToPoint:Add(end,d) controlPoint:Add(joint,Normal(joint,end,leg?22:19))];
    [p addQuadCurveToPoint:Add(end,P(-d.x,-d.y)) controlPoint:Add(end,Normal(P(0,0),P(-(end.y-joint.y),end.x-joint.x),-10))];
    [p addQuadCurveToPoint:Add(joint,P(-b.x,-b.y)) controlPoint:Add(joint,Normal(joint,end,leg?-20:-17))];
    [p addQuadCurveToPoint:Add(root,P(-a.x,-a.y)) controlPoint:Add(root,Normal(root,joint,leg?-26:-23))];[p closePath];Outline(p,far,active);
}
static void Foot(CGPoint ankle,BOOL far,BOOL active){
    CGFloat x=ankle.x,y=ankle.y;
    UIBezierPath *p=[UIBezierPath bezierPath];[p moveToPoint:P(x-13,y-9)];
    [p addQuadCurveToPoint:P(x+13,y-8) controlPoint:P(x,y-19)];
    [p addCurveToPoint:P(x+38,y+8) controlPoint1:P(x+17,y-1) controlPoint2:P(x+38,y-4)];
    [p addQuadCurveToPoint:P(x+31,y+15) controlPoint:P(x+43,y+15)];[p addLineToPoint:P(x-19,y+15)];
    [p addQuadCurveToPoint:P(x-23,y+7) controlPoint:P(x-28,y+15)];[p closePath];Outline(p,far,active);
    Detail(Line(P(x+16,y+7),P(x+16,y+13)));Detail(Line(P(x+26,y+7),P(x+26,y+13)));
}
static void Fist(CGPoint wrist,CGPoint elbow,BOOL far,BOOL active){
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextSaveGState(c);CGContextTranslateCTM(c,wrist.x,wrist.y);
    CGContextRotateCTM(c,atan2(wrist.y-elbow.y,wrist.x-elbow.x));
    UIBezierPath *p=[UIBezierPath bezierPath];[p moveToPoint:P(-7,-13)];
    [p addCurveToPoint:P(22,-15) controlPoint1:P(2,-24) controlPoint2:P(19,-23)];
    [p addQuadCurveToPoint:P(25,9) controlPoint:P(32,-1)];
    [p addQuadCurveToPoint:P(7,17) controlPoint:P(21,23)];
    [p addQuadCurveToPoint:P(-10,9) controlPoint:P(-9,24)];[p addQuadCurveToPoint:P(-7,-13) controlPoint:P(-18,-2)];[p closePath];Outline(p,far,active);
    UIBezierPath *thumb=[UIBezierPath bezierPath];[thumb moveToPoint:P(-7,8)];[thumb addQuadCurveToPoint:P(10,5) controlPoint:P(5,-3)];Detail(thumb);
    Detail(Line(P(10,-14),P(12,-7)));Detail(Line(P(19,-12),P(20,-5)));CGContextRestoreGState(c);
}
static void Head(CGPoint neck,CGFloat tilt){
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextSaveGState(c);CGContextTranslateCTM(c,neck.x,neck.y);CGContextRotateCTM(c,tilt);
    UIBezierPath *p=[UIBezierPath bezierPath];[p moveToPoint:P(-27,3)];
    [p addQuadCurveToPoint:P(-46,-45) controlPoint:P(-55,-10)];
    [p addQuadCurveToPoint:P(-39,-75) controlPoint:P(-50,-65)];
    [p addQuadCurveToPoint:P(-42,-103) controlPoint:P(-53,-88)];
    [p addQuadCurveToPoint:P(-20,-81) controlPoint:P(-25,-103)];
    [p addQuadCurveToPoint:P(15,-82) controlPoint:P(-4,-90)];
    [p addQuadCurveToPoint:P(34,-105) controlPoint:P(20,-104)];
    [p addQuadCurveToPoint:P(38,-72) controlPoint:P(46,-94)];
    [p addQuadCurveToPoint:P(51,-49) controlPoint:P(50,-65)];
    [p addCurveToPoint:P(69,-28) controlPoint1:P(71,-47) controlPoint2:P(79,-38)];
    [p addQuadCurveToPoint:P(48,-9) controlPoint:P(72,-12)];
    [p addQuadCurveToPoint:P(25,5) controlPoint:P(39,7)];[p addLineToPoint:P(22,14)];[p addLineToPoint:P(-22,14)];[p closePath];Outline(p,NO,NO);
    UIBezierPath *brow=[UIBezierPath bezierPath];[brow moveToPoint:P(5,-59)];[brow addQuadCurveToPoint:P(35,-57) controlPoint:P(20,-69)];Detail(brow);
    Outline([UIBezierPath bezierPathWithOvalInRect:CGRectMake(13,-56,18,21)],NO,NO);
    [UIColor.whiteColor setFill];[[UIBezierPath bezierPathWithOvalInRect:CGRectMake(24,-50,5,9)]fill];
    Detail(Line(P(53,-37),P(56,-36)));
    UIBezierPath *mouth=[UIBezierPath bezierPath];[mouth moveToPoint:P(16,-18)];[mouth addQuadCurveToPoint:P(59,-23) controlPoint:P(39,-13)];Detail(mouth);
    UIBezierPath *tooth=[UIBezierPath bezierPath];[tooth moveToPoint:P(43,-19)];[tooth addLineToPoint:P(46,-12)];[tooth addLineToPoint:P(50,-21)];Detail(tooth);CGContextRestoreGState(c);
}

typedef struct {CGPoint neck,hip,elbow[2],hand[2],knee[2],ankle[2];CGFloat tilt;int activeArm,activeLeg;} Rig;
static Rig Pose(NSString *p){
    Rig r={P(322,232),P(300,328),{P(226,283),P(379,286)},{P(257,241),P(410,242)},{P(265,375),P(339,374)},{P(248,415),P(351,415)},0,-1,-1};
    if([p isEqual:@"breathe"]){r.neck=P(322,224);r.hand[0]=P(261,229);r.hand[1]=P(413,229);}
    if([p isEqual:@"walk1"]){r.knee[0]=P(246,367);r.ankle[0]=P(211,415);r.knee[1]=P(350,371);r.ankle[1]=P(395,415);r.activeLeg=1;r.hand[1]=P(390,253);}
    if([p isEqual:@"walk2"]){r.neck.y=226;r.hip.y=322;r.knee[0]=P(263,358);r.ankle[0]=P(237,375);r.knee[1]=P(326,371);r.ankle[1]=P(314,415);r.activeLeg=0;r.hand[1]=P(406,233);}
    if([p isEqual:@"walk3"]){r.knee[0]=P(357,374);r.ankle[0]=P(394,415);r.knee[1]=P(252,370);r.ankle[1]=P(214,415);r.activeLeg=0;r.hand[0]=P(266,240);r.hand[1]=P(388,257);}
    if([p isEqual:@"walk4"]){r.neck.y=226;r.hip.y=322;r.knee[0]=P(293,372);r.ankle[0]=P(282,415);r.knee[1]=P(358,362);r.ankle[1]=P(348,377);r.activeLeg=1;}
    if([p isEqual:@"punch1"]){r.neck=P(308,235);r.hip=P(292,328);r.elbow[1]=P(231,264);r.hand[1]=P(204,224);r.activeArm=1;r.hand[0]=P(389,248);r.elbow[0]=P(350,285);r.tilt=-.08;}
    if([p isEqual:@"punch2"]){r.neck=P(339,230);r.hip=P(311,328);r.elbow[1]=P(421,247);r.hand[1]=P(493,235);r.activeArm=1;r.ankle[0]=P(229,415);r.ankle[1]=P(374,415);r.tilt=.08;}
    if([p isEqual:@"kick"]){r.neck=P(288,224);r.hip=P(294,320);r.knee[0]=P(268,375);r.ankle[0]=P(258,415);r.knee[1]=P(381,302);r.ankle[1]=P(466,287);r.activeLeg=1;r.tilt=-.15;r.hand[0]=P(234,228);r.elbow[0]=P(218,279);r.hand[1]=P(369,226);r.elbow[1]=P(351,280);}
    if([p isEqual:@"crouch"]){r.neck=P(331,301);r.hip=P(289,365);r.knee[0]=P(221,378);r.ankle[0]=P(246,415);r.knee[1]=P(369,375);r.ankle[1]=P(346,415);r.elbow[0]=P(256,348);r.hand[0]=P(293,315);r.elbow[1]=P(393,348);r.hand[1]=P(418,303);r.tilt=.12;}
    // The feet still rest on the capture surface; the engine supplies air time.
    if([p isEqual:@"jump"]){r.neck=P(321,252);r.hip=P(302,341);r.knee[0]=P(224,350);r.ankle[0]=P(256,415);r.knee[1]=P(386,345);r.ankle[1]=P(365,415);r.elbow[0]=P(236,261);r.hand[0]=P(239,213);r.elbow[1]=P(401,264);r.hand[1]=P(414,215);r.activeLeg=1;}
    if([p isEqual:@"block"]){r.neck=P(303,240);r.hip=P(306,332);r.elbow[0]=P(340,269);r.hand[0]=P(358,196);r.elbow[1]=P(390,267);r.hand[1]=P(395,196);r.activeArm=1;r.tilt=-.13;}
    if([p isEqual:@"hit"]){r.neck=P(268,239);r.hip=P(306,330);r.tilt=-.38;r.elbow[0]=P(205,275);r.hand[0]=P(179,224);r.elbow[1]=P(366,284);r.hand[1]=P(414,307);r.ankle[0]=P(231,415);r.ankle[1]=P(378,415);}
    if([p isEqual:@"down"]){r.neck=P(430,355);r.hip=P(305,377);r.tilt=1.35;r.elbow[0]=P(420,330);r.hand[0]=P(461,344);r.elbow[1]=P(410,407);r.hand[1]=P(466,405);r.knee[0]=P(250,367);r.ankle[0]=P(204,415);r.knee[1]=P(280,335);r.ankle[1]=P(237,392);}
    if([p isEqual:@"win"]){r.neck=P(322,230);r.elbow[0]=P(244,199);r.hand[0]=P(224,135);r.elbow[1]=P(404,194);r.hand[1]=P(426,130);r.activeArm=1;}
    return r;
}
static void Monster(Rig r){
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextBeginTransparencyLayer(c,NULL);
    CGPoint shoulders[]={P(r.neck.x-36,r.neck.y+22),P(r.neck.x+27,r.neck.y+25)},hips[]={P(r.hip.x-22,r.hip.y-3),P(r.hip.x+24,r.hip.y-3)};
    Limb(shoulders[0],r.elbow[0],r.hand[0],NO,YES,r.activeArm==0);Fist(r.hand[0],r.elbow[0],YES,r.activeArm==0);
    Limb(hips[0],r.knee[0],r.ankle[0],YES,YES,r.activeLeg==0);Foot(r.ankle[0],YES,r.activeLeg==0);
    UIBezierPath *body=[UIBezierPath bezierPath];[body moveToPoint:P(r.neck.x-23,r.neck.y-2)];
    [body addCurveToPoint:P(r.hip.x-41,r.hip.y-5) controlPoint1:P(r.neck.x-67,r.neck.y+13) controlPoint2:P(r.hip.x-49,r.hip.y-45)];
    [body addQuadCurveToPoint:P(r.hip.x+32,r.hip.y+14) controlPoint:P(r.hip.x-10,r.hip.y+30)];
    [body addCurveToPoint:P(r.neck.x+29,r.neck.y+9) controlPoint1:P(r.hip.x+75,r.hip.y-22) controlPoint2:P(r.neck.x+70,r.neck.y+29)];[body closePath];Outline(body,NO,NO);
    UIBezierPath *belly=[UIBezierPath bezierPath];[belly moveToPoint:P(r.neck.x+23,r.neck.y+40)];[belly addCurveToPoint:P(r.hip.x+15,r.hip.y-8) controlPoint1:P(r.neck.x+49,r.neck.y+67) controlPoint2:P(r.hip.x+47,r.hip.y-16)];Detail(belly);
    Limb(hips[1],r.knee[1],r.ankle[1],YES,NO,r.activeLeg==1);Foot(r.ankle[1],NO,r.activeLeg==1);
    Head(r.neck,r.tilt);
    Limb(shoulders[1],r.elbow[1],r.hand[1],NO,NO,r.activeArm==1);Fist(r.hand[1],r.elbow[1],NO,r.activeArm==1);
    CGContextEndTransparencyLayer(c);
}
static void Portrait(void){
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextBeginTransparencyLayer(c,NULL);
    UIBezierPath *shoulders=[UIBezierPath bezierPath];[shoulders moveToPoint:P(170,430)];[shoulders addCurveToPoint:P(250,335) controlPoint1:P(167,371) controlPoint2:P(211,340)];[shoulders addLineToPoint:P(270,303)];[shoulders addLineToPoint:P(370,303)];[shoulders addLineToPoint:P(390,335)];[shoulders addCurveToPoint:P(470,430) controlPoint1:P(432,340) controlPoint2:P(473,371)];Outline(shoulders,NO,NO);
    UIBezierPath *head=[UIBezierPath bezierPath];[head moveToPoint:P(241,175)];[head addCurveToPoint:P(223,100) controlPoint1:P(217,153) controlPoint2:P(212,117)];[head addQuadCurveToPoint:P(270,139) controlPoint:P(255,100)];[head addQuadCurveToPoint:P(370,139) controlPoint:P(320,119)];[head addQuadCurveToPoint:P(417,100) controlPoint:P(385,100)];[head addCurveToPoint:P(399,175) controlPoint1:P(428,117) controlPoint2:P(423,153)];[head addCurveToPoint:P(384,302) controlPoint1:P(426,218) controlPoint2:P(414,274)];[head addQuadCurveToPoint:P(256,302) controlPoint:P(320,354)];[head addCurveToPoint:P(241,175) controlPoint1:P(226,274) controlPoint2:P(214,218)];[head closePath];Outline(head,NO,NO);
    for(int side=0;side<2;side++){CGFloat x=side?350:270;Outline([UIBezierPath bezierPathWithOvalInRect:CGRectMake(x-18,193,36,44)],NO,NO);[UIColor.whiteColor setFill];[[UIBezierPath bezierPathWithOvalInRect:CGRectMake(x-4,208,10,19)]fill];UIBezierPath *brow=[UIBezierPath bezierPath];[brow moveToPoint:P(x-23,186)];[brow addQuadCurveToPoint:P(x+23,186) controlPoint:P(x,174)];Detail(brow);}
    UIBezierPath *muzzle=[UIBezierPath bezierPathWithRoundedRect:CGRectMake(267,242,106,60) cornerRadius:27];Outline(muzzle,NO,NO);
    Detail(Line(P(301,251),P(305,253)));Detail(Line(P(335,253),P(339,251)));
    UIBezierPath *smile=[UIBezierPath bezierPath];[smile moveToPoint:P(283,276)];[smile addQuadCurveToPoint:P(357,276) controlPoint:P(320,293)];Detail(smile);
    for(int i=0;i<2;i++){CGFloat x=i?341:293;UIBezierPath *tooth=[UIBezierPath bezierPath];[tooth moveToPoint:P(x,281)];[tooth addLineToPoint:P(x+4,290)];[tooth addLineToPoint:P(x+8,281)];Detail(tooth);}
    CGContextEndTransparencyLayer(c);
}
static void Arrow(CGPoint from,CGPoint to){
    UIBezierPath *p=Line(from,to);CGFloat a=atan2(to.y-from.y,to.x-from.x);[p moveToPoint:P(to.x-11*cos(a-.55),to.y-11*sin(a-.55))];[p addLineToPoint:to];[p addLineToPoint:P(to.x-11*cos(a+.55),to.y-11*sin(a+.55))];p.lineWidth=2.5;p.lineCapStyle=kCGLineCapRound;p.lineJoinStyle=kCGLineJoinRound;[Mint()setStroke];[p stroke];
}
static void Caption(NSString *s,CGRect rect,BOOL right){
    NSMutableParagraphStyle *style=[NSMutableParagraphStyle new];style.alignment=right?NSTextAlignmentRight:NSTextAlignmentLeft;
    [s drawInRect:rect withAttributes:@{NSFontAttributeName:[UIFont systemFontOfSize:13 weight:UIFontWeightSemibold],NSForegroundColorAttributeName:[UIColor colorWithWhite:1 alpha:.8],NSParagraphStyleAttributeName:style}];
}
@implementation CFGuideView
- (instancetype)initWithFrame:(CGRect)frame {if((self=[super initWithFrame:frame])){self.opaque=NO;self.backgroundColor=UIColor.clearColor;self.contentMode=UIViewContentModeRedraw;self.isAccessibilityElement=NO;}return self;}
- (void)setPose:(NSString *)pose {_pose=[pose copy];[self setNeedsDisplay];}
- (void)drawRect:(CGRect)rect {
    CGContextRef c=UIGraphicsGetCurrentContext();CGContextSaveGState(c);CGContextScaleCTM(c,self.bounds.size.width/640,self.bounds.size.height/480);
    BOOL portrait=[self.pose isEqual:@"portrait"];
    // Short corner brackets reserve room for hands/horns, without a distracting grid.
    UIBezierPath *frame=[UIBezierPath bezierPath];for(int i=0;i<4;i++){CGFloat x=i%2?608:32,y=i/2?448:32,dx=i%2?-18:18,dy=i/2?-18:18;[frame moveToPoint:P(x,y+dy)];[frame addLineToPoint:P(x,y)];[frame addLineToPoint:P(x+dx,y)];}Detail(frame);
    if(portrait){Portrait();Caption(@"ЛИЦО К КАМЕРЕ",CGRectMake(48,49,220,20),NO);}
    else{
        UIBezierPath *floor=Line(P(72,433),P(568,433));CGFloat dash[]={5,7};[floor setLineDash:dash count:2 phase:0];Detail(floor);
        Caption(@"ЛИНИЯ ОПОРЫ",CGRectMake(394,445,175,20),YES);Caption(@"БОКОМ",CGRectMake(48,49,100,20),NO);Arrow(P(535,60),P(577,60));
        {Rig r=Pose(self.pose);Monster(r);
            if([self.pose isEqual:@"punch1"])Arrow(P(273,194),P(218,191));
            if([self.pose isEqual:@"punch2"])Arrow(P(449,189),P(511,189));
            if([self.pose isEqual:@"kick"])Arrow(P(445,244),P(502,232));
            if([self.pose isEqual:@"crouch"])Arrow(P(442,194),P(442,253));
            if([self.pose isEqual:@"breathe"])Arrow(P(439,274),P(439,238));
            if([self.pose isEqual:@"jump"])Arrow(P(456,349),P(456,296));
            if([self.pose isEqual:@"hit"])Arrow(P(230,152),P(184,144));
            if([self.pose hasPrefix:@"walk"]){int i=r.activeLeg;CGPoint ankle=r.ankle[i];UIBezierPath *contact=Line(P(ankle.x-20,ankle.y+20),P(ankle.x+36,ankle.y+20));contact.lineWidth=3;[Mint()setStroke];[contact stroke];}
        }
    }
    CGContextRestoreGState(c);
}
@end
