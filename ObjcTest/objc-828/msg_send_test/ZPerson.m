//
//  ZPerson.m
//  ZTest
//
//  Created by zsq on 2021/1/8.
//

#import "ZPerson.h"
//#import "ZPerson+Extension.h"

@interface ZPerson ()

@property (nonatomic, copy) NSString *mName;

- (void)extM_method;

@end

@implementation ZPerson
+ (void)load{
    NSLog(@"%s",__func__);
}

- (void)extM_method{
    NSLog(@"%s",__func__);
}

- (void)extH_method{
    NSLog(@"%s",__func__);
}
@end
