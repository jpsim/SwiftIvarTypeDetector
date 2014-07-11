//
//  Detector.h
//  SwiftIvarTypeDetector
//
//  Created by JP Simard on 7/10/14.
//  Copyright (c) 2014 Realm. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface Detector : NSObject

+ (NSArray *)propertiesForClass:(Class)aClass;

@end
