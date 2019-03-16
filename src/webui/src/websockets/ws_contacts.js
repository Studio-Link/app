'use strict';

var ws_contacts_list = {};
var ws_contacts = new WebSocket('ws://'+location.host+'/ws_contacts');

$(function () {

	function ws_contacts_delete(sip) {
		ws_contacts.send('{"command": "deletecontact", "sip": "'+sip+'"}');
	}

	function RefreshEventListener() {
		$( "#buttonaddcontact" ).on( "click", function() {
			bootbox.dialog({
				title: "Add Contact",
				message: Handlebars.templates.addcontact(),
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

		$( ".deletecontact" ).on( "click", function() {
			ws_contacts_delete($(this).attr('data-sip'));
		});

		$( ".callcontact" ).on( "click", function() {
			$('#sipnumbercall').val($(this).attr('data-sip'));
			$('#buttoncall').click();
		});

	}

	ws_contacts.onerror = function () {
		$.notify("Websocket error", "error");
	};

	ws_contacts.onclose = function () {
		$.notify("Websocket closed", "error");
	};

	ws_contacts.onmessage = function (message) {
		var msg = JSON.parse(message.data);
		ws_contacts_list = msg;
		$( "#contacts" ).html(Handlebars.templates.listcontacts(ws_contacts_list));
		RefreshEventListener();
	};

	RefreshEventListener();	
});
