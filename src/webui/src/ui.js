// Small vanilla replacements for jQuery, bootbox, parsley and notify.js.
import { Modal, Toast } from "bootstrap";

export const qs = (sel, root = document) => root.querySelector(sel);
export const qsa = (sel, root = document) => root.querySelectorAll(sel);

// Bind `handler` to `event` on every element matching `sel` (like $(sel).on()).
export function on(sel, event, handler) {
  qsa(sel).forEach((el) => el.addEventListener(event, handler));
}

export function escapeHtml(text) {
  const div = document.createElement("div");
  div.textContent = text;
  return div.innerHTML;
}

/* Dialogs ---------------------------------------------------------------- */

/*
 * bootbox.dialog() compatible subset:
 *   dialog({ title, message, size: "large", buttons: { name: { label,
 *            className, callback } }, onEscape })
 * A button callback returning false keeps the dialog open.
 */
export function dialog({ title, message, size, buttons = {}, onEscape }) {
  const el = document.createElement("div");
  const uid = "dialog-" + Math.random().toString(36).slice(2, 9);
  el.className = "modal fade";
  el.tabIndex = -1;
  el.setAttribute("role", "dialog");
  if (title) el.setAttribute("aria-labelledby", uid + "-title");
  else el.setAttribute("aria-labelledby", uid + "-body");
  el.innerHTML = `
    <div class="modal-dialog${size === "large" ? " modal-lg" : ""}">
      <div class="modal-content">
        ${title ? `<div class="modal-header">
          <h2 class="modal-title h5" id="${uid}-title"></h2>
          <button type="button" class="btn-close" aria-label="Close"></button>
        </div>` : ""}
        <div class="modal-body"><div class="dialog-body" id="${uid}-body"></div></div>
        <div class="modal-footer"></div>
      </div>
    </div>`;

  if (title) qs(".modal-title", el).textContent = title;
  qs(".dialog-body", el).innerHTML = message;

  const modal = new Modal(el, { backdrop: "static", keyboard: false });
  let escaping = false;
  const dismiss = () => {
    if (onEscape) onEscape();
    modal.hide();
  };

  const footer = qs(".modal-footer", el);
  const names = Object.keys(buttons);
  names.forEach((name, i) => {
    const def = buttons[name];
    const btn = document.createElement("button");
    btn.type = "button";
    btn.className =
      "btn " +
      (def.className || (i === names.length - 1 ? "btn-primary" : "btn-secondary"));
    btn.textContent = def.label || name;
    btn.addEventListener("click", () => {
      if (def.callback && def.callback() === false) return;
      modal.hide();
    });
    footer.appendChild(btn);
  });
  if (!names.length) footer.remove();

  const close = qs(".btn-close", el);
  if (close) close.addEventListener("click", dismiss);
  // Escape/backdrop clicks are handled here; skip Bootstrap's "static
  // backdrop" bounce animation, which would outlive the disposed modal.
  el.addEventListener("hidePrevented.bs.modal", (e) => e.preventDefault());
  el.addEventListener("keydown", (e) => {
    if (e.key === "Escape" && !escaping) {
      escaping = true;
      dismiss();
    }
  });

  el.addEventListener("shown.bs.modal", () => {
    const focus = qs("[autofocus]", el);
    if (focus) focus.focus();
  });
  el.addEventListener("hidden.bs.modal", () => {
    modal.dispose();
    el.remove();
  });

  document.body.appendChild(el);
  modal.show();
  return el;
}

export function alert({ message, size }) {
  return dialog({ message, size, buttons: { ok: { label: "OK" } } });
}

export function confirm(message, callback) {
  return dialog({
    message: escapeHtml(message),
    onEscape: () => callback(false),
    buttons: {
      cancel: { label: "Cancel", callback: () => callback(false) },
      confirm: { label: "OK", callback: () => callback(true) },
    },
  });
}

// Hide every open dialog without running any callbacks (bootbox.hideAll()).
export function hideAll() {
  qsa(".modal").forEach((el) => {
    const modal = Modal.getInstance(el);
    if (modal) modal.hide();
  });
}

/* Notifications ---------------------------------------------------------- */

const notifyClasses = {
  error: "text-bg-danger",
  warn: "text-bg-warning",
  success: "text-bg-success",
  info: "text-bg-info",
};

// $.notify(text, className) replacement using Bootstrap toasts.
export function notify(text, type = "error") {
  let container = qs("#toast-container");
  if (!container) {
    container = document.createElement("div");
    container.id = "toast-container";
    container.className = "toast-container position-fixed top-0 end-0 p-2";
    document.body.appendChild(container);
  }

  const el = document.createElement("div");
  el.className = `toast align-items-center border-0 ${notifyClasses[type] || notifyClasses.info}`;
  el.setAttribute("role", type === "error" ? "alert" : "status");
  el.setAttribute("aria-live", type === "error" ? "assertive" : "polite");
  el.setAttribute("aria-atomic", "true");
  el.innerHTML = `<div class="d-flex">
      <div class="toast-body fw-bold"></div>
      <button type="button" class="btn-close me-2 m-auto" data-bs-dismiss="toast" aria-label="Close"></button>
    </div>`;
  qs(".toast-body", el).textContent = text;
  el.addEventListener("hidden.bs.toast", () => el.remove());

  container.prepend(el);
  // Errors stay until closed, so there is time to read them (WCAG 2.2.1).
  new Toast(el, { autohide: type !== "error", delay: 10000 }).show();
}

/* Forms ------------------------------------------------------------------ */

// $(form).serializeObject() replacement: repeated names become arrays.
export function serializeForm(form) {
  const obj = {};
  for (const [name, value] of new FormData(form)) {
    if (!(name in obj)) obj[name] = value;
    else if (Array.isArray(obj[name])) obj[name].push(value);
    else obj[name] = [obj[name], value];
  }
  return obj;
}

function validateField(field) {
  if (!field.willValidate || field.type === "hidden") return;
  const valid = field.checkValidity();
  field.classList.toggle("is-invalid", !valid);
  field.classList.toggle("is-valid", valid);

  let feedback = field.nextElementSibling;
  if (!feedback || !feedback.classList.contains("invalid-feedback")) {
    feedback = document.createElement("div");
    feedback.className = "invalid-feedback";
    field.after(feedback);
  }
  feedback.textContent = valid ? "" : field.validationMessage;
}

// Native constraint validation with Bootstrap styling (parsley replacement).
// After the first call, fields re-validate while typing.
export function validateForm(form) {
  if (!form) return false;
  if (!form.dataset.validating) {
    form.dataset.validating = "1";
    form.addEventListener("input", (e) => validateField(e.target));
  }
  Array.from(form.elements).forEach(validateField);
  return form.checkValidity();
}

/* Misc ------------------------------------------------------------------- */

const FOCUSABLE = "a[href], button, input, select, textarea, [tabindex]";

// Replace el's content but keep keyboard focus on the "same" control
// (matched by id, else by position), so screen reader and keyboard users
// are not thrown back to the top of the page on every update.
export function render(el, html) {
  if (!el) return;
  const active = document.activeElement;
  let id = null;
  let index = -1;
  if (active && el.contains(active)) {
    id = active.id;
    index = Array.from(el.querySelectorAll(FOCUSABLE)).indexOf(active);
  }
  el.innerHTML = html;
  if (index < 0) return;
  const target =
    (id && document.getElementById(id)) ||
    el.querySelectorAll(FOCUSABLE)[index];
  if (target) target.focus();
}

// Run `handler` when Enter is pressed in any element matching `sel`.
export function onEnter(sel, handler) {
  on(sel, "keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      handler(e);
    }
  });
}
