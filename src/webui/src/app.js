require('bootstrap');
var bootbox = require('bootbox');
var $ = require('jquery');
window.jQuery = $;
window.$ = $;
var marked = require('marked');
window.marked = marked;
//window.ws_host = location.host;
window.ws_host = "127.0.0.1:39963";
window.bootbox = bootbox;

var Handlebars = require('handlebars/runtime');
Handlebars.registerHelper("inc", function(value, options){
	        return parseInt(value) + 1;
});

Handlebars.registerHelper("mysip", function(value, options){
	        if (value.match("sip:(.+@studio-link\.de)"))
		                return value.match("sip:(.+@studio-link\.de)")[1];
	        return value;
});

var swvariant;

Handlebars.registerHelper("inc", function(value, options){
	        return parseInt(value) + 1;
});

Handlebars.registerHelper("mysip", function(value, options){
	        if (value.match("sip:(.+@studio-link\.de)"))
		                return value.match("sip:(.+@studio-link\.de)")[1];
	        return value;
});


$(function () {
	var changelog = require("./templates/changelog.handlebars");

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
