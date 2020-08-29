<template>
  <ul class="grid grid-cols-1 gap-4 sm:grid-cols-2 md:grid-cols-3 lg:grid-cols-4">
    <li class="flex items-center justify-center col-span-1 bg-sl-02dp rounded-lg shadow h-40">
        <button class="bg-sl-primary rounded-lg py-2 px-3 text-black font-bold uppercase tracking-wider">Select Audio</button>
    </li>
    <li v-for="index in 0" class="flex items-center justify-center col-span-1 bg-sl-02dp rounded-lg shadow h-40">
    </li>
  </ul>
</template>