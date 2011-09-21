/*
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
 * EnttecUsbProWidget.h
 * The Enttec USB Pro Widget
 * TODO(simon): implement RDM for this device - bug #146.
 * It doesn't do discovery onboard through which makes thing pretty difficult.
 * Copyright (C) 2010 Simon Newton
 */

#include <string.h>
#include "ola/BaseTypes.h"
#include "ola/Logging.h"
#include "ola/rdm/RDMCommand.h"
#include "ola/rdm/RDMEnums.h"
#include "ola/rdm/UID.h"
#include "ola/rdm/UIDSet.h"
#include "plugins/usbpro/BaseUsbProWidget.h"
#include "plugins/usbpro/EnttecUsbProWidget.h"

namespace ola {
namespace plugin {
namespace usbpro {

using ola::rdm::RDMCommand;
using ola::rdm::RDMRequest;
using ola::rdm::UID;
using ola::rdm::UIDSet;


/*
 * New Enttec Usb Pro Device.
 * This also works for the RDM Pro with the standard firmware loaded.
 */
EnttecUsbProWidgetImpl::EnttecUsbProWidgetImpl(
  ola::thread::SchedulerInterface *scheduler,
  ola::network::ConnectedDescriptor *descriptor)
    : BaseUsbProWidget(descriptor),
      m_scheduler(scheduler),
      m_active(true),
      // m_rdm_timeout_id(ola::thread::INVALID_TIMEOUT),
      m_dmx_callback(NULL)
      /*
      m_uid_set_callback(NULL),
      m_discovery_callback(NULL),
      m_rdm_request_callback(NULL),
      m_pending_request(NULL),
      m_transaction_number(0)
      */ {
}


/*
 * Shutdown
 */
EnttecUsbProWidgetImpl::~EnttecUsbProWidgetImpl() {
  Stop();

  // timeout any existing message
  /*
  if (m_rdm_request_callback)
    m_rdm_request_callback->Run(ola::rdm::RDM_TIMEOUT, NULL);
  if (m_pending_request)
    delete m_pending_request;

  if (m_uid_set_callback)
    delete m_uid_set_callback;

  if (m_discovery_callback)
    delete m_discovery_callback;
  */

  if (m_dmx_callback)
    delete m_dmx_callback;

  // empty params struct
  usb_pro_parameters params;
  while (!m_outstanding_param_callbacks.empty()) {
    usb_pro_params_callback* &callback = m_outstanding_param_callbacks.front();
    callback->Run(false, params);
    m_outstanding_param_callbacks.pop_front();
  }
}


/**
 * Set the callback to run when new DMX data arrives
 */
void EnttecUsbProWidgetImpl::SetDMXCallback(ola::Callback0<void> *callback) {
  if (m_dmx_callback)
    delete m_dmx_callback;
  m_dmx_callback = callback;
}


/**
 * Set the callback used when the UIDSet changes
void EnttecUsbProWidgetImpl::SetUIDListCallback(
    ola::Callback1<void, const ola::rdm::UIDSet&> *callback) {
  if (m_uid_set_callback)
    delete m_uid_set_callback;
  m_uid_set_callback = callback;
}


 * Set the callback to be run when discovery completes
void EnttecUsbProWidgetImpl::SetDiscoveryCallback(ola::Callback0<void> *callback) {
  if (m_discovery_callback)
    delete m_discovery_callback;
  m_discovery_callback = callback;
}
 */


/**
 * Stop the rdm discovery process if it's running
 */
void EnttecUsbProWidgetImpl::Stop() {
  m_active = false;
  /*
  if (m_rdm_timeout_id != ola::network::INVALID_TIMEOUT) {
    m_ss->RemoveTimeout(m_rdm_timeout_id);
    m_rdm_timeout_id = ola::network::INVALID_TIMEOUT;
  }
  */
}


/*
 * Send a DMX message
 * @returns true if we sent ok, false otherwise
 */
bool EnttecUsbProWidgetImpl::SendDMX(const DmxBuffer &buffer) const {
  if (!m_active)
    return false;

  struct {
    uint8_t start_code;
    uint8_t dmx[DMX_UNIVERSE_SIZE];
  } widget_dmx;

  widget_dmx.start_code = 0;
  unsigned int length = DMX_UNIVERSE_SIZE;
  buffer.Get(widget_dmx.dmx, &length);
  return SendMessage(BaseUsbProWidget::DMX_LABEL,
                     reinterpret_cast<uint8_t*>(&widget_dmx),
                     length + 1);
}


/*
 * Put the device back into recv mode
 * @return true on success, false on failure
 */
bool EnttecUsbProWidgetImpl::ChangeToReceiveMode(bool change_only) {
  if (!m_active)
    return false;

  uint8_t mode = change_only;
  bool status = SendMessage(DMX_RX_MODE_LABEL, &mode, sizeof(mode));

  if (status && change_only)
    m_input_buffer.Blackout();
  return status;
}


/**
 * Return the latest DMX data
 */
const DmxBuffer &EnttecUsbProWidgetImpl::FetchDMX() const {
  return m_input_buffer;
}


/**
 * Send a request for the widget's parameters.
 * TODO(simon): add timers to these
 */
void EnttecUsbProWidgetImpl::GetParameters(usb_pro_params_callback *callback) {
  m_outstanding_param_callbacks.push_back(callback);

  uint16_t user_size = 0;
  bool r = SendMessage(PARAMETERS_LABEL,
                       reinterpret_cast<uint8_t*>(&user_size),
                       sizeof(user_size));

  if (!r) {
    // failed
    m_outstanding_param_callbacks.pop_back();
    usb_pro_parameters params = {0, 0, 0, 0, 0};
    callback->Run(false, params);
  }
}


/**
 * Set the widget's parameters. Due to the lack of confirmation, this returns
 * immediately.
 */
bool EnttecUsbProWidgetImpl::SetParameters(uint8_t break_time,
                                           uint8_t mab_time,
                                           uint8_t rate) {
  struct widget_params_s {
    uint16_t length;
    uint8_t break_time;
    uint8_t mab_time;
    uint8_t rate;
  } __attribute__((packed));

  widget_params_s widget_parameters = {
    0,
    break_time,
    mab_time,
    rate};

  bool ret = SendMessage(
      SET_PARAMETERS_LABEL,
      reinterpret_cast<uint8_t*>(&widget_parameters),
      sizeof(widget_parameters));

  if (!ret)
    OLA_WARN << "Failed to send a set params message";
  return ret;
}


/*
 * Handle a message received from the widget
 */
void EnttecUsbProWidgetImpl::HandleMessage(uint8_t label,
                                           const uint8_t *data,
                                           unsigned int length) {
  switch (label) {
    case REPROGRAM_FIRMWARE_LABEL:
      break;
    case PARAMETERS_LABEL:
      HandleParameters(data, length);
      break;
    case RECEIVED_DMX_LABEL:
      HandleDMX(data, length);
      break;
    case DMX_CHANGED_LABEL:
      HandleDMXDiff(data, length);
      break;
    case BaseUsbProWidget::SERIAL_LABEL:
      break;
    default:
      OLA_WARN << "Unknown message type " << label;
  }
}


/*
 * Called when we get new parameters from the widget.
 */
void EnttecUsbProWidgetImpl::HandleParameters(const uint8_t *data,
                                              unsigned int length) {
  if (m_outstanding_param_callbacks.empty())
    return;

  // parameters
  typedef struct {
    uint8_t firmware;
    uint8_t firmware_high;
    uint8_t break_time;
    uint8_t mab_time;
    uint8_t rate;
  } widget_parameters_reply;

  if (length < sizeof(usb_pro_parameters))
    return;

  usb_pro_parameters params;
  memcpy(&params, data, sizeof(usb_pro_parameters));

  usb_pro_params_callback *callback = m_outstanding_param_callbacks.front();
  m_outstanding_param_callbacks.pop_front();

  callback->Run(true, params);
}


/*
 * Handle the dmx frame
 */
void EnttecUsbProWidgetImpl::HandleDMX(const uint8_t *data,
                                       unsigned int length) {
  typedef struct {
    uint8_t status;
    uint8_t dmx[DMX_UNIVERSE_SIZE + 1];
  } widget_dmx;

  if (length < 2)
    return;

  const widget_dmx *widget_reply =
    reinterpret_cast<const widget_dmx*>(data);

  if (widget_reply->status) {
    OLA_WARN << "UsbPro got corrupted packet, status: " <<
      static_cast<int>(widget_reply->status);
    return;
  }

  // only handle start code = 0
  if (length > 2 && widget_reply->dmx[0] == 0) {
    m_input_buffer.Set(widget_reply->dmx + 1, length - 2);
    if (m_dmx_callback)
      m_dmx_callback->Run();
  }
  return;
}


/*
 * Handle the dmx change of state frame
 */
void EnttecUsbProWidgetImpl::HandleDMXDiff(const uint8_t *data,
                                           unsigned int length) {
  typedef struct {
    uint8_t start;
    uint8_t changed[5];
    uint8_t data[40];
  } widget_data_changed;

  if (length < sizeof(widget_data_changed)) {
    OLA_WARN << "Change of state packet was too small: " << length;
    return;
  }

  const widget_data_changed *widget_reply =
    reinterpret_cast<const widget_data_changed*>(data);

  unsigned int start_channel = widget_reply->start * 8;
  unsigned int offset = 0;

  // skip non-0 start codes, this code is pretty messed up because the USB Pro
  // doesn't seem to provide a guarantee on the ordering of packets. Packets
  // with non-0 start codes are almost certainly going to cause problems.
  if (start_channel == 0 && (widget_reply->changed[0] & 0x01) &&
      widget_reply->data[offset])
    return;

  for (int i = 0; i < 40; i++) {
    if (start_channel + i > DMX_UNIVERSE_SIZE + 1 || offset + 6 >= length)
      break;

    if (widget_reply->changed[i/8] & (1 << (i % 8)) && start_channel + i != 0) {
      m_input_buffer.SetChannel(start_channel + i - 1,
                                widget_reply->data[offset]);
      offset++;
    }
  }

  if (m_dmx_callback)
    m_dmx_callback->Run();
}


/**
 * EnttecUsbProWidget Constructor
 */
EnttecUsbProWidget::EnttecUsbProWidget(
    ola::thread::SchedulerInterface *scheduler,
    ola::network::ConnectedDescriptor *descriptor,
    unsigned int queue_size)
    : m_impl(scheduler, descriptor) {
    // m_controller(&m_impl, queue_size) {
  /*
  m_impl.SetDiscoveryCallback(
      NewCallback(this, &UsbProWidget::ResumeRDMCommands));
  */
  (void) queue_size;
}
}  // usbpro
}  // plugin
}  // ola