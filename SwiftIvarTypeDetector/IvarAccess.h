//
//  IvarAccess.h
//  XprobePlugin
//
//  Generic access to get/set ivars - functions so they work with Swift.
//
//  $Id: //depot/XprobePlugin/Classes/IvarAccess.h#5 $
//
//  Source Repo:
//  https://github.com/johnno1962/Xprobe/blob/master/Classes/IvarAccess.h
//
//  Created by John Holdsworth on 16/05/2015.
//  Copyright (c) 2015 John Holdsworth. All rights reserved.
//

/*

 This file has the MIT License (MIT)

 Copyright (c) 2015 John Holdsworth

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

 */

#ifndef _IvarAccess_h
#define _IvarAccess_h

#import <Foundation/Foundation.h>

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

@interface XprobeSwift : NSObject
+ (NSString *)convert:(void *)stringPtr;
@end

NSString *utf8String( const char *chars ) {
    return chars ? [NSString stringWithUTF8String:chars] : @"";
}

static const char *isOOType( const char *type ) {
    return strncmp( type, "{OO", 3 ) == 0 ? strstr( type, "\"ref\"" ) : NULL;
}

static BOOL isCFType( const char *type ) {
    return type && strncmp( type, "^{__CF", 6 ) == 0;
}

#pragma mark ivar_getTypeEncoding() for swift

// From Jay Freeman's https://www.youtube.com/watch?v=Ii-02vhsdVk

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
    union {
        Class meta;
        unsigned long flags;
    };
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

static const char *strfmt( NSString *fmt, ... ) NS_FORMAT_FUNCTION(1,2);
static const char *strfmt( NSString *fmt, ... ) {
    va_list argp;
    va_start(argp, fmt);
    return strdup( [[[NSString alloc] initWithFormat:fmt arguments:argp] UTF8String] );
}

static const char *typeInfoForClass( Class aClass, const char *optionals ) {
    return strfmt( @"@\"%@\"%s", NSStringFromClass(aClass), optionals );
}

static const char *skipSwift( const char *typeIdent ) {
    while ( isalpha( *typeIdent ) )
        typeIdent++;
    while ( isnumber( *typeIdent ) )
        typeIdent++;
    return typeIdent;
}

const char *ivar_getTypeEncodingSwift( Ivar ivar, Class aClass ) {
    struct _swift_class *swiftClass = isSwift( aClass );
    if ( !swiftClass )
        return ivar_getTypeEncoding( ivar );

    struct _swift_data *swiftData = swiftClass->swiftData;
    const char *nameptr = swiftData->ivarNames;
    const char *name = ivar_getName(ivar);
    int ivarIndex;

    for ( ivarIndex=0 ; ivarIndex < swiftData->fieldcount ; ivarIndex++ )
        if ( strcmp(name,nameptr) == 0 )
            break;
        else
            nameptr += strlen(nameptr)+1;

    if ( ivarIndex == swiftData->fieldcount )
        return NULL;

    struct _swift_field *field0 = swiftData->get_field_data()[ivarIndex], *field = field0;
    char optionals[100] = "", *optr = optionals;

    // unpack any optionals
    while ( field->flags == 0x2 ) {
        if ( field->optional ) {
            field = field->optional;
            *optr++ = '?';
            *optr = '\000';
        }
        else
            return strfmt( @"%s%s", field->typeInfo->typeIdent, optionals );
    }

    if ( field->flags == 0x1 ) { // rawtype
        const char *typeIdent = field->typeInfo->typeIdent;
        if ( typeIdent[0] == 'V' ) {
            if ( typeIdent[2] == 'C' )
                return strfmt(@"{%@}%s", utf8String( skipSwift( typeIdent ) ), optionals );
            else
                return strfmt(@"{%@}%s", utf8String( skipSwift( skipSwift( typeIdent ) ) ), optionals );
        }
        else
            return field->typeInfo->typeIdent+1;
    }
    else if ( field->flags == 0xa ) // function
        return strfmt( @"^{Block}%s", optionals );
    else if ( field->flags == 0xc ) // protocol
        return strfmt( @"@\"<%@>\"%s", utf8String( field->optional->typeIdent ), optionals );
    else if ( field->flags == 0xe ) // objc class
        return typeInfoForClass( field->objcClass, optionals );
    else if ( field->flags == 0x10 ) // pointer
        return strfmt( @"^{%@}%s", utf8String( skipSwift( field->typeIdent ?: "??" ) ), optionals );
    else if ( (field->flags & 0xff) == 0x55 ) // enum?
        return strfmt( @"e%s", optionals );
    else if ( field->flags < 0x100 || field->flags & 0x3 ) // unknown/bad isa
        return strfmt( @"?FLAGS#%lx(%p)%s", field->flags, field, optionals );
    else // swift class
        return typeInfoForClass( (__bridge Class)field, optionals );
}

#pragma mark generic ivar/method access

static NSString *trapped = @"#INVALID", *notype = @"#TYPE";

NSString *xswiftString( void *iptr ) {
    static Class xprobeSwift;
    if ( !xprobeSwift && !(xprobeSwift = objc_getClass("XprobeSwift")) ) {
#ifdef XPROBE_MAGIC
        NSBundle *thisBundle = [NSBundle bundleForClass:[Xprobe class]];
        NSString *bundlePath = [[thisBundle bundlePath] stringByAppendingPathComponent:@"XprobeSwift.loader"];
        if ( ![[NSBundle bundleWithPath:bundlePath] load] )
            NSLog( @"Xprobe: Could not load XprobeSwift bundle: %@", bundlePath );
        xprobeSwift = objc_getClass("XprobeSwift");
#endif
    }
    return xprobeSwift ? [NSString stringWithFormat:@"\"%@\"", [xprobeSwift convert:iptr]] : @"unavailable";
}

static jmp_buf jmp_env;

static void handler( int sig ) {
    longjmp( jmp_env, sig );
}

int xprotect( void (^blockToProtect)() ) {
    void (*savetrap)(int) = signal( SIGTRAP, handler );
    void (*savesegv)(int) = signal( SIGSEGV, handler );
    void (*savebus )(int) = signal( SIGBUS,  handler );

    int signum;
    switch ( signum = setjmp( jmp_env ) ) {
        case 0:
            blockToProtect();
            break;
        default:
#ifdef XPROBE_MAGIC
            [Xprobe writeString:[NSString stringWithFormat:@"SIGNAL: %d", signum]];
#else
            NSLog( @"SIGNAL: %d", signum );
#endif
    }

    signal( SIGBUS,  savebus  );
    signal( SIGSEGV, savesegv );
    signal( SIGTRAP, savetrap );
    return signum;
}

id xvalueForPointer( id self, void *iptr, const char *type ) {
    if ( !type )
        return notype;
    switch ( type[0] ) {
        case 'V':
        case 'v': return @"void";

        case 'b': // for now, for swift
        case 'B': return @(*(bool *)iptr);// ? @"true" : @"false";

        case 'c': return @(*(char *)iptr);
        case 'C': return [NSString stringWithFormat:@"0x%x", *(unsigned char *)iptr];

        case 's': return @(*(short *)iptr);
        case 'S': return isSwift( [self class] ) ? xswiftString( iptr ) :
            [NSString stringWithFormat:@"0x%x", *(unsigned short *)iptr];

        case 'e': return @(*(int *)iptr);
        case 'f': return @(*(float *)iptr);
        case 'd': return @(*(double *)iptr);

        case 'I': return [NSString stringWithFormat:@"0x%x", *(unsigned *)iptr];
        case 'i':
#ifdef __LP64__
            if ( !isSwift( [self class] ) )
#endif
                return @(*(int *)iptr);

#ifndef __LP64__
        case 'q': return @(*(long long *)iptr);
#else
        case 'q':
#endif
        case 'l': return @(*(long *)iptr);
#ifndef __LP64__
        case 'Q': return @(*(unsigned long long *)iptr);
#else
        case 'Q':
#endif
        case 'L': return @(*(unsigned long *)iptr);

        case '@': {
            __block id out = trapped;

            xprotect( ^{
                uintptr_t uptr = *(uintptr_t *)iptr;
                if ( !uptr )
                    out = nil;
                else if ( uptr & 0xffffffff ) {
                    id obj = *(const id *)iptr;
#ifdef XPROBE_MAGIC
                    [obj description];
#endif
                    out = obj;
                }
            } );

            return out;
        }
        case ':': return [NSString stringWithFormat:@"@selector(%@)",
                          NSStringFromSelector(*(SEL *)iptr)];
        case '#': {
            Class aClass = *(const Class *)iptr;
            return aClass ? [NSString stringWithFormat:@"[%@ class]",
                             NSStringFromClass(aClass)] : @"Nil";
        }
        case '^':
            if ( isCFType( type ) ) {
                char buff[100];
                strcpy(buff, "@\"NS" );
                strcat(buff,type+6);
                strcpy(strchr(buff,'='),"\"");
                return xvalueForPointer( self, iptr, buff );
            }
            return [NSValue valueWithPointer:*(void **)iptr];

        case '{': @try {
            const char *ooType = isOOType( type );
            if ( ooType )
                return xvalueForPointer( self, iptr, ooType+5 );
            if ( type[1] == '?' )
                return xvalueForPointer( self, iptr, "I" );

            // remove names for valueWithBytes:objCType:
            char cleanType[1000], *tptr = cleanType;
            while ( *type )
                if ( *type == '"' ) {
                    while ( *++type != '"' )
                        ;
                    type++;
                }
                else
                    *tptr++ = *type++;
            *tptr = '\000';

            // for incomplete Swift encodings
            if ( strchr( cleanType, '=' ) )
                ;
            else if ( strcmp(cleanType,"{CGFloat}") == 0 )
                return @(*(CGFloat *)iptr);
            else if ( strcmp(cleanType,"{CGPoint}") == 0 )
                strcpy( cleanType, @encode(CGPoint) );
            else if ( strcmp(cleanType,"{CGSize}") == 0 )
                strcpy( cleanType, @encode(CGSize) );
            else if ( strcmp(cleanType,"{CGRect}") == 0 )
                strcpy( cleanType, @encode(CGRect) );
#if TARGET_OS_IPHONE
            else if ( strcmp(cleanType,"{UIOffset}") == 0 )
                strcpy( cleanType, @encode(UIOffset) );
            else if ( strcmp(cleanType,"{UIEdgeInsets}") == 0 )
                strcpy( cleanType, @encode(UIEdgeInsets) );
#else
            else if ( strcmp(cleanType,"{NSPoint}") == 0 )
                strcpy( cleanType, @encode(NSPoint) );
            else if ( strcmp(cleanType,"{NSSize}") == 0 )
                strcpy( cleanType, @encode(NSSize) );
            else if ( strcmp(cleanType,"{NSRect}") == 0 )
                strcpy( cleanType, @encode(NSRect) );
#endif
            else if ( strcmp(cleanType,"{CGAffineTransform}") == 0 )
                strcpy( cleanType, @encode(CGAffineTransform) );

            return [NSValue valueWithBytes:iptr objCType:cleanType];
        }
        @catch ( NSException *e ) {
            return @"raised exception";
        }
        case '*': {
            const char *ptr = *(const char **)iptr;
            return ptr ? utf8String( ptr ) : @"NULL";
        }
#if 0
        case 'b':
            return [NSString stringWithFormat:@"0x%08x", *(int *)iptr];
#endif
        default:
            return @"unknown";
    }
}

id xvalueForIvarType( id self, Ivar ivar, const char *type, Class aClass ) {
    void *iptr = (char *)(__bridge void *)self + ivar_getOffset(ivar);
    return xvalueForPointer( self, iptr, type );
}

id xvalueForIvar( id self, Ivar ivar, Class aClass ) {
    //NSLog( @"%p %p %p %s %s %s", aClass, ivar, isSwift(aClass), ivar_getName(ivar), ivar_getTypeEncoding(ivar), ivar_getTypeEncodingSwift(ivar, aClass) );
    return xvalueForIvarType( self, ivar, ivar_getTypeEncodingSwift(ivar, aClass), aClass );
}

static NSString *invocationException;

id xvalueForMethod( id self, Method method ) {
    @try {
        const char *type = method_getTypeEncoding(method);
        NSMethodSignature *sig = [NSMethodSignature signatureWithObjCTypes:type];
        NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:sig];
        [invocation setSelector:method_getName(method)];
        [invocation invokeWithTarget:self];

        NSUInteger size = 0, align;
        const char *returnType = [sig methodReturnType];
        NSGetSizeAndAlignment(returnType, &size, &align);

        char buffer[size];
        if ( returnType[0] != 'v' )
            [invocation getReturnValue:buffer];
        return xvalueForPointer( self, buffer, returnType );
    }
    @catch ( NSException *e ) {
        NSLog( @"Xprobe: exception on invoke: %@", e );
        return invocationException = [e description];
    }
}

BOOL xvalueUpdateIvar( id self, Ivar ivar, NSString *value ) {
    const char *iptr = (char *)(__bridge void *)self + ivar_getOffset(ivar);
    const char *type = ivar_getTypeEncodingSwift(ivar,[self class]);
    switch ( type[0] ) {
        case 'b': // Swift
        case 'B': *(bool *)iptr = [value boolValue]; break;
        case 'c': *(char *)iptr = [value intValue]; break;
        case 'C': *(unsigned char *)iptr = [value intValue]; break;
        case 's': *(short *)iptr = [value intValue]; break;
        case 'S': *(unsigned short *)iptr = [value intValue]; break;
        case 'e':
        case 'i': *(int *)iptr = [value intValue]; break;
        case 'I': *(unsigned *)iptr = [value intValue]; break;
        case 'f': *(float *)iptr = [value floatValue]; break;
        case 'd': *(double *)iptr = [value doubleValue]; break;
#ifndef __LP64__
        case 'q': *(long long *)iptr = [value longLongValue]; break;
#else
        case 'q':
#endif
        case 'l': *(long *)iptr = (long)[value longLongValue]; break;
#ifndef __LP64__
        case 'Q': *(unsigned long long *)iptr = [value longLongValue]; break;
#else
        case 'Q':
#endif
        case 'L': *(unsigned long *)iptr = (unsigned long)[value longLongValue]; break;
        case ':': *(SEL *)iptr = NSSelectorFromString(value); break;
        default:
            NSLog( @"Xprobe: update of unknown type: %s", type );
            return FALSE;
    }

    return TRUE;
}

#pragma mark HTML representation of type

NSString *xlinkForProtocol( NSString *protocolName ) {
    return [NSString stringWithFormat:@"<a href=\\'#\\' onclick=\\'this.id=\"%@\"; "
            "sendClient( \"protocol:\", \"%@\" ); event.cancelBubble = true; return false;\\'>%@</a>",
            protocolName, protocolName, protocolName];
}

NSString *xtype( const char *type );

NSString *xtypeStar( const char *type, const char *star ) {
    if ( type[-1] == '@' ) {
        if ( type[0] != '"' )
            return @"id";
        else if ( type[1] == '<' )
            type++;
    }
    if ( type[-1] == '^' && type[0] != '{' )
        return [xtype( type ) stringByAppendingString:@" *"];

    const char *end = ++type;
    if ( *end == '?' )
        end = end+strlen(end);
        else
            while ( isalnum(*end) || *end == '_' || *end == ',' || *end == '.' || *end < 0 )
                end++;
    NSData *data = [NSData dataWithBytesNoCopy:(void *)type length:end-type freeWhenDone:NO];
    NSString *typeName = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if ( type[-1] == '<' )
        return [NSString stringWithFormat:@"id&lt;%@&gt;",
                xlinkForProtocol( typeName )];
    else {
        return [NSString stringWithFormat:@"<span onclick=\\'this.id=\"%@\"; "
                "sendClient( \"class:\", \"%@\" ); event.cancelBubble=true;\\'>%@</span>%s",
                typeName, typeName, typeName, star];
    }
}

NSString *xtype_( const char *type ) {
    if ( !type )
        return @"notype";
    switch ( type[0] ) {
        case 'V': return @"oneway void";
        case 'v': return @"void";
        case 'a': return @"Array&lt;?&gt;";
        case 'b': return @"Bool";
        case 'B': return @"bool";
        case 'c': return @"char";
        case 'C': return @"unsigned char";
        case 's': return @"short";
        case 'S': return type[-1] != 'S' ? @"unsigned short" : @"String";
        case 'e': return @"enum";
        case 'i': return @"int";
        case 'I': return @"unsigned";
        case 'f': return @"float";
        case 'd': return @"double";
#ifndef __LP64__
        case 'q': return @"long long";
#else
        case 'q':
#endif
        case 'l': return @"long";
#ifndef __LP64__
        case 'Q': return @"unsigned long long";
#else
        case 'Q':
#endif
        case 'L': return @"unsigned long";
        case ':': return @"SEL";
        case '#': return @"Class";
        case '@': return xtypeStar( type+1, " *" );
        case '^': return xtypeStar( type+1, " *" );
        case '{': return xtypeStar( type, "" );
        case '[': {
            int dim = atoi( type+1 );
            while ( isnumber( *++type ) )
                ;
            return [NSString stringWithFormat:@"%@[%d]", xtype( type ), dim];
        }
        case 'r':
            return [@"const " stringByAppendingString:xtype( type+1 )];
        case '*': return @"char *";
        default:
            return utf8String( type ); //@"id";
    }
}

NSString *xtype( const char *type ) {
    NSString *typeStr = xtype_( type );
    return [NSString stringWithFormat:@"<span class=\\'%@\\' title=\\'%s\\'>%@</span>",
            [typeStr hasSuffix:@"*"] ? @"classStyle" : @"typeStyle", type, typeStr];
}

#endif
