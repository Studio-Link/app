<template>
  <img
    alt="Vue logo"
    src="./assets/logo.png"
  />
  <HelloWorld msg="Hello Vue 3.0 + Vite" />
  <div class="flex flex-wrap justify-between items-end w-full bottom-0 fixed mb-4 px-4">
    <div class="invisible w-full sm:w-1/3 sm:visible">
      Main | Settings | Chat
    </div>
    <div class="w-full sm:w-1/3 flex justify-center gap-2">
      <div class="rounded-full h-16 w-16 flex items-center justify-center bg-red-800">
        Circle
      </div>
      <div class="rounded-full h-16 w-16 flex items-center justify-center bg-red-800">
        Circle
      </div>
      <div class="rounded-full h-16 w-16 flex items-center justify-center bg-red-800">
        Circle
      </div>
    </div>
    <Version></Version>
  </div>
</template>

<script>
import HelloWorld from "./components/HelloWorld.vue";
import Version from "./components/Version.vue";

export default {
  name: "App",
  components: {
    HelloWorld,
    Version
  },
};
</script>