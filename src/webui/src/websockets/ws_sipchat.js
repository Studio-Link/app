'use strict';
$(function () {
	var ws_sipchat = new WebSocket('ws://'+location.host+'/ws_chat');

	ws_sipchat.onmessage = function (message) {
		var msg = JSON.parse(message.data);
		$( "#chatmessages" ).html(Handlebars.templates.chatmessages(msg));
	};

	$( "#btn-chat" ).on( "click", function() {
			var chatmsg = $( "#chatmessage" ).val();
			var peer = $("#current_contact").val();
			if(chatmsg != "")
				ws_sipchat.send('{"command": "message", "text": "'+ chatmsg + '", "peer": "'+ peer +'"}');

			$( "#chatmessage" ).val("");
	});
	
	$('#chatmessage').keypress(function(ev) {
		if (ev.which === 13)
			$('#btn-chat').click();
	});

});
