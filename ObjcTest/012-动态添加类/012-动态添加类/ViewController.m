//
//  ViewController.m
//  012-动态添加类
//
//  Created by zsq on 2021/9/1.
//

#import "ViewController.h"
#import <objc/runtime.h>
#import "ZStudent.h"
void z_class_addProperty(Class targetClass, const char *propertyName) {
    objc_property_attribute_t type = {"T", [[NSString stringWithFormat:@"@\"%@\"", NSStringFromClass([NSString class])] UTF8String]};//type
    
    objc_property_attribute_t ownership0 = {"C", ""}; //C = copy
    objc_property_attribute_t ownership = {"N", ""}; // N = nonatomic
    objc_property_attribute_t backingivar = {"V",[NSString stringWithFormat:@"_%@", [NSString stringWithCString:propertyName encoding:NSUTF8StringEncoding]].UTF8String };//variable name 变量名加下滑线 _
    objc_property_attribute_t attributes[] = {type, ownership0, ownership, backingivar};
    
    class_addProperty(targetClass, propertyName, attributes, 4);
    
}

void z_printerProperty(Class targetClass) {
    unsigned int outCount, i;
    objc_property_t *properties = class_copyPropertyList(targetClass, &outCount);
    for (i=0; i<outCount; i++) {
        objc_property_t property = properties[i];
        fprintf(stdout, "%s %s\n", property_getName(property), property_getAttributes(property));
    }
}


void zSetter(NSString *value){
    printf("%s/n",__func__);
}

NSString *zGetter(){
    printf("%s/n",__func__);
    return @"master NB";
}
@interface ViewController ()

@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    
    
    //1.动态创建类
    Class ZPerson = objc_allocateClassPair([NSObject class], "ZPerson", 0);
    
    //2.添加成员变量 1<<aligment
    // ivar - ro - ivarlist
    class_addIvar(ZPerson, "name", sizeof(NSString *), log2(sizeof(NSString *)), "@");
    
    //3.注册到内存
    objc_registerClassPair(ZPerson);
    
    // cls->ISA()->changeInfo(RW_CONSTRUCTED, RW_CONSTRUCTING | RW_REALIZING);
    // cls->changeInfo(RW_CONSTRUCTED, RW_CONSTRUCTING | RW_REALIZING);
    
    //3.1 添加property - rw
    z_class_addProperty(ZPerson, "sex");
    z_printerProperty(ZPerson);
    
    // 3.2 添加setter  +  getter 方法
    class_addMethod(ZPerson, @selector(setSex:), (IMP)zSetter, "v@:@");
    class_addMethod(ZPerson, @selector(sex), (IMP)zGetter, "@@:");
    
    // 开始使用
    id person = [ZPerson alloc];
    [person setValue:@"hope" forKey:@"name"];
    [person setValue:@"man" forKey:@"sex"];
    
    NSLog(@"name: %@",[person valueForKey:@"name"]);
    NSLog(@"sex: %@",[person valueForKey:@"sex"]);

}


// 相关api 的注释

/**
 * 创建类对
 *superClass: 父类，传Nil会创建一个新的根类
 *name: 类名
 *extraBytes: 0
 *return:返回新类，创建失败返回Nil，如果类名已经存在，则创建失败
  objc_allocateClassPair(<#Class  _Nullable __unsafe_unretained superclass#>, <#const char * _Nonnull name#>, <#size_t extraBytes#>)
 */


/**
 *添加成员变量
 *
 *cls 往哪个类添加
 *name 添加的名字
 *size 大小
 *alignment 对齐处理方式
 *types 签名
 *
 *这个函数只能在objc_allocateClassPair和objc_registerClassPair之前调用。不支持向现有类添加一个实例变量。
 *这个类不能是元类。不支持在元类中添加一个实例变量。
 *实例变量的最小对齐为1 << align。实例变量的最小对齐依赖于ivar的类型和机器架构。对于任何指针类型的变量，请通过log2(sizeof(pointer_type))。
  class_addIvar(<#Class  _Nullable __unsafe_unretained cls#>, <#const char * _Nonnull name#>, <#size_t size#>, <#uint8_t alignment#>, <#const char * _Nullable types#>)
 */

/**
 *往内存注册类
 *
 * cls 要注册的类
 *
 * objc_registerClassPair(<#Class  _Nonnull __unsafe_unretained cls#>)
 */


/**
 *往类里面添加方法
 *
 *cls 要添加方法的类
 *sel 方法编号
 *imp 函数实现指针
 *types 签名
 *
 *class_addMethod(<#Class  _Nullable __unsafe_unretained cls#>, <#SEL  _Nonnull name#>, <#IMP  _Nonnull imp#>, <#const char * _Nullable types#>)
 */



/**
 *往类里面添加属性
 *
 *cls 要添加属性的类
 *name 属性名字
 *attributes 属性的属性数组。
 *attriCount 属性中属性的数量。
 *
 *class_addProperty(<#Class  _Nullable __unsafe_unretained cls#>, <#const char * _Nonnull name#>, <#const objc_property_attribute_t * _Nullable attributes#>, <#unsigned int attributeCount#>)
 */

@end
