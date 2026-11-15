//
// MainMenu.swift Studio Link main menu (built in code, no nib)
//
// Copyright (C) 2026 studio-link.de
//

import AppKit

enum MainMenu {
	/// Build the main menu, Studio Link actions go to target (AppDelegate)
	static func build(target: AnyObject) -> NSMenu {
		let main = NSMenu()

		// Studio Link (application menu, the title is replaced by macOS)
		let appMenu = submenu(of: main, "Studio Link")
		appMenu.addItem(item("About Studio Link",
				     #selector(AppDelegate.showAbout(_:)), "",
				     target: target))
		appMenu.addItem(.separator())
		appMenu.addItem(item("Show in Menu Bar",
				     #selector(AppDelegate.toggleMenuBar(_:)), "",
				     target: target))
		appMenu.addItem(.separator())
		let services = submenu(of: appMenu, "Services")
		NSApp.servicesMenu = services
		appMenu.addItem(.separator())
		appMenu.addItem(item("Hide Studio Link",
				     #selector(NSApplication.hide(_:)), "h"))
		appMenu.addItem(item("Hide Others",
				     #selector(NSApplication.hideOtherApplications(_:)),
				     "h", modifiers: [.command, .option]))
		appMenu.addItem(item("Show All",
				     #selector(NSApplication.unhideAllApplications(_:)),
				     ""))
		appMenu.addItem(.separator())
		appMenu.addItem(item("Quit Studio Link",
				     #selector(NSApplication.terminate(_:)), "q"))

		// File
		let file = submenu(of: main, "File")
		file.addItem(item("Open Studio Link",
				  #selector(AppDelegate.openWebUI(_:)), "o",
				  target: target))
		file.addItem(.separator())
		file.addItem(item("Open Recordings Folder",
				  #selector(AppDelegate.openRecordings(_:)), "r",
				  modifiers: [.command, .shift], target: target))
		file.addItem(item("Open Log Folder",
				  #selector(AppDelegate.openLogs(_:)), "l",
				  modifiers: [.command, .option], target: target))
		file.addItem(.separator())
		file.addItem(item("Close", #selector(NSWindow.performClose(_:)),
				  "w"))

		// Edit (copy from the About panel and alerts)
		let edit = submenu(of: main, "Edit")
		edit.addItem(item("Copy", #selector(NSText.copy(_:)), "c"))
		edit.addItem(item("Select All", #selector(NSText.selectAll(_:)),
				  "a"))

		// Window
		let window = submenu(of: main, "Window")
		window.addItem(item("Minimize",
				    #selector(NSWindow.performMiniaturize(_:)), "m"))
		NSApp.windowsMenu = window

		// Help
		let help = submenu(of: main, "Help")
		help.addItem(item("Studio Link Help",
				  #selector(AppDelegate.openHelp(_:)), "?",
				  target: target))
		NSApp.helpMenu = help

		return main
	}

	private static func submenu(of parent: NSMenu, _ title: String) -> NSMenu
	{
		let menu = NSMenu(title: title)
		let item = NSMenuItem(title: title, action: nil, keyEquivalent: "")
		item.submenu = menu
		parent.addItem(item)
		return menu
	}

	/// Menu item, target nil = first responder chain
	private static func item(_ title: String, _ action: Selector,
				 _ key: String,
				 modifiers: NSEvent.ModifierFlags = .command,
				 target: AnyObject? = nil) -> NSMenuItem
	{
		let item = NSMenuItem(title: title, action: action,
				      keyEquivalent: key)
		item.keyEquivalentModifierMask = modifiers
		item.target = target
		return item
	}
}
