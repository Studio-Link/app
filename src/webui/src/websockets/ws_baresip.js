import addsip from "../templates/addsip.handlebars";
import editsip from "../templates/editsip.handlebars";
import listsip from "../templates/listsip.handlebars";
import currentlist from "../templates/currentlist.handlebars";
import { qs, qsa, on, onEnter, dialog, notify, serializeForm, validateForm } from "../ui.js";
'use strict';
window.ws_baresip_init = function() {
	
	var ws_baresip_sip_accounts = {};

	function ws_baresip_delete_sip(user, domain) {
		ws_baresip.send('{"command": "deletesip", "user": "'+user+'", "domain": "'+domain+'"}');
	}

	function RefreshEventListener() {

		on("#buttonaddsip", "click", function() {
			dialog({
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
							if (validateForm(qs("#formaddsip"))) {
								ws_baresip.send(JSON.stringify(serializeForm(qs("#formaddsip"))));
							} else {
								return false;
							}
						}
					}
				}
			}
			);
		});


		on(".deletesip", "click", function() {
			ws_baresip_delete_sip(this.dataset.user, this.dataset.domain);
		});

		on(".editsip", "click", function() {
			var sip_account_edit = {};
			for(var index in ws_baresip_sip_accounts) {
				var user = ws_baresip_sip_accounts[index]["user"];
				var domain = ws_baresip_sip_accounts[index]["domain"];
				if (domain == this.dataset.domain && user == this.dataset.user) {
					sip_account_edit = ws_baresip_sip_accounts[index];
				}
			}
			dialog({
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
							if (validateForm(qs("#formeditsip"))) {
								ws_baresip_delete_sip(sip_account_edit['user'], sip_account_edit['domain']);
								var sip_account_edit_form = serializeForm(qs("#formeditsip"));
								if (sip_account_edit_form['password'] == "") {
									delete sip_account_edit_form['password'];
								}
								Object.assign(sip_account_edit, sip_account_edit_form);
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
		notify("Websocket error", "error");
	};

	ws_baresip.onclose = function () {
		notify("Websocket closed", "error");
	};

	ws_baresip.onmessage = function (message) {
		var msg = JSON.parse(message.data);

		if (msg.callback == "UPDATE") {
			dialog({
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


		ws_baresip_sip_accounts = msg;
		qs("#accounts").innerHTML = listsip(ws_baresip_sip_accounts);

		qs("#current_uag").innerHTML = currentlist(ws_baresip_sip_accounts);

		if (qsa("#current_uag option").length > 1) {
			qs("#current_uag").classList.remove("d-none");
		}
		for (var key in ws_baresip_sip_accounts) {
			var domain = ws_baresip_sip_accounts[key]['domain'];
			var user = ws_baresip_sip_accounts[key]['user'];
			var state = ws_baresip_sip_accounts[key]['status'];
			if (state) {
				var state_html = '<span class="badge text-bg-success">OK</span>';
			} else {
				var state_html = '<span class="badge text-bg-danger">ERROR</span>';
			}

			if (domain == "studio.link") {
				var siphtml = user+"@"+domain;
				var sipstatus = "<b>Status:</b> "+state_html;
				qs("#sipid").innerHTML = siphtml;
				qs("#sipstatus").innerHTML = sipstatus;
			}
		};
		RefreshEventListener();
	};


	onEnter("#sipnumbercall", function(ev) {
		qs("#buttoncall").click();
		ev.target.blur();
	});

	on("#buttoncall", "click", function() {
		ws_baresip.send('{"command": "call", "dial": "'+
			qs("#sipnumbercall").value +'"}');
		qs("#sipnumbercall").value = "";
	});

	on("#current_uag", "change", function() {
		ws_baresip.send('{"command": "uagcurrent", "aor": "'+
			qs("#current_uag").value+'"}');
	});


	RefreshEventListener();
};