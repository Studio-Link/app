'use strict';
$(function () {

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

	var ws_meter = new WebSocket('ws://'+window.ws_host+'/ws_meter');

	ws_meter.onmessage = function (event) {
		
		var peaks = event.data.split(" ");

		$("#record_time").html(peaks[0]);

		var mic1 = iec_scale(parseFloat(peaks[2])) * 100;
		var mic2 = iec_scale(parseFloat(peaks[4])) * 100;
		var mic3 = iec_scale(parseFloat(peaks[6])) * 100;
		var mic4 = iec_scale(parseFloat(peaks[8])) * 100;
		var mic5 = iec_scale(parseFloat(peaks[10])) * 100;
		var mic6 = iec_scale(parseFloat(peaks[12])) * 100;
		var mic7 = iec_scale(parseFloat(peaks[14])) * 100;
		var mic8 = iec_scale(parseFloat(peaks[16])) * 100;
		var mic9 = iec_scale(parseFloat(peaks[18])) * 100;
		var mic10 = iec_scale(parseFloat(peaks[20])) * 100;
		var mic11 = iec_scale(parseFloat(peaks[22])) * 100;
		var mic12 = iec_scale(parseFloat(peaks[24])) * 100;

		var head1 = iec_scale(parseFloat(peaks[3])) * 100;
		var head2 = iec_scale(parseFloat(peaks[5])) * 100;
		var head3 = iec_scale(parseFloat(peaks[7])) * 100;
		var head4 = iec_scale(parseFloat(peaks[9])) * 100;
		var head5 = iec_scale(parseFloat(peaks[11])) * 100;
		var head6 = iec_scale(parseFloat(peaks[13])) * 100;
		var head7 = iec_scale(parseFloat(peaks[15])) * 100;
		var head8 = iec_scale(parseFloat(peaks[17])) * 100;
		var head9 = iec_scale(parseFloat(peaks[19])) * 100;
		var head10 = iec_scale(parseFloat(peaks[21])) * 100;
		var head11 = iec_scale(parseFloat(peaks[23])) * 100;
		var head12 = iec_scale(parseFloat(peaks[25])) * 100;

		if (peaks[4] != "inf") {
			$("#microphonebar2").removeClass("d-none");
		}
		if (peaks[6] != "inf") {
			$("#microphonebar3").removeClass("d-none");
		}
		if (peaks[8] != "inf") {
			$("#microphonebar4").removeClass("d-none");
		}
		if (peaks[10] != "inf") {
			$("#microphonebar5").removeClass("d-none");
		}
		if (peaks[12] != "inf") {
			$("#microphonebar6").removeClass("d-none");
		}
		if (peaks[14] != "inf") {
			$("#microphonebar7").removeClass("d-none");
		}
		if (peaks[16] != "inf") {
			$("#microphonebar8").removeClass("d-none");
		}
		if (peaks[18] != "inf") {
			$("#microphonebar9").removeClass("d-none");
		}
		if (peaks[20] != "inf") {
			$("#microphonebar10").removeClass("d-none");
		}
		if (peaks[22] != "inf") {
			$("#microphonebar11").removeClass("d-none");
		}
		if (peaks[24] != "inf") {
			$("#microphonebar12").removeClass("d-none");
		}

		$("#microphonebar1").html('<div class="progress-bar '+rainbow(mic1)+'" style="width: '+mic1+'%"> <span class="sr-only">'+mic1+'% Complete (success)</span></div>');
		$("#microphonebar2").html('<div class="progress-bar '+rainbow(mic2)+'" style="width: '+mic2+'%"> <span class="sr-only">'+mic2+'% Complete (success)</span></div>');
		$("#microphonebar3").html('<div class="progress-bar '+rainbow(mic3)+'" style="width: '+mic3+'%"> <span class="sr-only">'+mic3+'% Complete (success)</span></div>');
		$("#microphonebar4").html('<div class="progress-bar '+rainbow(mic4)+'" style="width: '+mic4+'%"> <span class="sr-only">'+mic4+'% Complete (success)</span></div>');
		$("#microphonebar5").html('<div class="progress-bar '+rainbow(mic5)+'" style="width: '+mic5+'%"> <span class="sr-only">'+mic5+'% Complete (success)</span></div>');
		$("#microphonebar6").html('<div class="progress-bar '+rainbow(mic6)+'" style="width: '+mic6+'%"> <span class="sr-only">'+mic6+'% Complete (success)</span></div>');
		$("#microphonebar7").html('<div class="progress-bar '+rainbow(mic7)+'" style="width: '+mic7+'%"> <span class="sr-only">'+mic7+'% Complete (success)</span></div>');
		$("#microphonebar8").html('<div class="progress-bar '+rainbow(mic8)+'" style="width: '+mic8+'%"> <span class="sr-only">'+mic8+'% Complete (success)</span></div>');
		$("#microphonebar9").html('<div class="progress-bar '+rainbow(mic9)+'" style="width: '+mic9+'%"> <span class="sr-only">'+mic9+'% Complete (success)</span></div>');
		$("#microphonebar10").html('<div class="progress-bar '+rainbow(mic10)+'" style="width: '+mic10+'%"> <span class="sr-only">'+mic10+'% Complete (success)</span></div>');

		$("#headphonesbar1").html('<div class="progress-bar '+rainbow(head1)+'" style="width: '+head1+'%"> <span class="sr-only">'+head1+'% Complete (success)</span></div>');
		$("#headphonesbar3").html('<div class="progress-bar '+rainbow(head2)+'" style="width: '+head2+'%"> <span class="sr-only">'+head2+'% Complete (success)</span></div>');
		$("#headphonesbar5").html('<div class="progress-bar '+rainbow(head3)+'" style="width: '+head3+'%"> <span class="sr-only">'+head3+'% Complete (success)</span></div>');
		$("#headphonesbar7").html('<div class="progress-bar '+rainbow(head4)+'" style="width: '+head4+'%"> <span class="sr-only">'+head4+'% Complete (success)</span></div>');
		$("#headphonesbar9").html('<div class="progress-bar '+rainbow(head5)+'" style="width: '+head5+'%"> <span class="sr-only">'+head5+'% Complete (success)</span></div>');
		$("#headphonesbar11").html('<div class="progress-bar '+rainbow(head6)+'" style="width: '+head6+'%"> <span class="sr-only">'+head6+'% Complete (success)</span></div>');
		$("#headphonesbar13").html('<div class="progress-bar '+rainbow(head7)+'" style="width: '+head7+'%"> <span class="sr-only">'+head7+'% Complete (success)</span></div>');
		$("#headphonesbar15").html('<div class="progress-bar '+rainbow(head8)+'" style="width: '+head8+'%"> <span class="sr-only">'+head8+'% Complete (success)</span></div>');
		$("#headphonesbar17").html('<div class="progress-bar '+rainbow(head9)+'" style="width: '+head9+'%"> <span class="sr-only">'+head9+'% Complete (success)</span></div>');
		$("#headphonesbar19").html('<div class="progress-bar '+rainbow(head10)+'" style="width: '+head10+'%"> <span class="sr-only">'+head10+'% Complete (success)</span></div>');
	};
});
