/* eslint-disable no-undef */
module.exports = {
  purge: {
    enabled: process.env.NODE_ENV === "production",
    content: ["./index.html", "./src/**/*.vue", "./src/**/*.js"],
  },
  theme: {
    fontFamily: {
      sans: ['"Roboto Mono"'],
    },
    extend: {
      colors: {
        sl: {
          surface: "#121212",
          disabled: "#6c6c6c",
          on_surface_1: "#e0e0e0",
          on_surface_2: "#a0a0a0",
          "06dp": "#2c2c2c",
          "02dp": "#232323",
          primary: "#de7b00",
        },
      },
    },
  },
  plugins: [
    require("@tailwindcss/ui")({
      layout: "sidebar",
    }),
  ],
};
