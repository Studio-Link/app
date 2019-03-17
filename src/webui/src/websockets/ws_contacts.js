'use strict';

window.ws_contacts_list = {};
var ws_contacts = new WebSocket('ws://'+window.ws_host+'/ws_contacts');
var addcontact = require('../templates/addcontact.handlebars');
var listcontacts = require('../templates/listcontacts.handlebars');

$(function () {


	function ws_contacts_delete(sip) {
		ws_contacts.send('{"command": "deletecontact", "sip": "'+sip+'"}');
	}

	function RefreshEventListener() {

		$( "#buttonaddcontact" ).on( "click", function() {
			bootbox.dialog({
				title: "Add Contact",
				message: addcontact(),
				buttons: {
					close: {
						label: 'Cancel',
						callback: function() {
							return true;
						}
					},
					success: {
						label: "Save",
						className: "btn-primary",
						callback: function () {
							if ($("#formaddcontact").parsley().validate()) {
								ws_contacts.send(JSON.stringify($('#formaddcontact').serializeObject()));
							} else {
								return false;
							}
						}
					}
				}
			}
			);

			$("#formaddcontact").parsley({
				errorClass: 'is-invalid text-danger',
				successClass: 'is-valid', // Comment this option if you don't want the field to become green when valid. Recommended in Google material design to prevent too many hints for user experience. Only report when a field is wrong.
				errorsWrapper: '<span class="form-text text-danger"></span>',
				errorTemplate: '<span></span>',
				trigger: 'change'
			});

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
		console.log(ws_contacts_list);
		$( "#contacts" ).html(listcontacts(ws_contacts_list));
		RefreshEventListener();
	};

	RefreshEventListener();	
});
