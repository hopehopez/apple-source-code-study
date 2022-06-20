//
//  ViewController.m
//  008-App加载分析
//
//  Created by zsq on 2021/6/21.
//

#import "ViewController.h"

@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view.
}

+ (void)load {
    NSLog(@"%s",  __FUNCTION__);
}

@end
