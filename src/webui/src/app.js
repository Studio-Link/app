var Handlebars = require("handlebars/runtime");

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

$.fn.serializeObject = function() {
  var obj = {};
  var arr = this.serializeArray();
  arr.forEach(function(item, index) {
    if (obj[item.name] === undefined) {
      // New
      obj[item.name] = item.value || "";
    } else {
      // Existing
      if (!obj[item.name].push) {
        obj[item.name] = [obj[item.name]];
      }
      obj[item.name].push(item.value || "");
    }
  });
  return obj;
};

window.addEventListener("beforeunload", function(e) {
  if (window.callactive) {
    e.preventDefault();
    e.returnValue = "";
  }
});

$(function() {
  var changelog = require("./templates/changelog.handlebars");
  window.callactive = false;

  $.get("/swvariant", function(data) {
    var swvariant = data;
    window.swvariant = swvariant;

    if (swvariant == "standalone") {
      $("#btn-interface").removeClass("d-none");
      $("#btn-record").removeClass("d-none");
      $("#btn-onair").removeClass("d-none");
      $("#btn-mute").removeClass("d-none");
      ws_rtaudio_init();

      if (localStorage.getItem("slonboarding") != "completed") {
        window.Vue.prototype.$tours["slTour"].start();
      }
    }
    if (swvariant == "plugin") {
      $("#btn-interface").addClass("d-none");
    }

    ws_baresip_init();
    ws_calls_init();
    ws_meter_init();
    ws_contacts_init();
    ws_options_init();
  });

  $.get("/version", function(data) {
    version = data;
    version_int = parseInt(version.replace(/[^0-9]+/g, ""));

    $("#version").html('<a href="#" id="changelog">' + version + "</a>");
    $("#changelog").on("click", function() {
      bootbox.alert({
        message: changelog(),
        size: "large",
      });
    });
  });
});
