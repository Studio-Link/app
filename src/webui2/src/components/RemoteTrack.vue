<template>
  <li
    aria-label="Empty Remote Call"
    class="col-span-1"
    @mouseenter="settingsVisible = true; OpenCall(true)"
    @mouseleave="settingsVisible = false; OpenCall(false)"
  >
    <div class="flex justify-between px-1">
      <div class="font-semibold text-sl-on_surface_2 text-sm">Remote {{pkey}}</div>
      <div class="font-semibold text-sm text-green-500 uppercase"></div>
    </div>
    <div class="bg-sl-02dp rounded-lg shadow h-44">
      <div class="flex justify-end">
        <div class="flex-shrink-0 pr-2 text-right mt-1">
          <button
            aria-label="Track Settings"
            class="w-8 h-8 inline-flex items-center justify-center text-gray-400 rounded-full bg-transparent hover:text-gray-500 focus:outline-none focus:text-sl-surface focus:bg-sl-on_surface_2 transition ease-in-out duration-150"
            @focus="settingsVisible = true; OpenCall(true)"
            @focusout="settingsVisible = false"
          >
            <svg aria-hidden="true" class="w-5 h-5" viewBox="0 0 20 20" fill="currentColor">
              <path
                v-if="settingsVisible"
                d="M10 6a2 2 0 110-4 2 2 0 010 4zM10 12a2 2 0 110-4 2 2 0 010 4zM10 18a2 2 0 110-4 2 2 0 010 4z"
              />
            </svg>
          </button>
        </div>
      </div>
      <div v-if="showOpenCall" class="flex-1 px-4 grid grid-cols-1">
        <label
          :for="pkey"
          class="block text-sm font-medium leading-5 text-sl-on_surface_2"
        >Enter Partner ID</label>
        <div class="mt-1 relative rounded-md shadow-sm">
          <input
            :id="pkey"
            class="form-input block w-full sm:text-sm sm:leading-5 text-sl-on_surface_1 bg-sl-surface mb-2 border-none focus:shadow-outline-orange"
            placeholder="xyz@studio.link"
          />
        </div>
        <div class="mt-1">
          <Button
            @focusout="OpenCall(false)"
          ></Button>
        </div>
      </div>
      <div v-if="!showOpenCall" class="text-center mt-10 text-sl-disabled">No call</div>
    </div>
  </li>
</template>

<script lang="ts">
import { ref, defineComponent, onMounted } from "vue";

export default defineComponent({
  props: { pkey: Number },
  setup(props) {
    const settingsVisible = ref(false);
    const showOpenCall = ref(true);

    function OpenCall(status) {
      if (props.pkey == 1) {
        showOpenCall.value = true;
        return;
      }

      showOpenCall.value = status;
    }

    return { settingsVisible, showOpenCall, OpenCall };
  },
});
</script>