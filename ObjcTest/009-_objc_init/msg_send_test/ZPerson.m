//
//  ZPerson.m
//  ZTest
//
//  Created by zsq on 2021/1/8.
//

#import "ZPerson.h"
#import <objc/message.h>

@implementation ZStudent

- (void)sayHello{
    NSLog(@"学生说: 你好");
}


@end

@implementation ZPerson
//- (void)sayHello{
//    NSLog(@"你好");
//}

- (id)forwardingTargetForSelector:(SEL)aSelector{
    NSLog(@"快速转发 forwardingTargetForSelector: %@", NSStringFromSelector(aSelector));
    if (aSelector == @selector(sayHello)) {
        return [ZStudent alloc];
    }

    return [super forwardingTargetForSelector:aSelector];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)aSelector{

    NSLog(@"慢速转发 methodSignatureForSelector: %@", NSStringFromSelector(aSelector));
    if (aSelector == @selector(sayHello)) {
        NSMethodSignature *methodSignature = [NSMethodSignature signatureWithObjCTypes:"v@:"];
        return  methodSignature;
    }
    return  [super methodSignatureForSelector:aSelector];
}

- (void)forwardInvocation:(NSInvocation *)anInvocation{
    NSLog(@"慢速转发 forwardInvocation: %@", anInvocation);
//    if (anInvocation.selector == @selector(sayHello)) {
//        [anInvocation invokeWithTarget:[ZStudent alloc]];
//    } else {
        [super forwardInvocation:anInvocation];
//    }
}

- (void)sayNB{
    NSLog(@"你牛逼");
}

+ (BOOL)resolveInstanceMethod:(SEL)sel{
    NSLog(@"进入实例方法动态决议: %@", NSStringFromSelector(sel));

//    if (sel == @selector(sayHello)) {
//        NSLog(@"执行 sayHello 的方法决议");
//
//        IMP sayHelloIMP = class_getMethodImplementation(self, @selector(sayNB));
//
//        Method sayHelloMethod = class_getInstanceMethod(self, @selector(sayNB));
//
//        const char *sayHelloType = method_getTypeEncoding(sayHelloMethod);
//
//        return class_addMethod(self, sel, sayHelloIMP, sayHelloType);
//    }
    return [super resolveInstanceMethod:sel];
}

//- (void)doesNotRecognizeSelector:(SEL)aSelector{
//    NSLog(@"doesNotRecognizeSelector: %@", NSStringFromSelector(aSelector));
//    [super doesNotRecognizeSelector:aSelector];
//}

//+ (void)load{
//    NSLog(@"ZPerson +lond");
//}

@end
