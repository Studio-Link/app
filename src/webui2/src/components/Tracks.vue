<template>
  <ul class="grid grid-cols-1 gap-4 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 mb-36 sm:mb-24">
    <li class="col-span-1">
      <h2 class="pl-1 font-bold text-sl-on_surface_2 text-sm">
        Local Track
      </h2>
      <div class="flex items-center justify-center bg-sl-02dp rounded-lg shadow h-44">
        <Button>
          <svg
            xmlns="http://www.w3.org/2000/svg"
            viewBox="0 0 20 20"
            fill="currentColor"
            class="h-6 mr-1"
          >
            <path
              fill-rule="evenodd"
              d="M7 4a3 3 0 016 0v4a3 3 0 11-6 0V4zm4 10.93A7.001 7.001 0 0017 8a1 1 0 10-2 0A5 5 0 015 8a1 1 0 00-2 0 7.001 7.001 0 006 6.93V17H6a1 1 0 100 2h8a1 1 0 100-2h-3v-2.07z"
              clip-rule="evenodd"
            />
          </svg>
          Select Microphone
        </Button>
      </div>
    </li>
    <RemoteTrack
      v-for="index in remoteTracks"
      :key="index"
      :pkey="index"
    />
    <li
      v-if="!newTrackDisabled"
      class="col-span-1"
      @mouseenter="newTrackVisible = true"
      @mouseleave="newTrackVisible = false"
    >
      <div class="flex items-center justify-center h-44 mt-4">
        <button
          accesskey="t"
          aria-label="Add Remote Track"
          :class="{'text-sl-disabled': newTrackVisible, 'text-sl-01dp': !newTrackVisible}"
          class="inline-flex items-center rounded-lg px-20 py-12 font-bold text-2xl leading-none uppercase tracking-wide focus:outline-none focus:text-sl-disabled"
          @focus="newTrackVisible = true"
          @focusout="newTrackVisible = false"
          @click="newRemoteTrack(); newTrackVisible = false"
        >
          <svg
            aria-hidden="true"
            xmlns="http://www.w3.org/2000/svg"
            fill="none"
            viewBox="0 0 24 24"
            stroke="currentColor"
            class="h-20 w-20"
          >
            <path
              stroke-linecap="round"
              stroke-linejoin="round"
              stroke-width="2"
              d="M12 6v6m0 0v6m0-6h6m-6 0H6"
            />
          </svg>
        </button>
      </div>
    </li>
  </ul>
</template>

<script>
import RemoteTrack from "./RemoteTrack.vue";
import { ref, defineComponent } from "vue";
import { tracks } from "../states/tracks";

// tracks.update();

export default defineComponent({
  components: {
    RemoteTrack,
  },
  setup() {
    const newTrackVisible = ref(false);
    const newTrackDisabled = ref(false);
    const remoteTracks = ref(0);

    function newRemoteTrack() {
      let next = remoteTracks.value + 1;
      if (tracks.isValid(next)) {
        tracks.setActive(next);
        remoteTracks.value = next;
      }
      if (!tracks.isValid(next+1)) {
        newTrackDisabled.value = true;
      }
    }

    return { newTrackDisabled, newTrackVisible, remoteTracks, newRemoteTrack };
  },
});
</script>