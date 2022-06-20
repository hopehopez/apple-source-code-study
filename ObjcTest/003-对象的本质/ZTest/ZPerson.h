//
//  ZPerson.h
//  ZTest
//
//  Created by zsq on 2021/1/18.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ZPerson : NSObject
- (void)sayHello;
@end



@interface ZTeacher : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) int age;
@property (nonatomic, assign) long height;
@property (nonatomic, strong) NSString *hobby;
//@property (nonatomic, assign) int sex;
//@property (nonatomic) char ch1;
//@property (nonatomic) char ch2;

@end

@interface ZTeacher2 : NSObject
@property (nonatomic, copy) NSString *name;
@property (nonatomic, assign) int age;
@property (nonatomic, assign) long height;
@property (nonatomic, strong) NSString *hobby;
//@property (nonatomic, assign) int sex;
//@property (nonatomic) char ch1;
//@property (nonatomic) char ch2;

@end



NS_ASSUME_NONNULL_END
