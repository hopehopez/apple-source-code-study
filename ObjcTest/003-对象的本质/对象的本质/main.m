//
//  main.m
//  对象的本质
//
//  Created by zsq on 2021/1/21.
//

#import <Foundation/Foundation.h>
// 对象 -编译 --> 结构体 父类的属性继承过来
// 下节课开始 - 展开分析 类的结构 (对象 - isa)
// 细节分析 -

@interface ZPerson : NSObject{
//    NSString *name; // 成员变量 : 底层编译不会生成相应的 setter getter
    // lgTeacher *t;    // 实例变量 : 是一种特殊的成员变量 (类声明而来 实例 - id (void *))
}
@property (nonatomic, copy) NSString *name; // 属性的区别
@end

@implementation ZPerson

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSLog(@"123");
    }
    return 0;
}

