'use strict';
window.ws_baresip_init = function() {
	
	var ws_baresip_sip_accounts = {};

	function ws_baresip_delete_sip(user, domain) {
		ws_baresip.send('{"command": "deletesip", "user": "'+user+'", "domain": "'+domain+'"}');
	}

	function RefreshEventListener() {
		var addsip = require("../templates/addsip.handlebars");
		var editsip = require("../templates/editsip.handlebars");

		$( "#buttonaddsip" ).on( "click", function() {
			bootbox.dialog({
				title: "Add SIP Account",
				message: addsip(),
				buttons: {
					close: {
						label: 'Cancel',
						callback: function() {
							return true;
						}
					},
					success: {
						label: "Save",
						className: "btn-success",
						callback: function () {
							if ($("#formaddsip").parsley().validate()) {
								ws_baresip.send(JSON.stringify($('#formaddsip').serializeObject()));
							} else {
								return false;
							}
						}
					}
				}
			}
			);
			$("#formaddsip").parsley({
				errorClass: 'is-invalid text-danger',
				successClass: 'is-valid', // Comment this option if you don't want the field to become green when valid. Recommended in Google material design to prevent too many hints for user experience. Only report when a field is wrong.
				errorsWrapper: '<span class="form-text text-danger"></span>',
				errorTemplate: '<span></span>',
				trigger: 'change'
			});
		});


		$( ".deletesip" ).on( "click", function() {
			ws_baresip_delete_sip($(this).attr('data-user'), $(this).attr('data-domain'));
		});

		$( ".editsip" ).on( "click", function() {
			var sip_account_edit = {};
			for(var index in ws_baresip_sip_accounts) {
				var user = ws_baresip_sip_accounts[index]["user"];
				var domain = ws_baresip_sip_accounts[index]["domain"];
				if (domain == $(this).attr('data-domain') && user == $(this).attr('data-user')) {
					sip_account_edit = ws_baresip_sip_accounts[index];
				}
			}
			bootbox.dialog({
				title: "Edit SIP Account",
				message: editsip(sip_account_edit),
				buttons: {
					close: {
						label: 'Cancel',
						callback: function() {
							return true;
						}
					},
					success: {
						label: "Save",
						className: "btn-success",
						callback: function () {
							if ($("#formeditsip").parsley().validate()) {
								ws_baresip_delete_sip(sip_account_edit['user'], sip_account_edit['domain']);
								var sip_account_edit_form = $('#formeditsip').serializeObject();
								if (sip_account_edit_form['password'] == "") {
									delete sip_account_edit_form['password'];
								}
								$.extend(sip_account_edit, sip_account_edit_form);
								ws_baresip.send(JSON.stringify(sip_account_edit));
							} else {
								return false;
							}
						}
					}
				}
			}
			);
			$("#formeditsip").parsley();
		});
	}


	var ws_baresip = new WebSocket('ws://'+window.ws_host+'/ws_baresip');

	ws_baresip.onerror = function () {
		$.notify("Websocket error", "error");
	};

	ws_baresip.onclose = function () {
		$.notify("Websocket closed", "error");
	};

	ws_baresip.onmessage = function (message) {
		var msg = JSON.parse(message.data);

		if (msg.callback == "UPDATE") {
			bootbox.dialog({
				title: "Update available",
				message: "Please update to " + msg.version,
				buttons: {
					close: {
						label: 'Cancel',
						callback: function() {
							return true;
						}
					},
					download: {
						label: "Download",
						className: "btn-success",
						callback: function () {
							if (window.swvariant == "standalone") {
								window.open("https://doku.studio-link.de/standalone/installation-standalone.html");
								return false;
							}
							window.open("https://doku.studio-link.de/plugin/installation-plugin-neu.html");
							return false;
						}
					}
				}
			}
			);
			return;
		}

		var listsip = require("../templates/listsip.handlebars");
		var currentlist = require("../templates/currentlist.handlebars");

		ws_baresip_sip_accounts = msg;
		$( "#accounts" ).html(listsip(ws_baresip_sip_accounts));

		$( "#current_uag" ).html(currentlist(ws_baresip_sip_accounts));

		if ($( "#current_uag option" ).length > 1) {
			$( "#current_uag" ).removeClass("d-none");
		}
		for (var key in ws_baresip_sip_accounts) {
			var domain = ws_baresip_sip_accounts[key]['domain'];
			var user = ws_baresip_sip_accounts[key]['user'];
			var state = ws_baresip_sip_accounts[key]['status'];
			if (state) {
				var state_html = '<span class="badge badge-success">OK</span>';
			} else {
				var state_html = '<span class="badge badge-danger">ERROR</span>';
			}

			if (domain == "studio.link") {
				var siphtml = user+"@"+domain;
				var sipstatus = "<b>Status:</b> "+state_html;
				$("#sipid").html(siphtml);
				$("#sipstatus").html(sipstatus);
			}
		};
		RefreshEventListener();
	};


	$('#sipnumbercall').keypress(function(ev) {
		if (ev.which === 13) {
			$('#buttoncall').click();
			$(this).blur();
		}
	});

	$( "#buttoncall" ).on( "click", function() {
		ws_baresip.send('{"command": "call", "dial": "'+
			$( "#sipnumbercall" ).val() +'"}');
		$('#sipnumbercall').val("");
	});

	$( "#current_uag" ).on( "change", function() {
		ws_baresip.send('{"command": "uagcurrent", "aor": "'+
			$( "#current_uag" ).val()+'"}');
	});


	RefreshEventListener();
};