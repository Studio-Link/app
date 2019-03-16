require('bootstrap');
var bootbox = require('bootbox');
var $ = require('jquery');
window.jQuery = $;
window.$ = $;
var marked = require('marked');
window.marked = marked;
//window.ws_host = location.host;
window.ws_host = "127.0.0.1:39963";

var swvariant;

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
			ws_rtaudio_init();
		}
	});



	$( "#changelog" ).on( "click", function() {
		bootbox.alert({
			message: changelog,
			size: 'large'
		}
		);
	});

});
