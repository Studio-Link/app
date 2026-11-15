// Two views, switched from the header nav and kept in the URL hash so the
// back button and reloads work: #tracks (default) and #settings.
const VIEWS = { tracks: "Tracks", settings: "Settings" };

function show(name, moveFocus) {
  for (const v in VIEWS) {
    const active = v === name;
    document.getElementById("view-" + v).hidden = !active;
    const link = document.querySelector('.sl-nav a[href="#' + v + '"]');
    if (active) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  }
  document.title =
    name === "tracks" ? "Studio Link" : VIEWS[name] + " - Studio Link";

  // Tell keyboard and screen reader users where they are now
  if (moveFocus) document.getElementById("view-" + name + "-title").focus();
}

function current() {
  const name = location.hash.slice(1) || "tracks";
  return name in VIEWS ? name : null;
}

// No hash means tracks; other hashes (e.g. the "#app" skip link) leave the
// view as it is
window.addEventListener("hashchange", () => {
  const name = current();
  if (name) show(name, true);
});

show(current() || "tracks", false);
