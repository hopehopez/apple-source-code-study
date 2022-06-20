//
//  ViewController.m
//  alloc&init探索
//
//  Created by zsq on 2021/1/8.
//

#import "ViewController.h"
#import "Person.h"
@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
    
    Person *p1 = [Person alloc];
    Person *p4 = [Person alloc];
    Person *p2 = [p1 init];
    Person *p3 = [p1 init];
    
    NSLog(@"%@ - %p", p1, &p1);
    NSLog(@"%@ - %p", p2, &p2);
    NSLog(@"%@ - %p", p3, &p3);
    
    
}


@end
