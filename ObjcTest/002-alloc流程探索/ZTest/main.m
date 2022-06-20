//
//  main.m
//  ZTest
//
//  Created by zsq on 2021/1/6.
//

#import <Foundation/Foundation.h>
#import "ZPerson.h"

union data{
    int i;
    char ch;
    double d;
    struct {
        unsigned m: 4;
        unsigned n: 4;
    };
};

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        // insert code here...
        NSLog(@"Hello, World!");
        
        ZPerson *p1 = [ZPerson alloc];
        ZPerson *p2 = [p1 init];
        ZPerson *p3 = [p1 init];
        
        NSLog(@"%@ - %p", p1, &p1);
        NSLog(@"%@ - %p", p2, &p2);
        NSLog(@"%@ - %p", p3, &p3);
        
        
        union data a;
        a.m = 1;
        
        
        
    }
    return 0;
}
