/**
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The RDM patcher.
 * Copyright (C) 2010 Simon Newton
 */

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.classes');
goog.require('goog.events');
goog.require('goog.math.Rect');
goog.require('goog.ui.Checkbox');
goog.require('goog.ui.MenuItem');
goog.require('goog.ui.Select');
goog.require('ola.CustomDragScrollSupport');
goog.require('ola.CustomDragger');
goog.require('ola.Dialog');

goog.require('ola.common.Server');
goog.require('ola.common.Server.EventType');
goog.require('ola.common.UidItem');

goog.provide('ola.RDMPatcher');
goog.provide('ola.RDMPatcherDevice');


/**
 * The model of an RDM device.
 * @param {string} uid the uid of the device.
 * @param {number} start_address the start address, from 1 - 512.
 * @param {number} footprint the number of dmx channels consumed.
 * @constructor
 */
ola.RDMPatcherDevice = function(uid, label, start_address, footprint,
                                current_personality, personality_count) {
  this.uid = uid;
  this.label = label;
  // convert to 0 offset
  this.start = start_address - 1;
  this.footprint = footprint;
  this.current_personality = current_personality;
  this.personality_count = personality_count;

  this.setStart(start_address);
  this._divs = new Array();
};


/**
 * Set the start address of this device
 * @param {number} start_address the start address, from 1 - 512.
 */
ola.RDMPatcherDevice.prototype.setStart = function(start_address) {
  this.start = start_address - 1;
  this.end = Math.min(this.start + this.footprint - 1,
                      ola.RDMPatcher.NUMBER_OF_CHANNELS);
};


/**
 * Associated a div with this Device
 */
ola.RDMPatcherDevice.prototype.resetDivs = function() {
  for (var i = 0; i < this._divs.length; ++i) {
    goog.events.removeAll(this._divs[i]);
  }
  this._divs = new Array();
};


/**
 * Associated a div with this Device
 */
ola.RDMPatcherDevice.prototype.addDiv = function(div) {
  this._divs.push(div);
};


/**
 * Get the list of divs associated with this device
 */
ola.RDMPatcherDevice.prototype.getDivs = function() {
  return this._divs;
};


/**
 * Check if a device overflows the 512 channel count.
 */
ola.RDMPatcherDevice.prototype.overflows = function(div) {
  return this.start + this.footprint > ola.RDMPatcher.NUMBER_OF_CHANNELS;
};


/**
 * A comparison function used to sort an array of devices based on start
 * address.
 */
ola.RDMPatcherDevice.sortByAddress = function(a, b) {
  return a.start - b.start;
};


/**
 * The class representing the patcher.
 * @param {string} element_id the id of the element to use for this patcher.
 * @constructor
 */
ola.RDMPatcher = function(element_id) {
  this.element = goog.dom.$(element_id);
  this.universe_id = undefined;
  // this of RDMPatcherDevice objects
  this.devices = new Array();

  // the top level divs that form each row
  this.rows = new Array();
  // the background tds used to set the row height correctly
  this.background_tds = new Array();
  // the tables that the device divs live in
  this.device_tables = new Array();

  // store a reference to the draggers so we can clean the up properly
  this.draggers = new Array();

  this.dialog = undefined;
  this.active_device = undefined;
  this.start_address_input = undefined;
  this.scroller = new ola.CustomDragScrollSupport(this.element);
};


/* Contants */
ola.RDMPatcher.NUMBER_OF_CHANNELS = 512;
ola.RDMPatcher.CHANNELS_PER_ROW = 8;
ola.RDMPatcher.HEIGHT_PER_DEVICE = 14;
ola.RDMPatcher.NUMBER_OF_ROWS = (ola.RDMPatcher.NUMBER_OF_CHANNELS /
  ola.RDMPatcher.CHANNELS_PER_ROW);


/**
 * Called when the size of the patcher changes
 */
ola.RDMPatcher.prototype.sizeChanged = function() {
  this.scroller.updateBoundaries();
};


/**
 * Set the list of devices.
 * @param {object} device the device to add.
 */
ola.RDMPatcher.prototype.setDevices = function(devices) {
  this.devices = devices;
};


/**
 * Set the universe this patcher is operating on
 * @param {object} device the device to add.
 */
ola.RDMPatcher.prototype.setUniverse = function(universe_id) {
  this.universe_id = universe_id;
};


/**
 * Hide the patcher
 */
ola.RDMPatcher.prototype.hide = function() {
  // hide all the rows
  for (var i = 0; i < this.rows.length; ++i) {
    this.rows[i].style.display = 'none';
  }
};


/**
 * Render the patcher view.
 */
ola.RDMPatcher.prototype.update = function() {
  if (this.rows.length == 0) {
    this._setup();
  }

  for (var i = 0; i < this.rows.length; ++i) {
    this.rows[i].style.display = 'block';
  }

  this._render();
};


/**
 * Setup the patcher
 * @private
 */
ola.RDMPatcher.prototype._setup = function() {
  for (var i = 0; i < ola.RDMPatcher.NUMBER_OF_ROWS; ++i) {
    var row_div = goog.dom.createElement('div');
    row_div.className = 'patch_row';
    this.rows.push(row_div);

    var spacer_table = goog.dom.createElement('table');
    spacer_table.className = 'patcher_table';
    var spacer_tr = goog.dom.createElement('tr');
    spacer_tr.id = 'str_' + i;
    for (var j = 0; j < ola.RDMPatcher.CHANNELS_PER_ROW; ++j) {
      var td = goog.dom.createElement('td');
      goog.dom.appendChild(spacer_tr, td);
      this.background_tds.push(td);
    }
    goog.dom.appendChild(spacer_table, spacer_tr);
    goog.dom.appendChild(row_div, spacer_table);

    var content_table = goog.dom.createElement('table');
    content_table.className = 'content_table';
    var title_tr = goog.dom.createElement('tr');
    for (var j = 0; j < ola.RDMPatcher.CHANNELS_PER_ROW; ++j) {
      var td = goog.dom.createElement('td');
      td.className = 'patcher_title';
      td.innerHTML = 1 + i * ola.RDMPatcher.CHANNELS_PER_ROW + j;
      goog.dom.appendChild(title_tr, td);
    }
    goog.dom.appendChild(content_table, title_tr);
    this.device_tables.push(content_table);
    goog.dom.appendChild(row_div, content_table);
    goog.dom.appendChild(this.element, row_div);
  }
};


/**
 * Render the patcher.
 * @private
 */
ola.RDMPatcher.prototype._render = function() {
  this.devices.sort(ola.RDMPatcherDevice.sortByAddress);

  // Each channel is assigned N slots, where N is the max number of devices
  // that occupy any one channel. We start out with a single slot, and add more
  // if we detect overlapping devices.
  var slots = new Array();
  slots.push(goog.array.repeat(
      false,
      ola.RDMPatcher.NUMBER_OF_CHANNELS));

  for (var i = 0; i < this.draggers.length; ++i) {
    goog.events.removeAll(this.draggers[i]);
  }
  this.draggers = new Array();

  // for each device, figure out which slot it's going to live in
  for (var i = 0; i < this.devices.length; ++i) {
    var device = this.devices[i];

    var found_free_slot = false;
    var slot;
    for (slot = 0; slot < slots.length; ++slot) {
      var channel;
      for (channel = device.start; channel <= device.end; ++channel) {
        if (slots[slot][channel])
          break;
      }
      if (channel > device.end) {
        found_free_slot = true;
        break;
      }
    }

    if (!found_free_slot) {
      slots.push(goog.array.repeat(
          false,
          ola.RDMPatcher.NUMBER_OF_CHANNELS));
    }
    // mark all appropriate channels in this slot as occupied.
    for (channel = device.start; channel <= device.end; ++channel) {
      slots[slot][channel] = device;
    }
    device.resetDivs();
  }

  // now update the cell heights according to how many slots we need
  this._updateCellHeights(slots.length);

  // now loop over all the rows in the patcher table, and render them
  for (var row = 0; row < this.device_tables.length; ++row) {
    var table = this.device_tables[row];
    var offset = row * ola.RDMPatcher.CHANNELS_PER_ROW;
    var tr = goog.dom.getFirstElementChild(table);
    tr = goog.dom.getNextElementSibling(tr);

    var slot = 0;
    // updating in place leads to less changes of memory leaks in crappy
    // browers
    while (slot < slots.length) {
      if (tr == undefined) {
        tr = goog.dom.createElement('tr');
        goog.dom.appendChild(table, tr);
      } else {
        var td = goog.dom.getFirstElementChild(tr);
        while (td != undefined) {
          goog.dom.removeChildren(td);
          td = goog.dom.getNextElementSibling(td);
        }
      }
      this._renderSlot(tr, slots[slot], offset);
      tr = goog.dom.getNextElementSibling(tr);
      slot++;
    }

    while (tr != undefined) {
      var td = goog.dom.getFirstElementChild(tr);
      while (td != undefined) {
        goog.dom.removeChildren(td);
        td.colSpan = 1;
        td = goog.dom.getNextElementSibling(td);
      }
      tr = goog.dom.getNextElementSibling(tr);
    }
  }
};


/**
 * Render a slot in the patcher table.
 * @param {element} tr the row to use. May contain tds already.
 * @private
 */
ola.RDMPatcher.prototype._renderSlot = function(tr, slot_data, start_channel) {
  var channel = start_channel;
  var end_channel = start_channel + ola.RDMPatcher.CHANNELS_PER_ROW;
  var td = goog.dom.getFirstElementChild(tr);
  while (channel < end_channel) {
    if (td == undefined) {
      td = goog.dom.createElement('td');
      goog.dom.appendChild(tr, td);
    }
    td.colSpan = 1;
    td.className = '';
    if (slot_data[channel]) {
      var device = slot_data[channel];
      td.className = 'patcher_occupied_cell';

      // work out the number of channels this spans
      var remaining_length = device.end - channel + 1;
      var colspan = Math.min(remaining_length, end_channel - channel);
      td.colSpan = colspan;
      channel += colspan;

      // create a new div
      var div = goog.dom.createElement('div');
      div.innerHTML = device.label;
      if (device.overflows()) {
        div.className = 'patcher_overflow_device';
        div.title = 'Device overflows the ' + ola.RDMPatcher.NUMBER_OF_CHANNELS
          + ' channel limit';
      } else {
        div.className = 'patcher_device';
      }
      device.addDiv(div);
      goog.dom.appendChild(td, div);
      goog.events.listen(
        div,
        goog.events.EventType.CLICK,
        function(d) {
          return function(e) { this._configureDevice(d, e); };
        }(device),
        false,
        this);

      // setup the dragger
      var dragger = new ola.CustomDragger(div);
      dragger.setScrollTarget(this.element);
      dragger.setHysteresis(8);
      goog.events.listen(
          dragger,
          goog.fx.Dragger.EventType.START,
          function(d1, d2, d3) {
            return function(e) { this._startDrag(d1, d2, e); };
          }(div, device),
          false,
          this);
      goog.events.listen(
          dragger,
          goog.fx.Dragger.EventType.END,
          function(d1, d2) {
            return function(e) { this._endDrag(d1, d2, e); };
          }(div, device),
          false,
          this);
      dragger.setScrollTarget(this.element);
      this.draggers.push(dragger);
    } else {
      channel++;
    }
    td = goog.dom.getNextElementSibling(td);
  };

  while (td != undefined) {
    var next = goog.dom.getNextElementSibling(td);
    goog.dom.removeNode(td);
    td = next;
  }
};


/**
 * Called when we start dragging a device
 */
ola.RDMPatcher.prototype._startDrag = function(div, device, e) {
  // remove any other divs associated with this device
  var device_divs = device.getDivs();
  for (var i = 0; i < device_divs.length; ++i) {
    if (div != device_divs[i]) {
      goog.dom.removeNode(device_divs[i]);
    }
  }

  var row_table = goog.dom.getFirstElementChild(this.element);
  while (!goog.dom.classes.has(row_table, 'patch_row')) {
    row_table = goog.dom.getNextElementSibling(row_table);
  }
  var row_table_size = goog.style.getBorderBoxSize(row_table);

  // figure out the cell size, remember to account for the borders
  this.cell_width = row_table_size.width / ola.RDMPatcher.CHANNELS_PER_ROW - 1;

  // reparent the div and set the styles correctly
  goog.dom.appendChild(this.element, div);
  div.className = 'draggable_device';
  goog.style.setOpacity(div, 0.50);
  div.style.width = this.cell_width + 'px';

  // set the position of the div to center under the mouse cursor
  var div_size = goog.style.getBorderBoxSize(div);
  var patcher_offset = goog.style.getClientPosition(this.element);
  div.style.top = (this.element.scrollTop + e.clientY - patcher_offset.y -
    div_size.height / 2) + 'px';
  div.style.left = (e.clientX - patcher_offset.x - div_size.width / 2) + 'px';

  // set the limits of the drag actions
  var patcher_height = ola.RDMPatcher.NUMBER_OF_ROWS * this.cell_height;
  e.dragger.setLimits(new goog.math.Rect(
        0,
        0,
        row_table_size.width - div_size.width - 1,
        patcher_height - div_size.height - 1));

  this.scroller.setEnabled(true);
  return true;
}


/**
 * Called when the dragging finishes
 */
ola.RDMPatcher.prototype._endDrag = function(div, device, e) {
  goog.style.setOpacity(div, 1);
  this.scroller.setEnabled(false);
  var box = goog.style.getBorderBoxSize(div);

  var deltaX = Math.max(0, e.dragger.deltaX);
  var deltaY = Math.max(0, e.dragger.deltaY);

  var center_x = Math.min(
    deltaX + (box.width / 2),
    this.cell_width * ola.RDMPatcher.CHANNELS_PER_ROW - 1);
  var center_y = Math.min(
    deltaY + (box.height / 2),
    this.cell_height * ola.RDMPatcher.NUMBER_OF_ROWS - 1);

  var new_start_address = (Math.floor(center_x / this.cell_width) +
        ola.RDMPatcher.CHANNELS_PER_ROW *
        Math.floor(center_y / this.cell_height))

  goog.dom.removeNode(div);
  this._setStartAddress(device, new_start_address + 1);
};


/**
 * Update the heights of the cells, given the current layout of devices
 */
ola.RDMPatcher.prototype._updateCellHeights = function(overlapping_devices) {
  // +1 for the title row, and always render at least one row so it looks nice
  var rows = 1 + Math.max(1, overlapping_devices);
  var height = rows * ola.RDMPatcher.HEIGHT_PER_DEVICE;
  for (var i = 0; i < this.rows.length; ++i) {
    this.rows[i].style.height = height + 'px';
  }

  for (var i = 0; i < this.background_tds.length; ++i) {
    this.background_tds[i].style.height = (height - 1) + 'px';
  }
  this.cell_height = height;
};


/**
 * Called when the user clicks on a device to configure it.
 */
ola.RDMPatcher.prototype._configureDevice = function(device, e) {
  if (!e.target.parentNode) {
    // this was a drag action and the div has been removed, don't show the
    // dialog.
    return;
  }

  if (this.dialog == undefined) {
    this.dialog = new goog.ui.Dialog(null, true);
    this.dialog.setButtonSet(goog.ui.Dialog.ButtonSet.OK_CANCEL);
    goog.events.listen(this.dialog,
                       goog.ui.Dialog.EventType.SELECT,
                       this._saveDevice, false, this);
    var table = goog.dom.createElement('table');

    // start address
    var tr = goog.dom.createElement('tr');
    var td = goog.dom.createElement('td');
    td.innerHTML = 'Start Address';
    goog.dom.appendChild(tr, td);
    td = goog.dom.createElement('td');
    this.start_address_input = goog.dom.createElement('input');
    goog.dom.appendChild(td, this.start_address_input);
    goog.dom.appendChild(tr, td);
    goog.dom.appendChild(table, tr);

    // personality
    var tr = goog.dom.createElement('tr');
    var td = goog.dom.createElement('td');
    td.innerHTML = 'Personality';
    goog.dom.appendChild(tr, td);
    td = goog.dom.createElement('td');
    td.noWrap = true;
    goog.dom.appendChild(tr, td);
    var select = new goog.ui.Select();
    select.addItem(new goog.ui.MenuItem('One'));
    select.render(td);
    this.personality_spinner = goog.dom.createElement('img');
    this.personality_spinner.src = '/loader-mini.gif';
    this.personality_spinner.style.display = 'none';
    this.personality_spinner.style.verticalAlign = 'middle';
    goog.dom.appendChild(td, this.personality_spinner);
    this.personality_row = tr;
    goog.dom.appendChild(table, tr);

    // now do the identify checkbox
    tr = goog.dom.createElement('tr');
    td = goog.dom.createElement('td');
    td.innerHTML = 'Identify';
    goog.dom.appendChild(tr, td);
    td = goog.dom.createElement('td');
    td.noWrap = true;
    var check = new goog.ui.Checkbox();
    check.render(td);

    goog.events.listen(
      check,
      goog.ui.Component.EventType.CHANGE,
      this._toggleIdentify,
      false,
      this);

    this.identify_box = check;
    this.identify_spinner = goog.dom.createElement('img');
    this.identify_spinner.src = '/loader-mini.gif';
    this.identify_spinner.style.display = 'none';
    this.identify_spinner.style.verticalAlign = 'middle';
    goog.dom.appendChild(td, this.identify_spinner);
    goog.dom.appendChild(tr, td);
    goog.dom.appendChild(table, tr);

    var content = this.dialog.getContentElement();
    goog.dom.appendChild(content, table);
  }

  this.active_device = device;

  // fetch the identify mode
  var server = ola.common.Server.getInstance();
  var patcher = this;
  server.rdmGetUIDIdentifyMode(
      this.universe_id,
      device.uid,
      function(e) { patcher._getIdentifyComplete(e); });

  var dialog = ola.Dialog.getInstance();
  dialog.setAsBusy();
  dialog.setVisible(true);
};


/**
 * Called when the get identify mode completes.
 */
ola.RDMPatcher.prototype._getIdentifyComplete = function(e) {

  var response = ola.common.Server.getInstance().checkForErrorLog(e);
  if (response != undefined) {
    var mode = (response['identify_mode'] ?
      goog.ui.Checkbox.State.CHECKED :
      goog.ui.Checkbox.State.UNCHECKED);
    this.identify_box.setChecked(mode);
  }

  if (this.active_device.personality_count == undefined ||
      this.active_device.personality_count < 2) {
    var dialog = ola.Dialog.getInstance();
    dialog.setVisible(false);
    this._displayConfigureDevice();
    this.personality_row.style.display = 'none';
  } else {
    // fetch personalities here
    /*
    var server = ola.common.Server.getInstance();
    var patcher = this;
    server.rdmGetUIDIdentifyMode(
        this.universe_id,
        device.uid,
        function(e) { patcher._getIdentifyComplete(e); });
    */
    this.personality_row.style.display = 'block';
  }
};


/**
 * Display the configure device dialog
 */
ola.RDMPatcher.prototype._displayConfigureDevice = function(e) {
  this.start_address_input.value = this.active_device.start + 1;
  this.dialog.setTitle(this.active_device.label);
  this.dialog.setVisible(true);
  this.start_address_input.focus();
};


/**
 * Save the start address for a device
 */
ola.RDMPatcher.prototype._saveDevice = function(e) {
  if (e.key == goog.ui.Dialog.DefaultButtonKeys.CANCEL ||
      this.start_address_input == undefined ||
      this.active_device == undefined) {
    return;
  }

  var value = parseInt(this.start_address_input.value);
  if (isNaN(value) || value < 1 || value > ola.RDMPatcher.NUMBER_OF_CHANNELS) {
    alert('Must be between 1 and ' + ola.RDMPatcher.NUMBER_OF_CHANNELS);
    return;
  }

  this._setStartAddress(this.active_device, value);
};


/**
 * Called to set the start address of a device
 */
ola.RDMPatcher.prototype._setStartAddress = function(device, start_address) {
  var server = ola.common.Server.getInstance();
  var patcher = this;
  server.rdmSetSectionInfo(
      this.universe_id,
      device.uid,
      'dmx_address',
      '',
      'address=' + start_address,
      function(e) {
        patcher._setStartAddressComplete(device, start_address, e);
      });

  var dialog = ola.Dialog.getInstance();
  dialog.setAsBusy();
  dialog.setVisible(true);
};


/**
 * Called when the start address set command completes
 */
ola.RDMPatcher.prototype._setStartAddressComplete = function(device,
                                                             start_address,
                                                             e) {
  var response = ola.common.Server.getInstance().checkForErrorDialog(e);
  if (response != undefined) {
    var dialog = ola.Dialog.getInstance();
    dialog.setVisible(false);
    device.setStart(start_address);
  }

  this._render();
};


/**
 * Set the personality.
 */
ola.RDMPatcher.prototype._setPersonality = function(e) {
  var server = ola.common.Server.getInstance();
  this.personality_spinner.style.display = 'inline';
  var patcher = this;
  server.rdmSetSectionInfo(
      this.universe_id,
      this.active_device.uid,
      'personality',
      '',
      'int=' + e.target.getValue(),
      function(e) { patcher._personalityComplete(e); });
};


/**
 * Called when the set personality request completes.
 */
ola.RDMPatcher.prototype._personalityComplete = function(e) {
  this.personality_spinner.style.display = 'none';
  var response = ola.common.Server.getInstance().checkForErrorDialog(e);
  if (response == undefined) {
    this.dialog.setVisible(false);
  }
};


/**
 * Toggle the identify mode
 */
ola.RDMPatcher.prototype._toggleIdentify = function(e) {
  var server = ola.common.Server.getInstance();
  this.identify_spinner.style.display = 'inline';
  var patcher = this;
  server.rdmSetSectionInfo(
      this.universe_id,
      this.active_device.uid,
      'identify',
      '',
      'bool=' + (e.target.isChecked() ? '1' : '0'),
      function(e) { patcher._identifyComplete(e); });
};


/**
 * Called when the identify request completes.
 */
ola.RDMPatcher.prototype._identifyComplete = function(e) {
  this.identify_spinner.style.display = 'none';
  var response = ola.common.Server.getInstance().checkForErrorDialog(e);
  if (response == undefined) {
    this.dialog.setVisible(false);
  }
};