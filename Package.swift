// swift-tools-version: 6.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "CrowyEngine",
    platforms: [
        .macOS(.v15),
    ],
    targets: [
        .target(
            name: "CrowyMetalSwiftRHI",
            dependencies: [],
            path: "Engine/MetalRHI/Private",
            exclude: [
                "MetalDevice.cpp",
            ],
            publicHeadersPath: nil,
            swiftSettings: [
                .enableExperimentalFeature("StrictConcurrency"),
                .unsafeFlags(
                    [
                        "-import-objc-header",
                        "CrowyMetalSwiftRHI-Bridging-Header.h",
                    ]
                )
            ],
            linkerSettings: [
                .linkedFramework("Metal"),
                .linkedFramework("MetalKit"),
                .linkedFramework("QuartzCore"),
                .linkedFramework("Foundation")
            ]
        ),
    ],
)
