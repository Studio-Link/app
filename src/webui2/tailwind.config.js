/* eslint-disable no-undef */
module.exports = {
  purge: {
    enabled: process.env.NODE_ENV === "production",
    content: ["./index.html", "./src/**/*.vue", "./src/**/*.js"],
  },
  theme: {
    container: {
      screens: {
        sm: "100%",
        md: "100%",
        lg: "1024px",
        xl: "1280px",
      }
    },
    fontFamily: {
      sans: ['"Roboto Mono"'],
    },
    extend: {
      colors: {
        sl: {
          primary: "#de7b00",
          surface: "#121212",
          disabled: "#6c6c6c",
          on_surface_1: "#e0e0e0",
          on_surface_2: "#a0a0a0",
          "01dp": "rgba(255, 255, 255, 0.05)",
          "02dp": "rgba(255, 255, 255, 0.07)",
          "03dp": "rgba(255, 255, 255, 0.08)",
          "04dp": "rgba(255, 255, 255, 0.09)",
          "06dp": "rgba(255, 255, 255, 0.11)",
          "08dp": "rgba(255, 255, 255, 0.12)",
          "12dp": "rgba(255, 255, 255, 0.14)",
          "16dp": "rgba(255, 255, 255, 0.15)",
          "24dp": "rgba(255, 255, 255, 0.16)",
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
