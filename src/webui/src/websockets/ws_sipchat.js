import chatmessages from "../templates/chatmessages.handlebars";
import { qs, qsa, on, onEnter } from "../ui.js";
'use strict';

// Not wired into main.js; kept for a possible chat feature.
window.ws_sipchat_init = function() {
	var ws_sipchat = new WebSocket('ws://'+window.ws_host+'/ws_chat');

	function addZero(i) {
		return i < 10 ? "0" + i : i;
	}

	ws_sipchat.onmessage = function (message) {
		var msg = JSON.parse(message.data);
		var elem = qs("#chatmessages");
		elem.innerHTML = chatmessages(msg);

		qsa(".timestamp", elem).forEach(function(ts) {
			var d = new Date(ts.textContent * 1000);
			ts.textContent = addZero(d.getHours()) + ":" +
				addZero(d.getMinutes()) + ":" + addZero(d.getSeconds());
		});
		elem.scrollTop = elem.scrollHeight;
	};

	on("#btn-chat", "click", function() {
		var input = qs("#chatmessage");
		var chatmsg = input.value;
		var peer = qs("#current_contact").value;
		if (chatmsg != "")
			ws_sipchat.send(JSON.stringify({command: "message", text: chatmsg, peer: peer}));
		input.value = "";
	});

	onEnter("#chatmessage", function() {
		qs("#btn-chat").click();
	});
};
