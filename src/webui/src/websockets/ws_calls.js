import addcontact from "../templates/addcontact.handlebars";
import keyboard from "../templates/keyboard.handlebars";
import activecalls from "../templates/activecalls.handlebars";
import { Dropdown } from "bootstrap";
import { qs, on, onEnter, dialog, confirm, hideAll, notify, serializeForm, validateForm } from "../ui.js";
'use strict';

var ws_calls_dropdown_open = 0;

window.ws_calls_init = function() {

	function RefreshEventListener() {

        if (ws_calls_dropdown_open) {
            var e = document.getElementById("dropdown"+ws_calls_dropdown_open);
            if (e) {
                Dropdown.getOrCreateInstance(e).show();
                e.scrollIntoView();
            }
        }

        on('.dropdown', 'hide.bs.dropdown', function () {
            ws_calls_dropdown_open = 0;
        });

        // Bootstrap 5 closes dropdowns on clicks inside the menu; keep the
        // Bootstrap 4 behaviour of leaving it open for the buffer/volume forms.
        on('.dropdown-menu form', 'click', function (e) {
            e.stopPropagation();
        });

		on(".hangup", "click", function() {
			ws_calls.send('{"command": "hangup", "key": "'+
				this.dataset.key+'"}');
		});

		on(".chmix", "click", function() {
			ws_calls.send('{"command": "chmix", "key": "'+
				this.dataset.key+'"}');
		});

		on(".bufferinc", "click", function() {
			ws_calls.send('{"command": "bufferinc", "key": "'+
				this.dataset.key+'"}');
            ws_calls_dropdown_open = this.dataset.key;
		});
    
		on(".bufferdec", "click", function() {
			ws_calls.send('{"command": "bufferdec", "key": "'+
				this.dataset.key+'"}');
            ws_calls_dropdown_open = this.dataset.key;
		});

		on(".volume", "change", function() {
			ws_calls.send('{"command": "volume", "key": "'+
				this.dataset.key+'", "value": "'+this.value+'" }');
            ws_calls_dropdown_open = this.dataset.key;
		});

		on(".keyboard", "click", function() {
			var call = this.dataset.key;
			dialog({
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
			on(".btn-dtmf", "click", function() {
				ws_calls.send('{"command": "dtmf", "tone": "'+
					this.textContent.trim() +'", "key": "'+ call +'"}');
			});
		});


		on(".addcontactshort", "click", function() {                             
			dialog({       
				title: "Add Contact",                                          
				message: addcontact({sip: this.dataset.key}),          
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
							if (validateForm(qs("#formaddcontact"))) {
								window.ws_contacts.send(JSON.stringify(serializeForm(qs("#formaddcontact"))));                                                     
							} else {                               
								return false;                  
							}                                      
						}                                              
					}      
				}              
			}                      
			);
			onEnter('#formaddcontact-name, #formaddcontact-sip', function() {
				qs('.btn-formaddcontact').click();
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
			notify("INCOMING Call", "success");
			ws_calls_notification(msg.peeruri);
			confirm("Incoming call from '" + msg.peeruri  + "', accept?", 
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

			hideAll();
			notify("Call closed: " + msg.message, "warn");

		} else if (msg.callback == "JITTER") {

			var jitters = msg.buffers.split(" ");
			for (let i=0; i<jitters.length; i++) {
				let track = i + 1;
				var jitter = qs("#jitter"+track);
				if (jitter) jitter.textContent = jitters[i];
			}

			var talks = msg.talks.split(" ");
			for (let i=0; i<talks.length; i++) {
				let track = i + 1;
				var talk = qs("#talk"+track);
				if (talk) talk.textContent = (talks[i] == 1) ? "yes" : "no";
			}

		} else {
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
                        msg[key]['contacthide'] = "d-none";
                        msg[key]['peer'] = ws_contacts_list[contact]['name'];
                    }
                }

                if (match[1] == 'echo@studio.link') {
                    msg[key]['contacthide'] = "d-none";
                    msg[key]['peer'] = 'echo';
                }

                if (match[1] == 'music@studio.link') {
                    msg[key]['contacthide'] = "d-none";
                    msg[key]['peer'] = 'music';
                }
            }

            if (established_calls) {
                window.callactive = true;
            } else {
                window.callactive = false;
            }

			qs("#activecalls").innerHTML = activecalls(msg);
			RefreshEventListener();
		}
	};

	RefreshEventListener();
};
