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
 * StageProfiPlugin.cpp
 * The StageProfi plugin for lla
 * Copyright (C) 2006-2008 Simon Newton
 */

#include <vector>
#include <stdlib.h>
#include <stdio.h>

#include <lla/Closure.h>
#include <lla/Logging.h>
#include <llad/PluginAdaptor.h>
#include <llad/Preferences.h>

#include "StageProfiPlugin.h"
#include "StageProfiDevice.h"

/*
 * Entry point to this plugin
 */
extern "C" lla::AbstractPlugin* create(
    const lla::PluginAdaptor *plugin_adaptor) {
  return new lla::plugin::StageProfiPlugin(plugin_adaptor);
}

/*
 * Called when the plugin is unloaded
 */
extern "C" void destroy(lla::AbstractPlugin *plugin) {
  delete plugin;
}

namespace lla {
namespace plugin {

using std::string;

const string StageProfiPlugin::STAGEPROFI_DEVICE_PATH = "/dev/ttyUSB0";
const string StageProfiPlugin::STAGEPROFI_DEVICE_NAME = "StageProfi Device";
const string StageProfiPlugin::PLUGIN_NAME = "StageProfi Plugin";
const string StageProfiPlugin::PLUGIN_PREFIX = "stageprofi";
const string StageProfiPlugin::DEVICE_KEY = "device";

/*
 * Start the plugin
 *
 * Multiple devices now supported
 */
bool StageProfiPlugin::StartHook() {
  vector<string> device_names;
  vector<string>::iterator it;
  StageProfiDevice *device;

  // fetch device listing
  device_names = m_preferences->GetMultipleValue(DEVICE_KEY);

  for (it = device_names.begin(); it != device_names.end(); ++it) {
    if (it->empty())
      continue;

    device = new StageProfiDevice(this, STAGEPROFI_DEVICE_NAME, *it);

    if (!device->Start()) {
      delete device;
      continue;
    }

    lla::network::ConnectedSocket *socket = device->GetSocket();
    m_plugin_adaptor->AddSocket(socket,
                                NewClosure(device,
                                           &StageProfiDevice::SocketReady,
                                           socket));
    m_plugin_adaptor->RegisterDevice(device);
    m_devices.insert(m_devices.end(), device);
  }
  return true;
}


/*
 * Stop the plugin
 *
 * @return 0 on sucess, -1 on failure
 */
bool StageProfiPlugin::StopHook() {
  vector<StageProfiDevice*>::iterator iter;
  for (iter = m_devices.begin(); iter != m_devices.end(); ++iter) {
    m_plugin_adaptor->RemoveSocket((*iter)->GetSocket());
    DeleteDevice(*iter);
  }
  m_devices.clear();
  return true;
}


/*
 * Return the description for this plugin
 */
string StageProfiPlugin::Description() const {
    return
"StageProfi Plugin\n"
"----------------------------\n"
"\n"
"This plugin creates devices with one output port.\n"
"\n"
"--- Config file : lla-stageprofi.conf ---\n"
"\n"
"device = /dev/ttyUSB0\n"
"device = 192.168.1.250\n"
"The device to use either as a path for the USB version or an IP address\n"
"for the LAN version. Multiple devices are supported.\n";
}


/*
 * Called when the file descriptor is closed.
 */
void StageProfiPlugin::SocketClosed(Socket *socket) {
  vector<StageProfiDevice*>::iterator iter;

  for (iter = m_devices.begin(); iter != m_devices.end(); ++iter) {
    if ((*iter)->GetSocket() == socket)
      break;
  }

  if (iter == m_devices.end()) {
    LLA_WARN << "unknown fd";
    return;
  }

  DeleteDevice(*iter);
  m_devices.erase(iter);
}


/*
 * load the plugin prefs and default to sensible values
 *
 */
bool StageProfiPlugin::SetDefaultPreferences() {
  if (!m_preferences)
    return false;

  if (m_preferences->GetValue(DEVICE_KEY).empty()) {
    m_preferences->SetValue(DEVICE_KEY, STAGEPROFI_DEVICE_NAME);
    m_preferences->Save();
  }

  // check if this saved correctly
  // we don't want to use it if null
  if (m_preferences->GetValue(DEVICE_KEY).empty()) {
    return false;
  }
  return true;
}


/*
 * Cleanup a single device
 */
void StageProfiPlugin::DeleteDevice(StageProfiDevice *device) {
  device->Stop();
  m_plugin_adaptor->UnregisterDevice(device);
  delete device;
}

} //plugin
} //lla
