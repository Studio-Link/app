'use strict';
$(function () {

	function RefreshEventListener() {
		$( ".hangup" ).on( "click", function() {
			ws_calls.send('{"command": "hangup", "key": "'+
				$(this).attr('data-key')+'"}');
		});


		$( ".addcontactshort" ).on( "click", function() {                             
			bootbox.dialog({       
				title: "Add Contact",                                          
				message: Handlebars.templates.addcontact({sip: $(this).attr('data-key')}),          
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
							if (form_validate('formaddcontact')) { 
								ws_contacts.send(JSON.stringify($('#formaddcontact').serializeObject()));                                                     
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
		} else {
			if (swvariant == "standalone") {
				for (var key in msg) {
					if (msg[key]['state'] == 'Established')	{
						established_calls = true;
					}
					var match = msg[key]['peer'].match("sip:(.+@studio-link\.de)");
					if (!match)
						continue;

					for (var contact in ws_contacts_list) {
						msg[key]['contact'] = "";
						if (match[1] == ws_contacts_list[contact]['sip']) {
							msg[key]['contacthide'] = "hide";
							msg[key]['peer'] = ws_contacts_list[contact]['name'];
						}
					}

					if (match[1] == 'echo@studio-link.de') {
						msg[key]['contacthide'] = "hide";
						msg[key]['peer'] = 'echo';
					}
					if (match[1] == 'music@studio-link.de') {
						msg[key]['contacthide'] = "hide";
						msg[key]['peer'] = 'music';
					}
				}

				if (established_calls) {
					if ($( "#btn-mute" ).hasClass("d-none")) {
						$( "#btn-mute" ).removeClass("d-none");
					}
				} else {
					if (!$( "#btn-mute" ).hasClass("d-none")) {
						$( "#btn-mute" ).addClass("d-none");
					}
				}
			}
			var activecalls = require("../templates/activecalls.handlebars");
			$( "#activecalls" ).html(activecalls(msg));
			RefreshEventListener();
		}
	};

	RefreshEventListener();
});
