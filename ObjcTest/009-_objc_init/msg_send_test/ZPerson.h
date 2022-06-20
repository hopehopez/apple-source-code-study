//
//  ZPerson.h
//  ZTest
//
//  Created by zsq on 2021/1/8.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ZPerson : NSObject

- (void)sayHello;
- (void)sayNB;
@end

@interface ZStudent : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) NSInteger age;
- (void)sayHello;
@end

NS_ASSUME_NONNULL_END
