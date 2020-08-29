<template>
  <div class="w-full sm:w-1/3 flex justify-center gap-2">
    <button
      @click="muted = !muted"
      :class="{ 'bg-red-900 hover:bg-red-800': muted, 'bg-sl-06dp': !muted}"
      class="rounded-full h-20 w-20 flex items-center justify-center hover:bg-gray-700 focus:outline-none focus:shadow-outline"
    >Mute</button>
  </div>
</template>


<script>
export default {
  data() {
    return {
      muted: false,
    };
  },
};
</script>