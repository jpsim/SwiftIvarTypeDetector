//
//  SwiftIvarTypeDetectorTests.swift
//  SwiftIvarTypeDetector
//
//  Created by JP Simard on 7/10/14.
//  Copyright (c) 2014 Realm. All rights reserved.
//

import XCTest
import SwiftIvarTypeDetector

class GenericClass<T> {}

class SimpleClass: NSObject {}

class ParentClass {
    var boolCol = false
    var intCol = 0
    var floatCol = 0 as Float
    var doubleCol = 0.0
    var stringCol = ""
    var simpleCol = SimpleClass()
    var genericCol = GenericClass<String>()
}

class SwiftIvarTypeDetectorTests: XCTestCase {

    func testGenericPropertyType() {
        let properties = Detector.propertiesForClass(ParentClass.self)
        print("properties: \(properties)")
        XCTAssertNotNil(properties, "class should have properties")
    }
}
