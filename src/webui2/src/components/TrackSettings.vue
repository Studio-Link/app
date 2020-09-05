<template>
  <div
    v-if="active"
    class="relative inline-block text-left z-10"
  >
    <div class="origin-top-right absolute right-0 mt-2 w-56 rounded-md shadow-lg">
      <div class="rounded-md bg-sl-04dp shadow-xs">
        <div
          class="py-1"
          role="menu"
          aria-orientation="vertical"
          aria-labelledby="options-menu"
        >
          <a
            href="#"
            class="group flex items-center px-4 py-2 text-sm leading-5  text-sl-on_surface_2 hover:bg-sl-surface focus:outline-none focus:bg-sl-surface"
            role="menuitem"
          >
            <svg
              viewBox="0 0 20 20"
              fill="currentColor"
              class="mr-3 view-grid w-5 h-5"
            ><path d="M5 3a2 2 0 00-2 2v2a2 2 0 002 2h2a2 2 0 002-2V5a2 2 0 00-2-2H5zM5 11a2 2 0 00-2 2v2a2 2 0 002 2h2a2 2 0 002-2v-2a2 2 0 00-2-2H5zM11 5a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2V5zM11 13a2 2 0 012-2h2a2 2 0 012 2v2a2 2 0 01-2 2h-2a2 2 0 01-2-2v-2z"></path></svg>
            Keypad
          </a>
          <a
            href="#"
            class="group flex items-center px-4 py-2 text-sm leading-5  text-sl-on_surface_2 hover:bg-sl-surface hover:text-red-500 focus:outline-none focus:bg-sl-surface focus:text-red-500"
            role="menuitem"
          >
            <svg
              viewBox="0 0 20 20"
              fill="currentColor"
              class="mr-3 switch-horizontal w-5 h-5"
            ><path d="M8 5a1 1 0 100 2h5.586l-1.293 1.293a1 1 0 001.414 1.414l3-3a1 1 0 000-1.414l-3-3a1 1 0 10-1.414 1.414L13.586 5H8zM12 15a1 1 0 100-2H6.414l1.293-1.293a1 1 0 10-1.414-1.414l-3 3a1 1 0 000 1.414l3 3a1 1 0 001.414-1.414L6.414 15H12z"></path></svg>
            Disable N-1
          </a>
        </div>
      </div>
    </div>
  </div>
</template>

<script lang="ts">
import { defineComponent } from "vue";

export default defineComponent({
  props: { active: Boolean },
});
</script>