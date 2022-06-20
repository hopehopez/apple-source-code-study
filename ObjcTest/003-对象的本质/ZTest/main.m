//
//  main.m
//  ZTest
//
//  Created by zsq on 2021/1/6.
//

#import <Foundation/Foundation.h>
#import "ZPerson.h"

#import <objc/runtime.h>
#import <malloc/malloc.h>

struct LGStruct1 {
    char a;     // 1 + 7        0-7
    double b;   // 8            8-15
    int c;      // 4            16-20
    short d;    // 2 + 2        20-22   + 2
} MyStruct1;

struct LGStruct2 {
    double b;   // 8             0-7
    char a;     // 1 + 3         8-11
    int c;      // 4             12-15
    short d;    // 2             16-17
} MyStruct2;
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        NSLog(@"%lu-%lu",sizeof(MyStruct1),sizeof(MyStruct2));
        
        // size :
        // 1: 对象需要的内存空间 8倍数 - 8字节对齐
        // 2: 最少16 安全 16 - 8  > 16
        
        // 对象开辟空间只有这样
        // 系统真的会按照我们的意愿 -- calloc
        // isa
        ZPerson *person = [ZPerson alloc];
        NSLog(@"%lu - %lu",class_getInstanceSize([person class]),malloc_size((__bridge const void *)(person)));
        
        ZTeacher2  *p = [ZTeacher2 alloc];
        // isa ---- 8
        p.name = @"小明";   // 8
        p.age  = 18;            // 4
        p.height = 185;         // 8
        p.hobby  = @"男";       // 8
        // 36 - 8*5 = 40
        // 48???
        // 对象申请的内存的大小 VS 系统开辟的大小 不一致
        // 前面8字节对齐 - 对象里面的属性
        // 16字节对齐 - 对象
        // 系统开辟 风险 - 内存 连续
        
        // calloc 我们没有进去 + initisa
        
//        p.sex    = 2;
//        p.ch1    = 'a';
//        p.ch2    = 'b';
        NSLog(@"%@",p);
        NSLog(@"%lu - %lu",class_getInstanceSize([p class]),malloc_size((__bridge const void *)(p)));
        // 0x0000001200006261 - 值 内存值 - 内存优化
        // 8 字节 0x00000012- int 4 + 4
        // char 1 + 7
        // char 1 + 7
        // 内存对齐
        
        // 二进制重拍
        
        // isa - 8 - 64 - 分别存了什么
        
        [person sayHello];
    }
    return 0;
}
