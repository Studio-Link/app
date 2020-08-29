<template>
  <footer class="flex flex-wrap justify-between items-end w-full bottom-0 fixed mb-4 px-4">
    <nav class="invisible w-full sm:w-1/3 sm:visible">
      Main | Settings | Chat
    </nav>
    <BottomActions></BottomActions>
    <Version></Version>
  </footer>
</template>

<script>
import Version from "./components/Version.vue";
import BottomActions from "./components/BottomActions.vue";

export default {
  name: "App",
  components: {
    BottomActions,
    Version
  },
};
</script>