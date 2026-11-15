'use strict';
window.ws_meter_init = function() {

	function iec_scale(db) {
		var def = 0.0;

		if (db < -70.0 || isNaN(db)) {
			def = 0.0;
		} else if (db < -60.0) {
			def = (db + 70.0) * 0.25;
		} else if (db < -50.0) {
			def = (db + 60.0) * 0.5 + 2.5;
		} else if (db < -40.0) {
			def = (db + 50.0) * 0.75 + 7.5;
		} else if (db < -30.0) {
			def = (db + 40.0) * 1.5 + 15.0;
		} else if (db < -20.0) {
			def = (db + 30.0) * 2.0 + 30.0;
		} else if (db < 0.0) {
			def = (db + 20.0) * 2.5 + 50.0;
		} else {
			def = 100.0;
		}

		return (def / 100.0);

	}

	function rainbow(value) {
		if (value > 90) {
			return "bg-danger";
		} else if (value > 65) {
			return "bg-warning";
		}
		return "bg-success";
	}

	var CHANNELS = ["Left", "Right"];

	// A stereo meter is a group (role="group", aria-label set in the
	// markup) holding one role="meter" bar per channel. The bars are
	// created lazily, so templates only need the empty container.
	function stereoBars(el) {
		if (el._bars)
			return el._bars;

		el.textContent = "";
		el._bars = CHANNELS.map(function (name) {
			var meter = document.createElement("div");
			meter.className = "progress";
			meter.setAttribute("role", "meter");
			meter.setAttribute("aria-label", name);
			meter.setAttribute("aria-valuemin", "0");
			meter.setAttribute("aria-valuemax", "100");
			meter.setAttribute("aria-valuenow", "0");

			var bar = document.createElement("div");
			bar.className = "progress-bar bg-success";
			bar.setAttribute("aria-hidden", "true");
			bar.style.width = "0%";

			meter.appendChild(bar);
			el.appendChild(meter);
			return { meter: meter, bar: bar };
		});

		return el._bars;
	}

	function setBar(b, value) {
		var color = rainbow(value);

		b.bar.style.width = value + "%";
		if (b.color !== color) {
			b.bar.classList.remove("bg-success", "bg-warning",
				"bg-danger");
			b.bar.classList.add(color);
			b.color = color;
		}
		b.meter.setAttribute("aria-valuenow", Math.round(value));
	}

	function setStereoMeter(el, left, right) {
		var bars = stereoBars(el);

		setBar(bars[0], left);
		setBar(bars[1], right);
	}

	var ws_meter = new WebSocket('ws://'+window.ws_host+'/ws_meter');

	ws_meter.onmessage = function (event) {

		var peaks = event.data.split(" ");

		document.getElementById("record_time").textContent = peaks[0];

		// peaks: [time, 0, <slot 0 L>, <slot 0 R>, <slot 1 L>, ...]
		function level(slot, c) {
			return iec_scale(parseFloat(peaks[2 + slot * 2 + c])) * 100;
		}

		// Meter slots: even = input (local/plugin instance),
		// odd = playback of the matching call track
		for (var ch = 1; ch <= 10; ch++) {
			var mic = (ch - 1) * 2;
			var head = mic + 1;

			var micbar = document.getElementById("microphonebar" + ch);
			if (micbar) {
				// additional inputs (plugin instances) appear once
				// they deliver audio
				if (ch > 1 && peaks[2 + mic * 2] != "inf")
					micbar.classList.remove("d-none");
				setStereoMeter(micbar, level(mic, 0), level(mic, 1));
			}

			// headphone bars are rendered per call track (ids 1, 3, 5, ...)
			var headbar = document.getElementById("headphonesbar" + head);
			if (headbar)
				setStereoMeter(headbar, level(head, 0),
					level(head, 1));
		}
	};
};
