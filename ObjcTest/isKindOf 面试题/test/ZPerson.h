//
//  ZPerson.h
//  test
//
//  Created by zsq on 2021/4/23.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN


@interface ZPerson : NSObject{
    NSString *hobby;
}

@property (nonatomic, copy) NSString *name;

- (void)sayHello;

- (void)sayCode;

- (void)sayMaster;

- (void)sayNB;


+ (void)sayHappy;

@end


NS_ASSUME_NONNULL_END
