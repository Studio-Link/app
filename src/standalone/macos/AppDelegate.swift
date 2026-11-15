//
// AppDelegate.swift Studio Link app lifecycle, menus and alerts
//
// Copyright (C) 2026 studio-link.de
//

import AppKit
import AVFoundation

enum Links {
	static let help = URL(string: "https://doku.studio-link.de")!
	static let microphoneSettings = URL(string:
		"x-apple.systempreferences:com.apple.preference.security?Privacy_Microphone")!
}

enum Defaults {
	static let showMenuBar = "showMenuBarExtra"
}


final class AppDelegate: NSObject, NSApplicationDelegate,
			 NSMenuItemValidation
{
	private let core = Core.shared
	private var menuBar: MenuBar?
	private var terminating = false

	func applicationDidFinishLaunching(_ notification: Notification) {
		UserDefaults.standard.register(defaults: [
			Defaults.showMenuBar: true,
		])

		NSApp.mainMenu = MainMenu.build(target: self)

		let menuBar = MenuBar(target: self)
		menuBar.isVisible = UserDefaults.standard.bool(
			forKey: Defaults.showMenuBar)
		self.menuBar = menuBar

		core.onChange = { [weak self] in
			self?.menuBar?.update()
		}
		core.onStopped = { [weak self] code in
			self?.coreDidStop(code)
		}

		requestMicrophoneAccess { [weak self] granted in
			guard let self = self else { return }
			self.core.start(arguments: Core.coreArguments())
			if !granted {
				self.showMicrophoneDenied()
			}
		}
	}

	/// Dock icon clicked or app opened again: show the web UI
	func applicationShouldHandleReopen(_ sender: NSApplication,
					   hasVisibleWindows: Bool) -> Bool
	{
		core.openWebUI()
		return false
	}

	func applicationDockMenu(_ sender: NSApplication) -> NSMenu? {
		let menu = NSMenu()

		for (title, action) in [
			("Open Studio Link", #selector(openWebUI(_:))),
			("Open Recordings Folder", #selector(openRecordings(_:))),
			("Open Log Folder", #selector(openLogs(_:))),
		] {
			let item = NSMenuItem(title: title, action: action,
					      keyEquivalent: "")
			item.target = self
			menu.addItem(item)
		}

		return menu
	}

	// MARK: Actions (main menu, menu bar item, Dock menu)

	@objc func openWebUI(_ sender: Any?) { core.openWebUI() }
	@objc func openRecordings(_ sender: Any?) { core.openRecordings() }
	@objc func openLogs(_ sender: Any?) { core.openLogs() }

	@objc func openHelp(_ sender: Any?) {
		NSWorkspace.shared.open(Links.help)
	}

	@objc func toggleMenuBar(_ sender: Any?) {
		guard let menuBar = menuBar else { return }
		menuBar.isVisible.toggle()
		UserDefaults.standard.set(menuBar.isVisible,
					  forKey: Defaults.showMenuBar)
	}

	@objc func showAbout(_ sender: Any?) {
		NSApp.activate(ignoringOtherApps: true)
		NSApp.orderFrontStandardAboutPanel(options: [
			.credits: NSAttributedString(
				string: core.software,
				attributes: [
					.font: NSFont.systemFont(
						ofSize: NSFont.smallSystemFontSize),
					.foregroundColor: NSColor.secondaryLabelColor,
				]),
		])
	}

	func validateMenuItem(_ item: NSMenuItem) -> Bool {
		switch item.action {
		case #selector(openWebUI(_:)), #selector(openRecordings(_:)):
			return core.isRunning
		case #selector(openLogs(_:)):
			return core.logFolder != nil
		case #selector(toggleMenuBar(_:)):
			item.state = menuBar?.isVisible == true ? .on : .off
			return true
		default:
			return true
		}
	}

	// MARK: Termination

	/// Quit (menu, Dock, logout/shutdown): stop the core cleanly first
	func applicationShouldTerminate(_ sender: NSApplication)
		-> NSApplication.TerminateReply
	{
		if core.hasStopped {
			return .terminateNow
		}

		terminating = true
		core.stop()

		// don't hang forever (e.g. on logout) if the core does not stop
		DispatchQueue.main.asyncAfter(deadline: .now() + 10) {
			[weak self] in
			guard let self = self, self.terminating else { return }
			self.terminating = false
			NSApp.reply(toApplicationShouldTerminate: true)
		}

		return .terminateLater
	}

	private func coreDidStop(_ code: Int32) {
		if terminating {
			terminating = false
			NSApp.reply(toApplicationShouldTerminate: true)
			return
		}

		// stopped by itself: startup error, signal or -t timeout
		// (-2: usage printed for unknown command line options)
		if code != 0 && code != -2 {
			showStartupError(code)
		}
		NSApp.terminate(nil)
	}

	// MARK: Microphone

	private func requestMicrophoneAccess(
		_ completion: @escaping (Bool) -> Void)
	{
		switch AVCaptureDevice.authorizationStatus(for: .audio) {
		case .authorized:
			completion(true)
		case .notDetermined:
			AVCaptureDevice.requestAccess(for: .audio) { granted in
				DispatchQueue.main.async { completion(granted) }
			}
		default:
			completion(false)
		}
	}

	private func showMicrophoneDenied() {
		let alert = NSAlert()
		alert.alertStyle = .warning
		alert.messageText = "Studio Link has no microphone access"
		let settings: String
		let path: String
		if #available(macOS 13, *) {
			settings = "System Settings"
			path = "System Settings > Privacy & Security > Microphone"
		} else {
			settings = "System Preferences"
			path = "System Preferences > Security & Privacy > "
				+ "Privacy > Microphone"
		}
		alert.informativeText = "Your guests will not hear you. Allow "
			+ "Studio Link in \(path) and restart Studio Link."
		alert.addButton(withTitle: "Open \(settings)")
		alert.addButton(withTitle: "Continue")

		NSApp.activate(ignoringOtherApps: true)
		if alert.runModal() == .alertFirstButtonReturn {
			NSWorkspace.shared.open(Links.microphoneSettings)
		}
	}

	private func showStartupError(_ code: Int32) {
		// the log folder is known once the config is loaded
		let logs = core.logFolder?.path ?? "~/.studio-link/logs"
		let alert = NSAlert()
		alert.alertStyle = .critical
		alert.messageText = "Studio Link could not be started"
		alert.informativeText = "\(Core.describe(error: code)) (\(code))."
			+ "\n\nThe log files can help to find the reason, see "
			+ logs
		alert.addButton(withTitle: "Quit")
		if core.logFolder != nil {
			alert.addButton(withTitle: "Open Log Folder")
		}

		NSApp.activate(ignoringOtherApps: true)
		if alert.runModal() == .alertSecondButtonReturn {
			core.openLogs()
		}
	}
}
