<template>
  <div class="w-full sm:w-1/3 flex justify-center">
    <button
      :class="{ 'bg-red-900 hover:bg-red-800': muted, 'bg-sl-08dp': !muted}"
      class="rounded-full h-20 w-20 flex items-center justify-center hover:bg-gray-700 shadow uppercase font-semibold"
      @click="muted = !muted"
    >
      Mute
    </button>
    <button
      :class="{ 'bg-red-900 hover:bg-red-800': record, 'bg-sl-08dp': !record}"
      class="ml-20 rounded-full h-20 w-20 flex items-center justify-center hover:bg-gray-700 shadow uppercase font-semibold"
      @click="record = !record"
    >
      Record
    </button>
  </div>
</template>

<script lang="ts">
import { defineComponent } from "vue";

export default defineComponent({
  data() {
    return {
      muted: false,
      record: false
    };
  },
});
</script>