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

// Code from here (MIT-licensed): https://github.com/johnno1962/XprobePlugin/blob/master/Classes/Xprobe.mm#L1491
//
//  Created by John Holdsworth on 17/05/2014.
//  Copyright (c) 2014 John Holdsworth. All rights reserved.
//
//  For full licensing term see https://github.com/johnno1962/XprobePlugin
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
//  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
//  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
//  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
//  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

struct _swift_class;

struct _swift_type {
    unsigned long flags;
    const char *typeIdent;
};

struct _swift_field {
    unsigned long flags;
    union {
        struct _swift_type *typeInfo;
        Class objcClass;
    };
    void *unknown;
    struct _swift_field *conditional;
    union {
        struct _swift_class *swiftClass;
        struct _swift_field *subType;
    };
};

struct _swift_data {
    unsigned long flags;
    const char *className;
    int fieldcount, flasg2;
    const char *ivarNames;
    struct _swift_field **(*get_field_data)();
};

struct _swift_class {
    union {
        Class meta;
        unsigned long flags;
    };
    Class supr;
    void *buckets, *vtable, *pdata;
    int size, tos, mds, eight;
    struct _swift_data *swiftData;
};

static const char *typeInfoForName( const char *name ) {
    return strdup([[NSString stringWithFormat:@"@\"%s\"", name] UTF8String]);
}

static const char *typeInfoForClass( Class aClass ) {
    return typeInfoForName( class_getName(aClass) );
}

static const char *ivar_getTypeEncodingSwift( Ivar ivar, Class aClass ) {
    struct _swift_class *swiftClass = (__bridge struct _swift_class *)aClass;
    if ( !((unsigned long)swiftClass->pdata & 0x1) )
        return ivar_getTypeEncoding( ivar );

    struct _swift_data *swiftData = swiftClass->swiftData;
    const char *nameptr = swiftData->ivarNames;
    const char *name = ivar_getName(ivar);
    int ivarIndex;

    for ( ivarIndex=0 ; ivarIndex<swiftData->fieldcount ; ivarIndex++ )
        if ( strcmp(name,nameptr) == 0 )
            break;
        else
            nameptr += strlen(nameptr)+1;

    if ( ivarIndex == swiftData->fieldcount )
        return NULL;

    struct _swift_field **swiftFields = swiftData->get_field_data();
    struct _swift_field *field = swiftFields[ivarIndex];
    struct _swift_class *ivarClass = field->swiftClass;

    // this could probably be tidied up if I knew what was going on...
    if ( field->flags == 0x2 && (field->conditional->flags > 0x2 || (ivarClass && ivarClass->flags>0x2) ) )
        return typeInfoForName(field->typeInfo->typeIdent);
    else if ( field->flags == 0xe )
        return typeInfoForClass(field->objcClass);
    else if ( field->conditional && field->conditional->flags<0xf ) {
        if ( field->conditional->flags == 0xe )
            return typeInfoForClass(field->conditional->objcClass);
        else
            return field->conditional->typeInfo->typeIdent+1;
    }
    else if ( !ivarClass )
        return field->typeInfo->typeIdent+1;
    else if ( ivarClass->flags == 0x1 )
        return field->subType->typeInfo->typeIdent+1;
    else if ( ivarClass->flags == 0xe )
        return typeInfoForClass(field->subType->objcClass);
    else
        return typeInfoForClass((__bridge Class)ivarClass);
}

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
