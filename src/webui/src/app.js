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
  if (channel == index)
    return 'selected';
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

$(function() {
  var changelog = require("./templates/changelog.handlebars");
  var listsip = require("./templates/listsip.handlebars");
  var listcontacts = require("./templates/listcontacts.handlebars");

  $("#accounts").html(listsip());
  $("#contacts").html(listcontacts());

  $.get("/swvariant", function(data) {
    var swvariant = data;

    if (swvariant == "standalone") {
      $("#btn-record").removeClass("d-none");
      //$("#btn-onair").removeClass("d-none");
      ws_rtaudio_init();
    }
    if (swvariant == "plugin") {
      $("#btn-interface").addClass("d-none");
    }
    window.swvariant = swvariant;
  });

  $.get("/version", function(data) {
    version = data;
    version_int = parseInt(version.replace(/[^0-9]+/g, ""));

    $("#version").html('<a href="#" id="changelog">' + version + "</a>");
    $("#changelog").on("click", function() {
      bootbox.alert({
        message: changelog(),
        size: "large"
      });
    });
  });
});
