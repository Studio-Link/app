import { createApp } from "vue";
import App from "./App.vue";
import "./index.css";
import "typeface-roboto-mono";
import Button from "./components/Button.vue";
import { tracks } from "./states/tracks";

//window.ws_host = location.host;
window.ws_host = "127.0.0.1:3333";

tracks.init_websocket();

let handleClickOutside;

const app = createApp(App);
app.directive("click-outside", {
  beforeMount(el, binding, vnode) {
    handleClickOutside = (e) => {
      e.stopPropagation();
      const { handler, exclude } = binding.value;
      let clickedOnExcludedEl = false;

      // Gives you the ability to exclude certain elements if you want, pass as array of strings to exclude
      if (exclude) {
        exclude.forEach((refName) => {
          if (!clickedOnExcludedEl) {
            const excludedEl = binding.instance.$refs[refName];
            clickedOnExcludedEl = excludedEl.contains(e.target);
          }
        });
      }

      if (!el.contains(e.target) && !clickedOnExcludedEl) {
        handler(e);
      }
    };
    document.addEventListener("click", handleClickOutside);
    document.addEventListener("touchstart", handleClickOutside);
  },
  unmounted() {
    document.removeEventListener("click", handleClickOutside);
    document.removeEventListener("touchstart", handleClickOutside);
  },
});
app.component("Button", Button);
app.mount("#app");
