import hoptions from "../templates/options.handlebars";
import { qs, on } from "../ui.js";
"use strict";
window.ws_options_init = function() {
  var ws_options = new WebSocket("ws://" + window.ws_host + "/ws_options");

  var bypass = false;
  var record = false;
  var onair = false;
  var raisehand = false;
  var afk = false;
  var mute = false;

  function toggleButton(sel, active, activeClass, inactiveClass) {
    var btn = qs(sel);
    if (!btn) return;
    btn.classList.toggle(activeClass, active);
    btn.classList.toggle(inactiveClass, !active);
  }


  function RefreshBypass() {
    toggleButton("#btn-bypass", bypass, "btn-danger", "btn-secondary");
  }

  function RefreshRecord() {
    toggleButton("#btn-record", record, "btn-danger", "btn-secondary");
  }

  function RefreshOnair() {
    toggleButton("#btn-onair", onair, "btn-danger", "btn-secondary");
  }

  function RefreshRaisehand() {
    toggleButton("#btn-raise-hand", raisehand, "btn-danger", "btn-secondary");
  }

  function RefreshAFK() {
    toggleButton("#btn-afk", afk, "btn-danger", "btn-secondary");
  }

  function RefreshEventListener() {
    on(".option-change", "click", function() {
      ws_options.send(
        '{"key": "' +
          this.dataset.option +
          '", "value": "' +
          this.dataset.value +
          '"}'
      );
    });
  }

  function RefreshMute() {
    toggleButton("#btn-mute", mute, "btn-danger", "btn-primary");
  }

  function RefreshMonitor() {
    toggleButton("#btn-monitor", mute, "btn-danger", "btn-primary");
  }

  ws_options.onmessage = function(message) {
    var msg = JSON.parse(message.data);

    if (msg.bypass) {
      if (msg.bypass == "true") {
        bypass = true;
      } else {
        bypass = false;
      }
      RefreshBypass();
    }

    if (msg.record) {
      if (msg.record == "true") {
        record = true;
      } else {
        record = false;
      }
      RefreshRecord();
    }

    if (msg.onair) {
      if (msg.onair == "true") {
        onair = true;
      } else {
        onair = false;
      }
      RefreshOnair();
    }

    if (msg.raisehand) {
      if (msg.raisehand == "true") {
        raisehand = true;
      } else {
        raisehand = false;
      }
      RefreshRaisehand();
    }

    if (msg.afk) {
      if (msg.afk == "true") {
        afk = true;
      } else {
        afk = false;
      }
      RefreshAFK();
    }

    if (msg.mute) {
      if (msg.mute == "true") {
        mute = true;
      } else {
        mute = false;
      }
      RefreshMute();
    }

    delete msg.bypass;
    delete msg.record;
    delete msg.onair;
    delete msg.raisehand;
    delete msg.afk;
    delete msg.mute;

    Object.keys(msg)
      .sort()
      .forEach(function(key) {
        var value = msg[key];
        delete msg[key];
        msg[key] = value;
      });

    qs("#options").innerHTML = hoptions(msg);

    RefreshEventListener();
  };

  RefreshEventListener();

  on("#btn-bypass", "click", function() {
    if (bypass) {
      bypass = false;
    } else {
      bypass = true;
    }

    ws_options.send('{"key": "bypass", "value": "' + bypass + '"}');
  });

  on("#btn-mono", "click", function() {
    ws_options.send('{"key": "mono", "value": "true"}');
  });

  on("#btn-stereo", "click", function() {
    ws_options.send('{"key": "mono", "value": "false"}');
  });

  on("#btn-record", "click", function() {
    if (record) {
      record = false;
    } else {
      record = true;
    }

    ws_options.send('{"key": "record", "value": "' + record + '"}');
  });

  on("#btn-record-folder", "click", function() {
    ws_options.send('{"key": "record-folder", "value": "1"}');
  });

  on("#btn-onair", "click", function() {
    if (onair) {
      onair = false;
    } else {
      onair = true;
    }

    ws_options.send('{"key": "onair", "value": "' + onair + '"}');
  });

  on("#btn-raise-hand", "click", function() {
    if (raisehand) {
      raisehand = false;
    } else {
      raisehand = true;
    }

    ws_options.send('{"key": "raisehand", "value": "' + raisehand + '"}');
  });

  on("#btn-afk", "click", function() {
    if (afk) {
      afk = false;
    } else {
      afk = true;
    }

    ws_options.send('{"key": "afk", "value": "' + afk + '"}');
  });

  on("#btn-mute", "click", function() {
    if (mute) {
      mute = false;
    } else {
      mute = true;
    }

    ws_options.send('{"key": "mute", "value": "' + mute + '"}');
  });

  document.onkeydown = function(e) {
    //"shortcut: m"
    if (e.keyCode == 77) {
      // don't toggle mute while typing into a text field
      if (e.target.closest("input, textarea, select, [contenteditable]")) return;
      if (mute) {
        mute = false;
      } else {
        mute = true;
      }

      ws_options.send('{"key": "mute", "value": "' + mute + '"}');
    }
  };
};
