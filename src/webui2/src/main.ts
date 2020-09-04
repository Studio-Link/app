import { createApp } from "vue";
import App from "./App.vue";
import "./index.css";
import "typeface-roboto-mono";
import Button from "./components/Button.vue";
import { tracks } from "./states/tracks";

window.tracks = tracks;

const app = createApp(App);
app.component("Button", Button);
app.mount("#app");