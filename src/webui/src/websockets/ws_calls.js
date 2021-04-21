'use strict';
window.ws_calls_init = function() {

	function RefreshEventListener() {
		var addcontact = require("../templates/addcontact.handlebars");
		var keyboard = require("../templates/keyboard.handlebars");

		$( ".hangup" ).on( "click", function() {
			ws_calls.send('{"command": "hangup", "key": "'+
				$(this).attr('data-key')+'"}');
		});

		$( ".chmix" ).on( "click", function() {
			ws_calls.send('{"command": "chmix", "key": "'+
				$(this).attr('data-key')+'"}');
		});

		$( ".bufferinc" ).on( "click", function() {
			ws_calls.send('{"command": "bufferinc", "key": "'+
				$(this).attr('data-key')+'"}');
		});
    
		$( ".bufferdec" ).on( "click", function() {
			ws_calls.send('{"command": "bufferdec", "key": "'+
				$(this).attr('data-key')+'"}');
		});

		$( ".volume" ).on( "change", function() {
			ws_calls.send('{"command": "volume", "key": "'+
				$(this).attr('data-key')+'", "value": "'+$(this)[0].value+'" }');
		});

		$( ".keyboard" ).on( "click", function() {
			var call = $(this).attr('data-key');
			bootbox.dialog({
				title: "DTMF Keyboard",
				message: keyboard(),
				buttons: {
					close: {
						label: 'Cancel',
						callback: function() {
							return true;
						}
					}
				}
			}
			);
			$( ".btn-dtmf" ).on( "click", function() {
				ws_calls.send('{"command": "dtmf", "tone": "'+
					$(this).html() +'", "key": "'+ call +'"}');
			});
		});


		$( ".addcontactshort" ).on( "click", function() {                             
			bootbox.dialog({       
				title: "Add Contact",                                          
				message: addcontact({sip: $(this).attr('data-key')}),          
				buttons: {     
					close: {                                               
						label: 'Cancel',                               
						callback: function() {                         
							return true;                           
						}                                              
					},     
					success: {                                             
						label: "Save",                                 
						className: "btn-success btn-formaddcontact",                      
						callback: function () {                        
							if ($("#formaddcontact").parsley().validate()) { 
								window.ws_contacts.send(JSON.stringify($('#formaddcontact').serializeObject()));                                                     
							} else {                               
								return false;                  
							}                                      
						}                                              
					}      
				}              
			}                      
			);
			$('#formaddcontact-name').keypress(function(ev) {
				if (ev.which === 13)
					$('.btn-formaddcontact').click();
			});

			$('#formaddcontact-sip').keypress(function(ev) {
				if (ev.which === 13)
					$('.btn-formaddcontact').click();
			});
		}); 

	}


	function ws_calls_notification(caller) {
		if (!("Notification" in window)) {
			//alert("This browser does not support desktop notification");
		} else if (Notification.permission === "granted") {
			var notification = new Notification("Studio Link call: " + caller);
			notification.onclick = function() {
				window.focus();
			};
		} else if (Notification.permission !== 'denied') {
			Notification.requestPermission(function (permission) {
				// Nothing todo
			});
		}
	}

	var ws_calls = new WebSocket('ws://'+window.ws_host+'/ws_calls');

	ws_calls.onmessage = function (message) {
		var msg = JSON.parse(message.data);
		var established_calls = false;
		if (msg.callback == "INCOMING") {
			$.notify("INCOMING Call", "success");
			ws_calls_notification(msg.peeruri);
			bootbox.confirm("Incoming call from '" + msg.peeruri  + "', accept?", 
					function(result) {
						if (result) {
							ws_calls.send('{"command": "accept", "key": "'+
								msg.key+'"}');
						} else {
							ws_calls.send('{"command": "hangup", "key": "'+
								msg.key+'"}');
						}
					});

		} else if (msg.callback == "CLOSED") {

			bootbox.hideAll();
			$.notify("Call closed: " + msg.message, "warn");

		} else if (msg.callback == "JITTER") {

			var jitters = msg.buffers.split(" ");
			for (let i=0; i<jitters.length; i++) {
				let track = i + 1;
				$("#jitter"+track).html(jitters[i]);
			}

			var talks = msg.talks.split(" ");
			for (let i=0; i<talks.length; i++) {
				let track = i + 1;
				if (talks[i] == 1)
					$("#talk"+track).html("yes");
				else 
					$("#talk"+track).html("no");
			}

		} else {
			if (swvariant == "standalone") {
				for (var key in msg) {
					if (msg[key]['state'] == 'Established')	{
						established_calls = true;
					}
					var match = msg[key]['peer'].match("sip:(.+@studio\.link)");
					if (!match)
						continue;

					for (var contact in ws_contacts_list) {
						msg[key]['contact'] = "";
						if (match[1] == ws_contacts_list[contact]['sip']) {
							msg[key]['contacthide'] = "hide";
							msg[key]['peer'] = ws_contacts_list[contact]['name'];
						}
					}

					if (match[1] == 'echo@studio.link') {
						msg[key]['contacthide'] = "hide";
						msg[key]['peer'] = 'echo';
					}
					if (match[1] == 'music@studio.link') {
						msg[key]['contacthide'] = "hide";
						msg[key]['peer'] = 'music';
					}
				}

				if (established_calls) {
					window.callactive = true;
				} else {
					window.callactive = false;
				}
			}
			var activecalls = require("../templates/activecalls.handlebars");
			$( "#activecalls" ).html(activecalls(msg));
			RefreshEventListener();
		}
	};

	RefreshEventListener();
};
