//
//  main.m
//  指针偏移
//
//  Created by zsq on 2021/2/19.
//

#import <Foundation/Foundation.h>

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
//        NSLog(@"Hello, World!");
        
        
        //基本类型
        int a, b;
        
        a = 10;
        b = a;
        a = 20;
        
//        NSLog(@"%d---%p", a, &a);
//        NSLog(@"%d---%p", b, &b);
        
        NSObject *p1 = [NSObject alloc];
        NSObject *p2 = p1;
        NSObject *p3 = [NSObject alloc];
        
        
        int c[4] = {1, 2, 3, 4};
        NSLog(@"%p - %p - %p - %p", &c, &c[0], &c[1], &c[2]);
        NSLog(@"%d - %d - %d - %d", *c, *(c+1), *(c+2), *(c+3));
        
        
    }
    return 0;
}
