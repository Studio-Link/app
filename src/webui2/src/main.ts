import { createApp } from "vue";
import App from "./App.vue";
import "./index.css";
import "typeface-roboto-mono";
import Button from "./components/Button.vue";
import {tracks} from "./states/tracks";

//window.ws_host = location.host;
window.ws_host = "127.0.0.1:3333";

tracks.init_websocket();

const app = createApp(App);
app.component("Button", Button);
app.mount("#app");