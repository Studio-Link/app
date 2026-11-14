import audiointerface from "../templates/audiointerface.handlebars";
import { qs, on, dialog } from "../ui.js";
"use strict";
var audio_config = {};

window.ws_rtaudio_init = function() {
  var ws_rtaudio = new WebSocket("ws://" + window.ws_host + "/ws_rtaudio");

  function RefreshEventListener() {
    on("#interface_driver", "change", function() {
      ws_rtaudio.send(
        '{"command": "driver", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#interface_input", "change", function() {
      ws_rtaudio.send(
        '{"command": "input", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#first_input_channel", "change", function() {
      ws_rtaudio.send(
        '{"command": "first_input_channel", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#interface_output", "change", function() {
      ws_rtaudio.send(
        '{"command": "output", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#interface_monitor", "change", function() {
      ws_rtaudio.send(
        '{"command": "monitor", "id": ' +
          parseInt(this.value) +
          "}"
      );
    });
    on("#btn-audio-loop", "click", function() {
      if (audio_config.loop) {
        ws_rtaudio.send('{"command": "loop", "value": false}');
      } else {
        ws_rtaudio.send('{"command": "loop", "value": true}');
      }
    });

    var loop = qs("#btn-audio-loop");
    if (loop) {
      loop.classList.toggle("btn-danger", !!audio_config.loop);
      loop.classList.toggle("btn-secondary", !audio_config.loop);
      loop.textContent = audio_config.loop
        ? "Stop Audio Test Loop"
        : "Start Audio Test Loop";
    }
  }

  ws_rtaudio.onmessage = function(message) {
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
            ws_rtaudio.send('{"command": "reload"}');
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
