import Handlebars from "handlebars/runtime";
import changelog from "./templates/changelog.handlebars";
import { startOnboarding } from "./onboarding.js";
import { alert } from "./ui.js";

Handlebars.registerHelper("inc", function(value, options) {
  return parseInt(value) + 1;
});

Handlebars.registerHelper("mysip", function(value, options) {
  if (value.match("sip:(.+@studio.link)"))
    return value.match("sip:(.+@studio.link)")[1];
  return value;
});

Handlebars.registerHelper("inc", function(value, options) {
  return parseInt(value) + 1;
});

Handlebars.registerHelper("myselect", function(channel, index, options) {
  if (channel == index) return "selected";
});

Handlebars.registerHelper("capitalize", function(str) {
  if (typeof str !== "string") return "";
  return str.charAt(0).toUpperCase() + str.slice(1);
});

Handlebars.registerHelper("ifeq", function(a, b, options) {
  if (a == b) {
    return options.fn(this);
  }
  return options.inverse(this);
});

Handlebars.registerHelper("times", function(n, block) {
  var accum = "";
  for (var i = 0; i < n; ++i) {
    block.data.index = i;
    block.data.first = i === 0;
    block.data.last = i === n - 1;
    accum += block.fn(this);
  }
  return accum;
});

window.addEventListener("beforeunload", function(e) {
  if (window.callactive) {
    e.preventDefault();
    e.returnValue = "";
  }
});

window.callactive = false;

const show = (id) => document.getElementById(id).classList.remove("d-none");

fetch("/swvariant")
  .then((r) => r.text())
  .then(function(swvariant) {
    window.swvariant = swvariant;

    if (swvariant == "standalone") {
      show("btn-interface");
      show("btn-record");
      show("btn-record-folder");
      show("btn-onair");
      show("btn-mute");
      ws_audio_init();

      if (localStorage.getItem("slonboarding") != "completed") {
        startOnboarding();
      }
    }
    if (swvariant == "plugin") {
      document.getElementById("btn-interface").classList.add("d-none");
      /* calls go out on the plugin track cards (ws_calls.js) */
      document.getElementById("newcall").classList.add("d-none");
    }

    ws_baresip_init();
    ws_calls_init();
    ws_meter_init();
    ws_contacts_init();
    ws_options_init();
  });

fetch("/version")
  .then((r) => r.text())
  .then(function(version) {
    const el = document.getElementById("version");
    el.innerHTML = '<button type="button" class="btn btn-link p-0" id="changelog"></button>';
    const link = el.firstChild;
    link.textContent = version;
    link.setAttribute("aria-label", version + ", show changelog");
    link.addEventListener("click", function(e) {
      e.preventDefault();
      alert({ message: changelog(), size: "large" });
    });
  });
