<template>
  <header class="flex items-end justify-between mb-8 mt-4">
    <img src="/logo_standalone.svg" class="h-10" />
    <HeadStatus />
  </header>
  <Tracks />

  <footer class="flex flex-wrap justify-between items-end container bottom-0 fixed mb-6">
    <Navbar></Navbar>
    <BottomActions></BottomActions>
    <Version></Version>
  </footer>
</template>

<script>
import Version from "./components/Version.vue";
import BottomActions from "./components/BottomActions.vue";
import Tracks from "./components/Tracks.vue";
import HeadStatus from "./components/HeadStatus.vue";
import Navbar from "./components/Navbar.vue"

export default {
  name: "App",
  components: {
    BottomActions,
    Version,
    Tracks,
    HeadStatus,
    Navbar
  },
};
</script>