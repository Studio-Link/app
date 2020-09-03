<template>
  <header class="flex flex-wrap items-end justify-between sm:mb-4 sm:mt-4 p-2">
    <a aria-hidden="true" href="/">
      <img src="/logo_standalone.svg" class="h-10" />
    </a>
    <HeadStatus />
  </header>
  <Tracks class="p-2" />

  <footer class="bottom-0 container fixed mb-4">
    <div class="flex flex-wrap justify-between items-end px-2">
      <Navbar></Navbar>
      <BottomActions></BottomActions>
      <Version></Version>
    </div>
  </footer>
</template>

<script lang='ts'>
import Version from "./components/Version.vue";
import BottomActions from "./components/BottomActions.vue";
import Tracks from "./components/Tracks.vue";
import HeadStatus from "./components/HeadStatus.vue";
import Navbar from "./components/Navbar.vue";
import { defineComponent } from "vue";

export default defineComponent({
  name: "App",
  components: {
    BottomActions,
    Version,
    Tracks,
    HeadStatus,
    Navbar,
  },
});
</script>