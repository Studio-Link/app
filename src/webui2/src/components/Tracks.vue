<template>
  <ul class="grid grid-cols-1 gap-4 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4 mb-36 sm:mb-24">
    <!-- Local Track(s) -->
    <li class="flex items-center justify-center col-span-1 bg-sl-02dp rounded-lg shadow h-44">
      <button
        class="inline-flex items-center bg-sl-primary rounded-lg px-4 py-3 text-black font-bold leading-none uppercase tracking-wide"
      >Select Audio</button>
    </li>
    <RemoteTrack v-for="index in remoteTracks" :key="index" :pkey="index" />
    <li
      class="flex items-center justify-center col-span-1 h-40"
      @mouseenter="newTrackVisible = true"
      @mouseleave="newTrackVisible = false"
    >
      <button
        @focus="newTrackVisible = true"
        @focusout="newTrackVisible = false"
        @click="remoteTracks++; newTrackVisible = false"
        accesskey="t"
        aria-label="Add Remote Track"
        :class="{'text-sl-disabled': newTrackVisible, 'text-sl-01dp': !newTrackVisible}"
        class="inline-flex items-center rounded-lg px-20 py-12 font-bold text-2xl leading-none uppercase tracking-wide focus:outline-none focus:text-sl-disabled"
      >
        <svg
          aria-hidden="true"
          focusable="false"
          data-prefix="fas"
          data-icon="plus"
          class="svg-inline--fa fa-plus fa-w-14 h-12 w-12"
          role="img"
          xmlns="http://www.w3.org/2000/svg"
          viewBox="0 0 448 512"
        >
          <path
            fill="currentColor"
            d="M416 208H272V64c0-17.67-14.33-32-32-32h-32c-17.67 0-32 14.33-32 32v144H32c-17.67 0-32 14.33-32 32v32c0 17.67 14.33 32 32 32h144v144c0 17.67 14.33 32 32 32h32c17.67 0 32-14.33 32-32V304h144c17.67 0 32-14.33 32-32v-32c0-17.67-14.33-32-32-32z"
          />
        </svg>
      </button>
    </li>
  </ul>
</template>

<script>
import RemoteTrack from "./RemoteTrack.vue";
import { ref } from "vue";

export default {
  components: {
    RemoteTrack,
  },
  setup() {
    const newTrackVisible = ref(false);
    const remoteTracks = ref(0);

    return { newTrackVisible, remoteTracks };
  },
};
</script>