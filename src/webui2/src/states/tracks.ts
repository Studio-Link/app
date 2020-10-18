import { reactive } from "vue";
export const tracks = {
  socket: Object,
  state: reactive([]),
  active_debounce: Boolean,

  getTrackName(id) {
    if (this.state[id]) {
      return this.state[id].name;
    }

    return "error";
  },

  init_websocket() {
    this.socket = new WebSocket("ws://" + window.ws_host + "/ws_tracks");
    this.socket.onerror = function () {
      console.log("Websocket error");
    };
    tracks.update();
    this.active_debounce = false;
  },

  update() {
    let i: number;
    for (i = 0; i < this.state.length; i++) {
      this.state.pop();
    }

    this.state.push({ name: "Local", active: false });
    this.state.push({ name: "Remote 1", active: false });
    this.state.push({ name: "Remote 2", active: false });
    this.state.push({ name: "Remote 3", active: false });
    this.state.push({ name: "Remote 4", active: false });
    this.state.push({ name: "Remote 5", active: false });
    this.state.push({ name: "Remote 6", active: false });
    this.state.push({ name: "Remote 7", active: false });
    this.state.push({ name: "Remote 8", active: false });
  },

  isValid(id: number) {
    if (this.state[id]) {
      return true;
    }
    return false;
  },

  isActive(id: number) {
    if (this.state[id]) {
      return this.state[id].active;
    }
    return false;
  },

  debounce_recover() {
    this.active_debounce = false;
  },

  setActive(id: number) {
    //Workaround for mouseenter event after focus change
    if (this.active_debounce) return;
    this.active_debounce = true;
    setTimeout(() => {this.debounce_recover()}, 10);

    for (let i = 0; i < this.state.length; i++) {
      if (i == id) {
        this.state[i].active = true;
      } else {
        this.state[i].active = false;
      }
    }
  },
};
