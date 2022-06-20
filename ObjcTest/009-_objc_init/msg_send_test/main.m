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

void run(){
    NSLog(@"%s",__func__);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        ZPerson *person = [ZPerson alloc];
//        instrumentObjcMessageSends(true);
        [person sayHello];
////        [person performSelector:@selector(sayHello)];
//        instrumentObjcMessageSends(false);
        
        // 发送消息 : objc_msgSend
        // 对象方法 - person - sel
        // 类方法  - 类 - sel
        // 父类 : objc_superMSgSend
        //
        
//        run();
    }
    return 0;
}
