<template>
  <div>
    <v-tour name="slTour" :steps="steps" :callbacks="slCallbacks"></v-tour>
  </div>
</template>

<script>
export default {
  name: "sl-tour",
  data() {
    return {
      steps: [
        {
          target: "#btn-interface",
          content: "Select your Audio Interface'"
        },
        {
          target: "#sipnumbercall",
          content: "Enter <b>echo</b> and press <b>Call</b>",
          params: {
              placement: 'left'
          }
        },
        {
          target: "#btn-record",
          content: "Press <b>Record</b> if ready",
        }
      ],
      slCallbacks: {
        onFinish: function() {
          localStorage.setItem("slonboarding", "completed");
        },
        onSkip: function() {
          localStorage.setItem("slonboarding", "completed");
        }
      }
    };
  }
};
</script>