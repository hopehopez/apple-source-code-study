//
//  ZPerson.h
//  ZTest
//
//  Created by zsq on 2021/1/8.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface ZPerson : NSObject{
    NSString *hobby;
}

@property (nonatomic, copy) NSString *nickName;


- (void)sayHello1;
- (void)sayHello2;
- (void)sayHello3;
- (void)sayHello4;
- (void)sayHello5;
- (void)sayHello6;
- (void)sayHello7;
- (void)sayHello8;
- (void)sayHello9;
+ (void)sayHappy;

@end

@interface ZStudent : ZPerson{
    NSString *studentNo;
    int age;
}

@end

@interface ZTeacher : ZPerson{
    int teacher_id;
    NSMutableArray *students;
}

@end



NS_ASSUME_NONNULL_END
