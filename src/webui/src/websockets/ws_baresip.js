'use strict';
$(function () {
	
	var ws_baresip_sip_accounts = {};


	function ws_baresip_delete_sip(user, domain) {
		ws_baresip.send('{"command": "deletesip", "user": "'+user+'", "domain": "'+domain+'"}');
	}

	function RefreshEventListener() {
				

		$( "#buttonaddsip" ).on( "click", function() {
			bootbox.dialog({
				title: "Add SIP Account",
				message: Handlebars.templates.addsip(),
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
							if (form_validate('formaddsip')) {
								ws_baresip.send(JSON.stringify($('#formaddsip').serializeObject()));
							} else {
								return false;
							}
						}
					}
				}
			}
			);
		});

		$( ".deletesip" ).on( "click", function() {
			ws_baresip_delete_sip($(this).attr('data-user'), $(this).attr('data-domain'));
		});

		$( ".editsip" ).on( "click", function() {
			var sip_account_edit = {};
			for(index in ws_baresip_sip_accounts) {
				user = ws_baresip_sip_accounts[index]["user"];
				domain = ws_baresip_sip_accounts[index]["domain"];
				if (domain == $(this).attr('data-domain') && user == $(this).attr('data-user')) {
					sip_account_edit = ws_baresip_sip_accounts[index];
				}
			}
			ws_baresip_sip_accounts
			bootbox.dialog({
				title: "Edit SIP Account",
				message: Handlebars.templates.editsip(sip_account_edit),
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
							if (form_validate('formeditsip')) {
								ws_baresip_delete_sip(sip_account_edit['user'], sip_account_edit['domain']);
								sip_account_edit_form = $('#formeditsip').serializeObject();
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

			if (domain == "studio-link.de") {
				var siphtml = user+"@"+domain;
				var sipstatus = "<b>Status:</b> "+state_html;
				$("#sipid").html(siphtml);
				$("#sipstatus").html(sipstatus);
			}
		};
		RefreshEventListener();
	};


	$('#sipnumbercall').keypress(function(ev) {
		if (ev.which === 13)
			$('#buttoncall').click();
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
});
