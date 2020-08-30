<template>
  <ul class="grid grid-cols-1 gap-4 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4">
    <li class="flex items-center justify-center col-span-1 bg-sl-02dp rounded-lg shadow h-40">
      <button
        class="inline-flex items-center bg-sl-primary rounded-lg px-4 py-3 text-black font-bold leading-none uppercase tracking-wide"
      >Select Audio</button>
    </li>
    <li
      v-for="index in 2"
      class="flex items-center justify-center col-span-1 bg-sl-02dp rounded-lg shadow h-40"
    ></li>
  </ul>
</template>