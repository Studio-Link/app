'use strict';
var audio_config = {};

window.ws_rtaudio_init = function() {
	var ws_rtaudio = new WebSocket('ws://'+window.ws_host+'/ws_rtaudio');
	var audiointerface = require("../templates/audiointerface.handlebars");

	function RefreshEventListener() {
		$( "#interface_driver" ).on( "change", function() {
			ws_rtaudio.send('{"command": "driver", "id": '+
				parseInt($( "#interface_driver" ).val())+'}');
		});
		$( "#interface_input" ).on( "change", function() {
			ws_rtaudio.send('{"command": "input", "id": '+
				parseInt($( "#interface_input" ).val())+'}');
		});
		$( "#interface_output" ).on( "change", function() {
			ws_rtaudio.send('{"command": "output", "id": '+
				parseInt($( "#interface_output" ).val())+'}');
		});
		$( "#interface_monitor" ).on( "change", function() {
			ws_rtaudio.send('{"command": "monitor", "id": '+
				parseInt($( "#interface_monitor" ).val())+'}');
		});
		$( "#btn-audio-loop" ).on( "click", function() {
			if (audio_config.loop) {
				ws_rtaudio.send('{"command": "loop", "value": false}');
			} else {
				ws_rtaudio.send('{"command": "loop", "value": true}');
			}
		});

		if (audio_config.loop) {
			$("#btn-audio-loop").removeClass("btn-default");
			$("#btn-audio-loop").addClass("btn-danger");
			$("#btn-audio-loop").html("Stop Audio Test Loop");
		} else {
			$("#btn-audio-loop").removeClass("btn-danger");
			$("#btn-audio-loop").addClass("btn-default");
			$("#btn-audio-loop").html("Start Audio Test Loop");
		}
	}


	ws_rtaudio.onmessage = function (message) {
		audio_config = JSON.parse(message.data);
		console.log(audio_config);
		$( ".bootbox-body" ).html(audiointerface(audio_config));
		RefreshEventListener();
	};

	$( "#btn-interface" ).on( "click", function() {
		bootbox.dialog({
			title: "Change audio device",
			message: audiointerface(audio_config),
			buttons: {
				close: {
					label: 'Close',
					callback: function() {
						return true;
					}
				}
			}
		});
		RefreshEventListener();
	});
};
