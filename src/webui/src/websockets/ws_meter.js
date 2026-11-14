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

	function rainbow(db) {
		if (db > 90) {
			return "bg-danger";
		} else if (db > 65) {
			return "bg-warning";
		}
		return "bg-success";
	}

	function progressbar(value) {
		return '<div class="progress-bar ' + rainbow(value) + '" style="width: ' + value +
			'%"> <span class="visually-hidden">' + value + '% Complete (success)</span></div>';
	}

	var ws_meter = new WebSocket('ws://'+window.ws_host+'/ws_meter');

	ws_meter.onmessage = function (event) {
		
		var peaks = event.data.split(" ");

		document.getElementById("record_time").textContent = peaks[0];

		// peaks: [time, ?, mic1, head1, mic2, head2, ...]
		for (var ch = 1; ch <= 10; ch++) {
			var mic = iec_scale(parseFloat(peaks[ch * 2])) * 100;
			var head = iec_scale(parseFloat(peaks[ch * 2 + 1])) * 100;

			var micbar = document.getElementById("microphonebar" + ch);
			if (ch > 1 && peaks[ch * 2] != "inf")
				micbar.classList.remove("d-none");
			micbar.innerHTML = progressbar(mic);

			// headphone bars are rendered per call track (ids 1, 3, 5, ...)
			var headbar = document.getElementById("headphonesbar" + (ch * 2 - 1));
			if (headbar)
				headbar.innerHTML = progressbar(head);
		}
	};
};
