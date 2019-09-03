require('bootstrap');
require('./icons');

var $ = require('jquery');
window.jQuery = $;
window.$ = $;

import Parsley from 'parsleyjs'

var bootbox = require('bootbox');
window.bootbox = bootbox;

var marked = require('marked');
window.marked = marked;

window.ws_host = location.host;
//window.ws_host = "127.0.0.1:39242";
