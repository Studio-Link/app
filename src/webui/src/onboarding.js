import { driver } from "driver.js";
import "driver.js/dist/driver.css";

export function startOnboarding() {
  const tour = driver({
    showProgress: true,
    steps: [
      {
        element: "#btn-interface",
        popover: { description: "Select your Audio Interface" },
      },
      {
        element: "#sipnumbercall",
        popover: {
          description: "Enter <b>echo</b> and press <b>Call</b>",
          side: "left",
        },
      },
      {
        element: "#btn-record",
        popover: { description: "Press <b>Record</b> if ready" },
      },
    ],
    // Called on finish as well as on skip/close.
    onDestroyed: () => {
      localStorage.setItem("slonboarding", "completed");
    },
  });
  tour.drive();
}
