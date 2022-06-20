//
//  main.m
//  ZTest
//
//  Created by zsq on 2021/1/6.
//

#import <Foundation/Foundation.h>
#import "ZPerson.h"
#import <objc/runtime.h>
#include <sys/malloc.h>
//#import "objc-internal.h"

void test1() {
    NSObject *objc = [[NSObject alloc] init];
    NSNumber *number = [[[NSNumber alloc] initWithInt:1] copy];
    NSNumber *numberMax = [[NSNumber alloc] initWithLong:NSIntegerMax];

    NSLog(@"objc pointer: %zu malloc: %zu CLASS: %@ ADDRESS: %p", sizeof(objc), malloc_size(CFBridgingRetain(objc)), object_getClass(objc), objc);
    NSLog(@"number pointer: %zu malloc: %zu CLASS: %@ ADDRESS: %p", sizeof(number), malloc_size(CFBridgingRetain(number)), object_getClass(number), number);
    NSLog(@"numberMax pointer: %zu malloc: %zu CLASS: %@ ADDRESS: %p", sizeof(numberMax), malloc_size(CFBridgingRetain(numberMax)), object_getClass(numberMax), numberMax);
  
    //打印结果
//    objc pointer: 8 malloc: 16 CLASS: NSObject ADDRESS: 0x101337090
//    number pointer: 8 malloc: 0 CLASS: __NSCFNumber ADDRESS: 0xb7b35d3f7a9cedbd  // 看这个地址值大概是在栈区
 //   0xb7b35d3f7a9cedbd
 //   1011 0111 1011 0011 0101 1101 0011 1111
 //   0111 1010 1001 1100 1110 1101 1011 1101
    
//    numberMax pointer: 8 malloc: 32 CLASS: __NSCFNumber ADDRESS: 0x101336570 // 看这个地址值大概是在堆区
}

void test2() {
    // 引入 #import "objc-internal.h"
    NSString *str1 = [NSString stringWithFormat:@"a"];
    NSNumber *num1 = [NSNumber numberWithInteger:1];

    //NSLog(@"str1 class: %@", _objc_getClassForTag(_objc_getTaggedPointerTag((__bridge void *)str1)));
    //NSLog(@"num1 class: %@", _objc_getClassForTag(_objc_getTaggedPointerTag((__bridge void *)num1)));

    // 打印结果:
//    str1 class: NSTaggedPointerString
//    num1 class: __NSCFNumber

   
}

void test3() {
    NSString *str1 = [NSString stringWithFormat:@"a"];
    NSString *str2 = [NSString stringWithFormat:@"ab"];
    NSString *str3 = [NSString stringWithFormat:@"abc"];

//    uintptr_t value1 = _objc_getTaggedPointerValue((__bridge void *)str1);
//    uintptr_t value2 = _objc_getTaggedPointerValue((__bridge void *)str2);
//    uintptr_t value3 = _objc_getTaggedPointerValue((__bridge void *)str3);

//    NSLog(@"value1: %lx", value1);
//    NSLog(@"value2: %lx", value2);
//    NSLog(@"value3: %lx", value3);

    // 打印：
//    value1: 611
//    value2: 62612
//    value3: 6362613

//    NSNumber *num1 = [NSNumber numberWithInteger:11];
//    NSNumber *num2 = [NSNumber numberWithInteger:12];
//    NSNumber *num3 = [NSNumber numberWithInteger:13];
//
//    uintptr_t value1 = _objc_getTaggedPointerValue((__bridge void *)num1);
//    uintptr_t value2 = _objc_getTaggedPointerValue((__bridge void *)num2);
//    uintptr_t value3 = _objc_getTaggedPointerValue((__bridge void *)num3);
//
//    NSLog(@"value1: %lx", value1);
//    NSLog(@"value2: %lx", value2);
//    NSLog(@"value3: %lx", value3);

//    // 打印：
//    value1: b3
//    value2: c3
//    value3: d3

    
}
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        
        NSNumber *number = [[[NSNumber alloc] initWithInt:1] copy];
        
        test1();
        
//        test2();
//
//        test3();
        
        
        
    }
    return 0;
}



