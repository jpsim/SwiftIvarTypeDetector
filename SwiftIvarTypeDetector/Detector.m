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

// Code from here: https://github.com/johnno1962/XprobePlugin/blob/master/Classes/Xprobe.mm#L1491
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

struct _swift_data {
    unsigned long flags;
    const char *className;
    int fieldcount, flags2;
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
    int f1, f2; // added for Beta5
    int size, tos, mdsize, eight;
    struct _swift_data *swiftData;
    IMP dispatch[1];
};

struct _swift_field {
    unsigned long flags;
    union {
        struct _swift_field *typeInfo;
        const char *typeIdent;
        Class objcClass;
    };
    void *unknown;
    struct _swift_field *optional;
};

static struct _swift_class *isSwift( Class aClass ) {
    struct _swift_class *swiftClass = (__bridge struct _swift_class *)aClass;
    return (uintptr_t)swiftClass->pdata & 0x1 ? swiftClass : NULL;
}

static const char *strfmt( NSString *fmt, ... ) {
    va_list argp;
    va_start(argp, fmt);
    return strdup([[[NSString alloc] initWithFormat:fmt arguments:argp] UTF8String]);
}

static const char *typeInfoForClass( Class aClass ) {
    return strfmt( @"@\"%s\"", class_getName(aClass) );
}

static const char *skipSwift( const char *typeIdent ) {
    while ( isalpha( *typeIdent ) )
        typeIdent++;
    while ( isnumber( *typeIdent ) )
        typeIdent++;
    return typeIdent;
}

static const char *ivar_getTypeEncodingSwift( Ivar ivar, Class aClass ) {
    struct _swift_class *swiftClass = isSwift( aClass );
    if ( !swiftClass )
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

    struct _swift_field *field = swiftData->get_field_data()[ivarIndex];

    // unpack any optionals
    while ( field->flags == 0x2 ) {
        if ( field->optional )
            field = field->optional;
        else
            return field->typeInfo->typeIdent;
    }

    if ( field->flags == 0x1 ) { // rawtype
        const char *typeIdent = field->typeInfo->typeIdent;
        if ( typeIdent[0] == 'V' ) {
            if ( typeIdent[2] == 'C' )
                return strfmt(@"{%s}", skipSwift(typeIdent) );
            else
                return strfmt(@"{%s}", skipSwift(skipSwift(typeIdent)) );
        }
        else
            return field->typeInfo->typeIdent+1;
    }
    else if ( field->flags == 0xa ) // function
        return "^{CLOSURE}";
    else if ( field->flags == 0xc ) // protocol
        return strfmt(@"@\"<%s>\"", field->optional->typeIdent);
    else if ( field->flags == 0xe ) // objc class
        return typeInfoForClass(field->objcClass);
    else if ( field->flags == 0x10 ) // pointer
        return strfmt(@"^{%s}", skipSwift(field->typeIdent?:"??") );
    else if ( field->flags < 0x100 ) // unknown/bad isa
        return strfmt(@"?FLAGS#%d", (int)field->flags);
    else // swift class
        return typeInfoForClass((__bridge Class)field);
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
