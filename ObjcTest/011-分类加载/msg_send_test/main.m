//
//  main.m
//  msg_send_test
//
//  Created by zsq on 2021/4/21.
//
#import <Foundation/Foundation.h>
#import "ZPerson.h"
#import <objc/runtime.h>
#import "objc-internal.h"
void printMethodsList(id obj) {
    unsigned int count = 0;
    Method *methods = class_copyMethodList([obj class], &count);
    for (unsigned int i=0; i < count; i++) {
        Method const method = methods[i];
        //获取方法名
        NSString *key = NSStringFromSelector(method_getName(method));
        
        NSLog(@"Method, name: %@", key);
    }
    free(methods);
}

// 类拓展 -- 匿名的分类
// 分类  - read_images
// 类拓展的数据 是如何加载进去的呢?

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        
        ZPerson *person = [ZPerson alloc];
        printMethodsList(person);

    }
    return 0;
}
