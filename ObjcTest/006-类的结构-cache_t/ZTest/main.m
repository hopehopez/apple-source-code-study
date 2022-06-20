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


int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
   
        ZPerson *p = [[ZPerson alloc] init];
        
        [p sayHello1];
        [p sayHello2];
        [p sayHello3];
        [p sayHello4];
        [p sayHello5];
        [p sayHello6];
        [p sayHello7];
        [p sayHello8];
        [p sayHello9];
        
        
    }
    return 0;
}



