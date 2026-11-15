//
// MenuBar.swift Studio Link menu bar item (NSStatusItem)
//
// Left click opens the web UI (like the Windows tray icon), right click or
// control-click shows the menu. While the core is not running, every click
// shows the menu.
//
// Copyright (C) 2026 studio-link.de
//

import AppKit

final class MenuBar: NSObject {
	private let core = Core.shared
	private let menu = NSMenu()
	private let statusLine = NSMenuItem(title: "", action: nil,
					    keyEquivalent: "")
	private var item: NSStatusItem?

	/// Studio Link mark (MenuBarIcon.pdf, template image)
	private static let icon: NSImage? = {
		if let url = Bundle.main.url(forResource: "MenuBarIcon",
					     withExtension: "pdf"),
		   let image = NSImage(contentsOf: url) {
			image.isTemplate = true
			return image
		}
		return NSImage(systemSymbolName: "waveform.circle",
			       accessibilityDescription: "Studio Link")
	}()

	/// Menu actions go to target (AppDelegate, also validates the items)
	init(target: AnyObject) {
		super.init()

		statusLine.isEnabled = false
		menu.addItem(statusLine)
		menu.addItem(.separator())
		menu.addItem(Self.menuItem("Open Studio Link",
				       #selector(AppDelegate.openWebUI(_:)), target))
		menu.addItem(.separator())
		menu.addItem(Self.menuItem("Open Recordings Folder",
				       #selector(AppDelegate.openRecordings(_:)),
				       target))
		menu.addItem(Self.menuItem("Open Log Folder",
				       #selector(AppDelegate.openLogs(_:)), target))
		menu.addItem(.separator())
		menu.addItem(Self.menuItem("Quit Studio Link",
				       #selector(NSApplication.terminate(_:)), NSApp))

		update()
	}

	private static func menuItem(_ title: String, _ action: Selector,
				 _ target: AnyObject?) -> NSMenuItem
	{
		let item = NSMenuItem(title: title, action: action,
				      keyEquivalent: "")
		item.target = target
		return item
	}

	var isVisible: Bool {
		get { item != nil }
		set {
			if newValue && item == nil {
				show()
			} else if !newValue, let item = item {
				NSStatusBar.system.removeStatusItem(item)
				self.item = nil
			}
		}
	}

	private func show() {
		let item = NSStatusBar.system.statusItem(
			withLength: NSStatusItem.squareLength)

		if let button = item.button {
			button.image = Self.icon
			button.toolTip = "Studio Link"
			button.setAccessibilityLabel("Studio Link")
			button.target = self
			button.action = #selector(clicked(_:))
			button.sendAction(on: [.leftMouseUp, .rightMouseUp])
		}

		self.item = item
		update()
	}

	/// Refresh status line and icon (core state changed)
	func update() {
		statusLine.title = core.statusText
		// dimmed template image until the core is running
		item?.button?.appearsDisabled = !core.isRunning
	}

	@objc private func clicked(_ sender: Any?) {
		let event = NSApp.currentEvent
		let menuClick = event?.type == .rightMouseUp
			|| event?.modifierFlags.contains(.control) == true

		if menuClick || !core.isRunning {
			showMenu()
		} else {
			core.openWebUI()
		}
	}

	private func showMenu() {
		guard let item = item else { return }

		update()
		// attach the menu only for this click, otherwise every click
		// would open it (and the button action is not called anymore)
		item.menu = menu
		item.button?.performClick(nil)
		item.menu = nil
	}
}
