require('bootstrap');
var bootbox = require('bootbox');
var $ = require('jquery');
window.jQuery = $;
window.$ = $;
var marked = require('marked');
window.marked = marked;

var changelog = require("./templates/changelog.handlebars");


$( "#changelog" ).on( "click", function() {
	bootbox.alert({
		message: changelog,
		size: 'large'
	}
	);
});
