//
//  Detector.m
//  SwiftIvarTypeDetector
//
//  Created by JP Simard on 7/10/14.
//  Copyright (c) 2014 Realm. All rights reserved.
//

#import "Detector.h"
#import <objc/runtime.h>

#pragma mark - Swift ivar encoding detection

#import "IvarAccess.h"

@implementation Detector

+ (NSArray *)propertiesForClass:(Class)aClass {
    NSMutableArray *properties = [NSMutableArray array];
    unsigned int ivarCount;
    Ivar *ivars = class_copyIvarList(aClass, &ivarCount);
    for (unsigned int i = 0; i < ivarCount; i++) {
        NSString *name = [NSString stringWithUTF8String:ivar_getName(ivars[i])];
        NSString *typeEncoding = [NSString stringWithCString:ivar_getTypeEncodingSwift(ivars[i], aClass) encoding:NSUTF8StringEncoding];
        [properties addObject:@{@"name": name, @"typeEncoding": typeEncoding}];
    }
    return properties;
}

@end
