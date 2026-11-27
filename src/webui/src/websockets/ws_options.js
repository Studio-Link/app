import hoptions from "../templates/options.handlebars";
import { qs, on, render, dialog } from "../ui.js";
"use strict";

// Settings shown in the Options tab (labels in options.handlebars)
var OPTION_KEYS = ["monitoring", "monorecord", "monostream", "verbose"];

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
    btn.setAttribute("aria-pressed", active ? "true" : "false");
  }


  function RefreshBypass() {
    toggleButton("#btn-bypass", bypass, "btn-danger", "btn-secondary");
  }

  function RefreshRecord() {
    toggleButton("#btn-record", record, "btn-danger", "btn-secondary");
  }

  function RefreshOnair() {
    toggleButton("#btn-onair", onair, "btn-danger", "btn-secondary");
    var btn = qs("#btn-onair");
    if (btn) {
      btn.title = onair
        ? "You are live: the session audio is being streamed. Click to stop streaming."
        : "Live stream: broadcast the session audio (you and all call participants) via stream.studio-link.de";
    }
  }

  function setOnair(value) {
    onair = value;
    RefreshOnair();
    ws_options.send('{"key": "onair", "value": "' + onair + '"}');
  }

  // Explain what the stream button does before going live
  function confirmOnair() {
    var skip = false;
    try { skip = localStorage.getItem("slstreaminfo") == "hide"; } catch (e) { /* ignore */ }
    if (skip) {
      setOnair(true);
      return;
    }

    dialog({
      title: "Start live stream?",
      message:
        "<p>Streaming broadcasts your <strong>live session audio</strong> " +
        "(your microphone and all call participants) via " +
        "<strong>stream.studio-link.de</strong>. Anyone with the stream link " +
        "can listen in real time.</p>" +
        "<p>A browser window opens where you log in and get the listener " +
        "link. The stream keeps running until you click the button " +
        "again.</p>" +
        "<p class=\"mb-0 text-muted\">This is not a recording. Use " +
        "<em>Record</em> for local recordings.</p>" +
        "<div class=\"form-check mt-3\">" +
        "<input class=\"form-check-input\" type=\"checkbox\" id=\"stream-info-hide\">" +
        "<label class=\"form-check-label\" for=\"stream-info-hide\">Don't show this again</label>" +
        "</div>",
      onEscape: function() {},
      buttons: {
        cancel: { label: "Cancel", className: "btn-secondary" },
        start: {
          label: "Start streaming",
          className: "btn-danger",
          callback: function() {
            var hide = qs("#stream-info-hide");
            if (hide && hide.checked) {
              try { localStorage.setItem("slstreaminfo", "hide"); } catch (e) { /* ignore */ }
            }
            setOnair(true);
          },
        },
      },
    });
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
    toggleButton("#btn-mute", mute, "btn-danger", "btn-secondary");
  }

  // DAW plugin only: host sample rate, Studio Link needs 48 kHz
  function RefreshSamplerate(rate) {
    var row = qs("#samplerate-warning");
    if (!row) return;
    var wrong = !!rate && rate !== "48000";
    if (wrong) qs("#samplerate-value").textContent = rate;
    row.classList.toggle("d-none", !wrong);
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

    RefreshSamplerate(msg.samplerate);

    // Only list known settings, everything else is runtime state
    var settings = {};
    OPTION_KEYS.forEach(function(key) {
      if (key in msg) settings[key] = msg[key];
    });

    render(qs("#options"), hoptions(settings));

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
      setOnair(false);
    } else {
      confirmOnair();
    }
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
