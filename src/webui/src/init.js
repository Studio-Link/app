require('bootstrap');
require('./icons');

window.Vue = require("vue");
import VueTour from 'vue-tour'

require('vue-tour/dist/vue-tour.css')

var $ = require('jquery');
window.jQuery = $;
window.$ = $;

import Parsley from 'parsleyjs'

var bootbox = require('bootbox');
window.bootbox = bootbox;

window.ws_host = location.host;
//window.ws_host = "127.0.0.1:34081";

Vue.use(VueTour);

Vue.component("onboarding", require("./components/Onboarding.vue").default);

const app = new Vue({
    el: "#app"
});
