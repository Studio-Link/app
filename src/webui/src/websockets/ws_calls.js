import addcontact from "../templates/addcontact.handlebars";
import keyboard from "../templates/keyboard.handlebars";
import activecalls from "../templates/activecalls.handlebars";
import idletracks from "../templates/idletracks.handlebars";
import { Dropdown } from "bootstrap";
import { qs, on, render, onEnter, dialog, confirm, hideAll, notify, serializeForm, validateForm } from "../ui.js";
'use strict';

var ws_calls_dropdown_open = 0;

/* Sliding window (~2 s at 250 ms reports) for the recent packet loss rate */
const NETSTATUS_WINDOW = 8;
var netstatus_history = {};

/* Plugin: tracks owned by a plugin instance (from STATS) and tracks with a
 * call (from the calls list), the difference is shown as empty slots */
var instance_tracks = [];
var call_tracks = new Set();
var idletracks_shown = null;

function idletracks_update() {
	const idle = instance_tracks.filter((t) => !call_tracks.has(t));
	const key = idle.join(",");

	if (key === idletracks_shown)
		return;

	idletracks_shown = key;
	render(qs("#idletracks"), idletracks(idle.map((t) => ({ track: t }))));
	if (window.ws_baresip_uag_fill)
		window.ws_baresip_uag_fill();
}

function fmt_kbit(bps) {
	return (bps / 1000).toFixed(0) + " kbit/s";
}

function fmt_ms_us(us) {
	return (us / 1000).toFixed(1) + " ms";
}

/* Recent loss rate in percent, from jitter buffer counters (received
 * frames vs. lost and too-late-dropped frames). */
function netstatus_loss(t) {
	let h = netstatus_history[t.track];
	if (!h || !t.jbuf || !t.active) {
		h = netstatus_history[t.track] = [];
		if (!t.jbuf || !t.active)
			return null;
	}

	const lost = t.jbuf.lost + t.jbuf.late_lost;
	/* counters reset (new call) */
	if (h.length && (t.jbuf.put < h[h.length-1].put ||
			lost < h[h.length-1].lost))
		h.length = 0;

	h.push({ put: t.jbuf.put, lost: lost });
	if (h.length > NETSTATUS_WINDOW)
		h.shift();

	const put = h[h.length-1].put - h[0].put;
	const dlost = h[h.length-1].lost - h[0].lost;
	if (put + dlost <= 0)
		return null;

	return 100 * dlost / (put + dlost);
}

function netstatus_quality(t, loss) {
	/* no RTP received yet */
	if (!t.active || !t.rx_packets)
		return { cls: "netstatus-none", text: "no data" };

	/* loss unknown (no jitter buffer stats): rate by RTT/jitter only */
	if (loss === null)
		loss = 0;

	const rtt = t.rtt || 0;        /* us */
	const jit = t.rx_jitter || 0;  /* us */

	if (loss > 5 || rtt > 400000 || jit > 60000)
		return { cls: "netstatus-bad", text: "bad" };
	if (loss > 1 || rtt > 200000 || jit > 30000)
		return { cls: "netstatus-fair", text: "fair" };

	return { cls: "netstatus-good", text: "good" };
}

function netstatus_update(t) {
	const el = qs("#netstatus" + t.track);
	if (!el)
		return;

	const loss = netstatus_loss(t);
	const q = netstatus_quality(t, loss);

	const icon = el.querySelector(".netstatus-icon");
	if (icon) {
		icon.classList.remove("netstatus-none", "netstatus-good",
				      "netstatus-fair", "netstatus-bad");
		icon.classList.add(q.cls);
	}

	const label = el.querySelector(".netstatus-label");
	if (label)
		label.textContent = "Network quality: " + q.text;

	const set = (key, val) => {
		const td = el.querySelector('[data-stat="' + key + '"]');
		if (td)
			td.textContent = val;
	};

	if (!t.active) {
		el.querySelectorAll("[data-stat]").forEach(
			(td) => td.textContent = "-");
		set("quality", q.text);
		return;
	}

	set("quality", q.text + (loss !== null ?
		" (" + loss.toFixed(1) + " % loss)" : ""));
	set("rtt", t.rtt ? fmt_ms_us(t.rtt) : "-");
	set("bitrate", fmt_kbit(t.rx_bitrate) + " / " +
		fmt_kbit(t.tx_bitrate));
	set("packets", t.rx_packets + " / " + t.tx_packets);
	set("lost", (t.rx_lost ?? "-") + " / " + (t.tx_lost ?? "-"));
	set("jitter", t.rx_jitter !== undefined ?
		fmt_ms_us(t.rx_jitter) + " / " + fmt_ms_us(t.tx_jitter) : "-");
	set("buffer", t.buffer + " ms");

	const jb = t.jbuf;
	if (jb) {
		set("jb_delay", jb.delay + " ms");
		set("jb_jitter", jb.jitter + " ms");
		set("jb_skew", jb.skew + " ms");
		set("jb_packets", jb.packets);
		set("jb_lost", jb.lost);
		set("jb_late", jb.late + " / " + jb.late_lost);
		set("jb_oos", jb.oos + " / " + jb.dups);
		set("jb_overflow", jb.overflow + " / " + jb.flush);
	}
}

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

		on(".netstatus", "keydown", function(e) {
			if (e.key === "Escape")
				this.classList.add("netstatus-dismissed");
		});
		on(".netstatus", "focusout", function() {
			this.classList.remove("netstatus-dismissed");
		});
		on(".netstatus", "mouseleave", function() {
			this.classList.remove("netstatus-dismissed");
		});

		on(".hangup", "click", function() {
			ws_calls.send('{"command": "hangup", "key": "'+
				this.dataset.key+'"}');
		});

		on(".chmix", "click", function() {
			ws_calls.send('{"command": "chmix", "key": "'+
				this.dataset.key+'"}');
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

		} else if (msg.callback == "STATS") {

			for (const t of msg.tracks) {
				netstatus_update(t);
			}

			instance_tracks = msg.tracks
				.filter((t) => t.instance && !t.call)
				.map((t) => t.track);
			idletracks_update();

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

			call_tracks = new Set(Object.values(msg).map((c) => c.track));
			idletracks_update();

			render(qs("#activecalls"), activecalls(msg));
			RefreshEventListener();
		}
	};

	RefreshEventListener();
};
