//
//  main.m
//  test
//
//  Created by zsq on 2021/4/23.
//

#import <Foundation/Foundation.h>
#import "ZPerson.h"
#import <objc/runtime.h>
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        NSLog(@"Hello, World!");
        
        
        BOOL re1 = [(id)[NSObject class] isKindOfClass:[NSObject class]];       // 1
        BOOL re2 = [(id)[NSObject class] isMemberOfClass:[NSObject class]];     // 0
        BOOL re3 = [(id)[ZPerson class] isKindOfClass:[ZPerson class]];       // 0
        BOOL re4 = [(id)[ZPerson class] isMemberOfClass:[ZPerson class]];     // 0
        NSLog(@" re1 :%hhd\n re2 :%hhd\n re3 :%hhd\n re4 :%hhd\n",re1,re2,re3,re4);

        BOOL re5 = [(id)[NSObject alloc] isKindOfClass:[NSObject class]];       // 1
        BOOL re6 = [(id)[NSObject alloc] isMemberOfClass:[NSObject class]];     // 1
        BOOL re7 = [(id)[ZPerson alloc] isKindOfClass:[ZPerson class]];       // 1
        BOOL re8 = [(id)[ZPerson alloc] isMemberOfClass:[ZPerson class]];     // 1
        NSLog(@" re5 :%hhd\n re6 :%hhd\n re7 :%hhd\n re8 :%hhd\n",re5,re6,re7,re8);
    }
    return 0;
}
