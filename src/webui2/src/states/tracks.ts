import { reactive } from "vue";
export const tracks = {
  socket: Object,
  state: reactive([]),

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
  },

  update() {

    let i: number;
    for (i = 0; i < this.state.length; i++) {
      this.state.pop();
    }

    this.state.push({ name: "Local", active: false });
    this.state.push({ name: "Remote 1", active: false });
    this.state.push({ name: "Remote 2", active: false });
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

  setActive(id: number) {
    for (let i = 0; i < this.state.length; i++) {
      if (i == id) {
        this.state[i].active = true;
      } else {
        this.state[i].active = false;
      }
    }
  },
};
