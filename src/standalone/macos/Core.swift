//
// Core.swift Studio Link core (C) running on its own thread
//
// Copyright (C) 2026 studio-link.de
//

import AppKit
import Foundation

/// The Studio Link core (libre/baresip, src/standalone/main.c) as seen by
/// the app. All C calls go through the bridging header (macapp.h).
///
/// Main thread only (except runCore). No Swift concurrency (async/Task/
/// actors) on purpose: on macOS 11 its runtime is not part of the OS and
/// would have to be bundled.
final class Core {
	static let shared = Core()

	enum State: Equatable {
		case idle
		case starting
		case running(port: UInt16)
		case stopping
		/// exit code of sl_mac_core_main (0 = clean shutdown)
		case stopped(code: Int32)
	}

	private(set) var state: State = .idle {
		didSet { onChange?() }
	}

	/// called on the main thread when the state changes
	var onChange: (() -> Void)?

	/// called on the main thread after the core thread has finished
	var onStopped: ((Int32) -> Void)?

	private var thread: Thread?
	/// keeps App Nap away from the core (timers, SIP, audio) while the app
	/// has no visible window
	private var activity: NSObjectProtocol?

	private init() {}

	var port: UInt16? {
		if case .running(let port) = state { return port }
		return nil
	}

	var isRunning: Bool { port != nil }

	var hasStopped: Bool {
		switch state {
		case .idle, .stopped: return true
		default: return false
		}
	}

	var statusText: String {
		switch state {
		case .idle, .starting: return "Studio Link is starting…"
		case .running: return "Studio Link is running"
		case .stopping: return "Studio Link is stopping…"
		case .stopped: return "Studio Link has stopped"
		}
	}

	var software: String { String(cString: sl_mac_software()) }

	var webURL: URL? {
		guard let port = port else { return nil }
		return URL(string: "http://127.0.0.1:\(port)/")
	}

	var logFolder: URL? {
		let path = String(cString: sl_mac_log_path())
		return path.isEmpty ? nil : URL(fileURLWithPath: path,
						 isDirectory: true)
	}

	func start(arguments: [String]) {
		guard state == .idle else { return }
		state = .starting
		activity = ProcessInfo.processInfo.beginActivity(
			options: [.userInitiatedAllowingIdleSystemSleep,
				  .latencyCritical],
			reason: "Studio Link calls and audio")

		sl_mac_set_ready_handler { port in
			// core thread
			DispatchQueue.main.async {
				Core.shared.didBecomeReady(port)
			}
		}

		let args = arguments
		let thread = Thread {
			let code = Core.runCore(args)
			DispatchQueue.main.async { Core.shared.didStop(code) }
		}
		thread.name = "studio-link core"
		thread.qualityOfService = .userInteractive
		// secondary threads get 512 KiB by default, the core was written
		// for a main thread stack (8 MiB)
		thread.stackSize = 8 << 20
		self.thread = thread
		thread.start()
	}

	/// Stop the core, onStopped is called once it has finished
	func stop() {
		switch state {
		case .starting, .running:
			state = .stopping
			sl_mac_quit()
		default:
			break
		}
	}

	func openWebUI() {
		guard let url = webURL else { return }
		NSWorkspace.shared.open(url)
	}

	func openRecordings() {
		guard isRunning else { return }
		sl_mac_open_recordings()
	}

	func openLogs() {
		guard let url = logFolder else { return }
		NSWorkspace.shared.open(url)
	}

	private func didBecomeReady(_ port: UInt16) {
		guard state == .starting else { return }
		state = .running(port: port)
	}

	private func didStop(_ code: Int32) {
		thread = nil
		if let activity = activity {
			ProcessInfo.processInfo.endActivity(activity)
			self.activity = nil
		}
		state = .stopped(code: code)
		onStopped?(code)
	}

	/// Runs on the core thread, blocks until the core has stopped
	private static func runCore(_ args: [String]) -> Int32 {
		// argv for getopt, C strings must stay valid during the call
		var cargs: [UnsafeMutablePointer<CChar>?] = args.map { strdup($0) }
		cargs.append(nil)
		defer { cargs.forEach { free($0) } }

		return cargs.withUnsafeMutableBufferPointer { argv in
			sl_mac_core_main(Int32(args.count), argv.baseAddress)
		}
	}

	/// Command line arguments for the core: drops the arguments AppKit,
	/// Xcode and old Finder versions add (-psn_..., -NSKey value)
	static func coreArguments(
		_ arguments: [String] = CommandLine.arguments) -> [String]
	{
		guard let name = arguments.first else {
			return ["studio-link"]
		}

		var out = [name]
		var it = arguments.dropFirst().makeIterator()

		while let arg = it.next() {
			if arg.hasPrefix("-psn_") {
				continue
			}
			if arg.hasPrefix("-NS") || arg.hasPrefix("-Apple") {
				_ = it.next()	// value
				continue
			}
			out.append(arg)
		}

		return out
	}

	static func describe(error code: Int32) -> String {
		String(cString: sl_mac_strerror(code))
	}
}
