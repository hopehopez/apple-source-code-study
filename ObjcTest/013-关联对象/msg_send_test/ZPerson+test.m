//
//  ZPerson+test.m
//  msg_send_test
//
//  Created by zsq on 2021/8/31.
//

#import "ZPerson+test.h"
#import <objc/runtime.h>
@implementation ZPerson (test)
- (void)setCate_name:(NSString *)cate_name{
    /**
    参数一：id object : 给哪个对象添加属性，这里要给自己添加属性，用self。
    参数二：void * == id key : 属性名，根据key获取关联对象的属性的值，在objc_getAssociatedObject中通过次key获得属性的值并返回。
    参数三：id value : 关联的值，也就是set方法传入的值给属性去保存。
    参数四：objc_AssociationPolicy policy : 策略，属性以什么形式保存。
     
     typedef OBJC_ENUM(uintptr_t, objc_AssociationPolicy) {
         OBJC_ASSOCIATION_ASSIGN = 0,           < Specifies a weak reference to the associated object.
         OBJC_ASSOCIATION_RETAIN_NONATOMIC = 1, < Specifies a strong reference to the associated object.
                                                   The association is not made atomically.
         OBJC_ASSOCIATION_COPY_NONATOMIC = 3,   < Specifies that the associated object is copied.
                                                   The association is not made atomically.
         OBJC_ASSOCIATION_RETAIN = 01401,       < Specifies a strong reference to the associated object.
                                                   The association is made atomically.
         OBJC_ASSOCIATION_COPY = 01403          < Specifies that the associated object is copied.
                                                   The association is made atomically.
     };

    */
    objc_setAssociatedObject(self, "cate_name", cate_name, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    
    
    /**
     存储
     manager -> hashMap 有所有的关联对象 - 总表 - 千千万的 -> 关联表 -> index -> 属性
     哈希
     哈希函数 -> index - key (属性) + 地址 (对象)
     */
}

- (NSString *)cate_name{
    /**
    参数一：id object : 获取哪个对象里面的关联的属性。
    参数二：void * == id key : 什么属性，与objc_setAssociatedObject中的key相对应，即通过key值取出value。
    */
    return  objc_getAssociatedObject(self, "cate_name");
}
@end
