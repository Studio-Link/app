import addcontact from "../templates/addcontact.handlebars";
import listcontacts from "../templates/listcontacts.handlebars";
import { qs, on, render, onEnter, dialog, notify, serializeForm, validateForm } from "../ui.js";
"use strict";

window.ws_contacts_init = function() {
  window.ws_contacts_list = {};
  var ws_contacts = new WebSocket("ws://" + window.ws_host + "/ws_contacts");
  window.ws_contacts = ws_contacts;

  function ws_contacts_delete(sip) {
    ws_contacts.send('{"command": "deletecontact", "sip": "' + sip + '"}');
  }

  function RefreshEventListener() {
    on("#buttonaddcontact", "click", function() {
      dialog({
        title: "Add Contact",
        message: addcontact(),
        buttons: {
          close: {
            label: "Cancel",
            callback: function() {
              return true;
            },
          },
          success: {
            label: "Save",
            className: "btn-primary btn-formaddcontact",
            callback: function() {
              if (validateForm(qs("#formaddcontact"))) {
                ws_contacts.send(
                  JSON.stringify(serializeForm(qs("#formaddcontact")))
                );
              } else {
                return false;
              }
            },
          },
        },
      });

      onEnter("#formaddcontact-name, #formaddcontact-sip", function() {
        qs(".btn-formaddcontact").click();
      });
    });

    on(".deletecontact", "click", function() {
      ws_contacts_delete(this.dataset.sip);
    });

    on(".callcontact", "click", function() {
      qs("#sipnumbercall").value = this.dataset.sip;
      qs("#buttoncall").click();
      // Show the new call (switches from the settings view)
      location.hash = "#tracks";
    });
  }

  ws_contacts.onerror = function() {
    notify("Websocket error", "error");
  };

  ws_contacts.onclose = function() {
    notify("Websocket closed", "error");
  };

  ws_contacts.onmessage = function(message) {
    var msg = JSON.parse(message.data);
    ws_contacts_list = msg;
    render(qs("#contacts"), listcontacts(ws_contacts_list));
    RefreshEventListener();
  };

  RefreshEventListener();
};
