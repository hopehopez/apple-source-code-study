//
//  main.m
//  isa与类
//
//  Created by zsq on 2021/1/20.
//

#import <Foundation/Foundation.h>
#import <objc/runtime.h>

//MARK: - 分析类对象内存存在个数
void testClassNum(){
    Class class1 = [NSObject class];
    Class class2 = [NSObject alloc].class;
    Class class3 = object_getClass([NSObject alloc]);
    Class class4 = [NSObject alloc].class;
    NSLog(@"\n%p-\n%p-\n%p-\n%p",class1,class2,class3,class4);
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        NSLog(@"Hello, World!");
        
        Class cls = NSObject.class;
        NSObject *p1 = [NSObject alloc];
        NSObject *p2 = [NSObject alloc];
        
        NSLog(@"class: %@, p1: %@, p2: %@", cls, p1, p2);
        
        testClassNum();
    }
    return 0;
}
