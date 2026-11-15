//
// main.swift Studio Link Standalone for macOS (process entry point)
//
// AppKit app, replaces the old Objective-C overlay (overlay-standalone-osx).
// The user interface is the web UI in the browser, the app provides:
//
//  - a menu bar item (left click: web UI, right click: menu) and a Dock
//    icon/menu to open the web UI, the recordings and log folders and to
//    quit Studio Link (clean shutdown of the core)
//  - opening the web UI again on Dock click or when the app is started
//    again (macOS keeps a single instance per app bundle)
//  - the microphone permission request and error alerts
//
// The Studio Link core runs on its own thread, see Core.swift and
// src/standalone/macapp.c.
//
// Copyright (C) 2026 studio-link.de
//

import AppKit

let app = NSApplication.shared
let delegate = AppDelegate()

app.delegate = delegate	// weak reference, delegate is kept alive here
app.setActivationPolicy(.regular)
app.run()
