require('bootstrap');

var $ = require('jquery');
window.jQuery = $;
window.$ = $;

import Parsley from 'parsleyjs'

var bootbox = require('bootbox');
window.bootbox = bootbox;

var marked = require('marked');
window.marked = marked;


//window.ws_host = location.host;
window.ws_host = "127.0.0.1:39963";
var swvariant;

var Handlebars = require('handlebars/runtime');

Handlebars.registerHelper("inc", function(value, options){
	        return parseInt(value) + 1;
});

Handlebars.registerHelper("mysip", function(value, options){
	        if (value.match("sip:(.+@studio-link\.de)"))
		                return value.match("sip:(.+@studio-link\.de)")[1];
	        return value;
});


Handlebars.registerHelper("inc", function(value, options){
	        return parseInt(value) + 1;
});

Handlebars.registerHelper("mysip", function(value, options){
	        if (value.match("sip:(.+@studio-link\.de)"))
		                return value.match("sip:(.+@studio-link\.de)")[1];
	        return value;
});


$.fn.serializeObject = function() {
    var obj = {};
    var arr = this.serializeArray();
    arr.forEach(function(item, index) {
        if (obj[item.name] === undefined) { // New
            obj[item.name] = item.value || '';
        } else {                            // Existing
            if (!obj[item.name].push) {
                obj[item.name] = [obj[item.name]];
            }
            obj[item.name].push(item.value || '');
        }
    });
    return obj;
};

$(function () {
	var changelog = require("./templates/changelog.handlebars");
	var listsip = require("./templates/listsip.handlebars");
	var listcontacts = require("./templates/listcontacts.handlebars");

	$( "#accounts" ).html(listsip());
	$( "#contacts" ).html(listcontacts());


	$.get( "/swvariant", function( data ) {
		swvariant = data;

		if (swvariant == "plugin") {
			$("#btn-bypass").removeClass("d-none");
			$("#variant").html("Plug-in");
		} else {
			$("#variant").html("Standalone");
			$("#btn-interface").removeClass("d-none");
			$("#btn-mono").removeClass("d-none");
			$("#btn-stereo").removeClass("d-none");
			$("#btn-record").removeClass("d-none");
			$("#btn-onair").removeClass("d-none");
			//ws_rtaudio_init();
		}
		window.swvariant = swvariant;
	});



	$( "#changelog" ).on( "click", function() {
		bootbox.alert({
			message: changelog,
			size: 'large'
		}
		);
	});

});
