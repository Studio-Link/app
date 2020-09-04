import { reactive } from "vue";
export const tracks = {
  state: reactive([
    { name: "Remote Track 1", active: false },
    { name: "Remote Track 2", active: false },
    { name: "Remote Track 3", active: false },
    { name: "Remote Track 4", active: false },
    { name: "Remote Track Test", active: false },
    { name: "Remote Track 6", active: false },
    { name: "Remote Track 7", active: false },
  ]),

  getTrackName(id) {
    if (this.state[id]) {
      return this.state[id].name;
    }

    return "error";
  },

  isValid(id) {
    if (this.state[id]) {
      return true;
    }
    return false;
  },

  isActive(id) {
    if (this.state[id]) {
      return this.state[id].active;
    }
    return false;
  },

  setActive(id) {
    for (let i = 0; i < this.state.length; i++) {
      if (i == id) {
        this.state[i].active = true;
      } else {
        this.state[i].active = false;
      }
    }
  },
};
