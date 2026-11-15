import addsip from "../templates/addsip.handlebars";
import editsip from "../templates/editsip.handlebars";
import listsip from "../templates/listsip.handlebars";
import currentlist from "../templates/currentlist.handlebars";
import { qs, qsa, on, render, onEnter, dialog, notify, serializeForm, validateForm } from "../ui.js";
'use strict';
window.ws_baresip_init = function() {
	
	var ws_baresip_sip_accounts = {};
	var ws_baresip_uag_options = "";

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
		render(qs("#accounts"), listsip(ws_baresip_sip_accounts));

		qs("#current_uag").innerHTML = currentlist(ws_baresip_sip_accounts);

		if (qsa("#current_uag option").length > 1) {
			qs("#current_uag").classList.remove("d-none");
		}
		ws_baresip_uag_options = currentlist(ws_baresip_sip_accounts);
		window.ws_baresip_uag_fill();
		for (var key in ws_baresip_sip_accounts) {
			var domain = ws_baresip_sip_accounts[key]['domain'];
			var user = ws_baresip_sip_accounts[key]['user'];
			var state = ws_baresip_sip_accounts[key]['status'];
			if (domain == "studio.link") {
				var label = state ? "Online" : "Offline";
				var dot = qs("#sipdot");

				qs("#sipid").textContent = user + "@" + domain;
				dot.classList.toggle("sl-dot-online", !!state);
				dot.title = label;
				/* announced by screen readers (role=status) */
				if (qs("#sipstatus").textContent !== label)
					qs("#sipstatus").textContent = label;
				qs("#sl-id").classList.remove("d-none");
			}
		};
		RefreshEventListener();
	};


	on("#btn-copy-id", "click", function() {
		var id = qs("#sipid").textContent;
		if (!id)
			return;
		if (!navigator.clipboard) {
			notify("Copy is not supported, please select the ID", "warn");
			return;
		}
		navigator.clipboard.writeText(id).then(function() {
			notify("Copied " + id, "success");
		}, function() {
			notify("Could not copy the ID", "warn");
		});
	});

	onEnter("#sipnumbercall", function(ev) {
		qs("#buttoncall").click();
		ev.target.blur();
	});

	on("#buttoncall", "click", function() {
		ws_baresip.send('{"command": "call", "dial": "'+
			qs("#sipnumbercall").value +'"}');
		qs("#sipnumbercall").value = "";
	});

	/* Account dropdowns of the plugin track cards, all show the current
	 * account (like #current_uag), also called after a card re-render */
	window.ws_baresip_uag_fill = function() {
		qsa(".sl-idle-uag").forEach(function(sel) {
			sel.innerHTML = ws_baresip_uag_options;
			sel.classList.toggle("d-none", sel.options.length < 2);
		});
	};

	on("#idletracks", "change", function(ev) {
		if (!ev.target.matches(".sl-idle-uag"))
			return;

		ws_baresip.send(JSON.stringify({
			command: "uagcurrent",
			aor: ev.target.value
		}));
	});

	/* Call out on a plugin track without a call (idle track card). The
	 * cards are re-rendered by ws_calls.js, so the events are delegated. */
	function idle_call(track) {
		const input = qs("#idle-dial" + track);
		if (!input)
			return;

		ws_baresip.send(JSON.stringify({
			command: "call",
			dial: input.value,
			track: Number(track)
		}));
		input.value = "";
	}

	on("#idletracks", "click", function(ev) {
		const btn = ev.target.closest(".sl-idle-call");
		if (btn)
			idle_call(btn.dataset.track);
	});

	on("#idletracks", "keydown", function(ev) {
		if (ev.key !== "Enter" || !ev.target.matches(".sl-idle-dial"))
			return;

		ev.preventDefault();
		idle_call(ev.target.dataset.track);
		ev.target.blur();
	});

	on("#current_uag", "change", function() {
		ws_baresip.send('{"command": "uagcurrent", "aor": "'+
			qs("#current_uag").value+'"}');
	});


	RefreshEventListener();
};