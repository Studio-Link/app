import audiointerface from "../templates/audiointerface.handlebars";
import { qs, on, dialog } from "../ui.js";
"use strict";
var audio_config = {};

window.ws_audio_init = function() {
  var ws_audio = new WebSocket("ws://" + window.ws_host + "/ws_audio");

  function RefreshEventListener() {
    on("#interface_driver", "change", function() {
      ws_audio.send(
        '{"command": "driver", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#interface_input", "change", function() {
      ws_audio.send(
        '{"command": "input", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#first_input_channel", "change", function() {
      ws_audio.send(
        '{"command": "first_input_channel", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#interface_output", "change", function() {
      ws_audio.send(
        '{"command": "output", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
  }

  ws_audio.onmessage = function(message) {
    audio_config = JSON.parse(message.data);
    var body = qs(".audiointerface-dialog .dialog-body");
    if (body) body.innerHTML = audiointerface(audio_config);
    RefreshEventListener();
  };

  on("#btn-interface", "click", function() {
    var el = dialog({
      title: "Change audio device",
      message: audiointerface(audio_config),
      buttons: {
        reload: {
          label: "Reload",
          callback: function() {
            ws_audio.send('{"command": "reload"}');
            return false;
          },
        },
        close: {
          label: "Close",
          callback: function() {
            return true;
          },
        },
      },
    });
    el.classList.add("audiointerface-dialog");
    RefreshEventListener();
  });
};
