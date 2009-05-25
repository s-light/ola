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
 * ArtNetPlugin.cpp
 * The ArtNet plugin for lla
 * Copyright (C) 2005-2009 Simon Newton
 */

#include <stdlib.h>
#include <stdio.h>

#include <lla/Closure.h>
#include <llad/PluginAdaptor.h>
#include <llad/Preferences.h>

#include "ArtNetPlugin.h"
#include "ArtNetDevice.h"


/*
 * Entry point to this plugin
 */
extern "C" lla::AbstractPlugin* create(const lla::PluginAdaptor *adaptor) {
  return new lla::plugin::ArtNetPlugin(adaptor);
}

/*
 * Called when the plugin is unloaded
 */
extern "C" void destroy(lla::Plugin* plugin) {
  delete plugin;
}


namespace lla {
namespace plugin {

const string ArtNetPlugin::ARTNET_LONG_NAME = "lla - ArtNet node";
const string ArtNetPlugin::ARTNET_SHORT_NAME = "lla - ArtNet node";
const string ArtNetPlugin::ARTNET_SUBNET = "0";
const string ArtNetPlugin::PLUGIN_NAME = "ArtNet Plugin";
const string ArtNetPlugin::DEVICE_NAME = "ArtNet Device";
const string ArtNetPlugin::PLUGIN_PREFIX = "artnet";

/*
 * Start the plugin, for now we just have one device.
 * TODO: allow multiple devices on different IPs ?
 * @returns true if we started ok, false otherwise
 */
bool ArtNetPlugin::StartHook() {
  m_device = new ArtNetDevice(this, DEVICE_NAME, m_preferences, m_debug);

  if (!m_device->Start()) {
    delete m_device;
    return false;
  }

  ConnectedSocket *socket = m_device->GetSocket();
  m_plugin_adaptor->AddSocket(socket,
                              NewClosure(m_device, &ArtNetDevice::SocketReady,
                                         socket));
  m_plugin_adaptor->RegisterDevice(m_device);
  return true;
}


/*
 * Stop the plugin
 * @return true on sucess, false on failure
 */
bool ArtNetPlugin::StopHook() {
  if (m_device) {
    m_plugin_adaptor->RemoveSocket(m_device->GetSocket());

    // stop the device
    bool ret = m_device->Stop();

    m_plugin_adaptor->UnregisterDevice(m_device);
    delete m_device;
    return ret;
  }
  return true ;
}


/*
 * Return the description for this plugin.
 * @return a string description of the plugin
 */
string ArtNetPlugin::Description() const {
    return
"ArtNet Plugin\n"
"----------------------------\n"
"\n"
"This plugin creates a single device with four input and four output ports.\n"
"\n"
"Art-Net has the concept of 'ports' on a device. Each device can support a\n"
"maximum of 4 ports in each direction and each port is assigned a universe\n"
"address in the range 0-255. When sending data from a (lla) port, the data\n"
"is addressed to the universe the (lla) port is patched to. For example if\n"
"(lla) port 0 is patched to universe 10, the data will be sent to Art-Net\n"
"universe 10.\n\n"
"--- Config file : lla-artnet.conf ---\n"
"\n"
"ip = a.b.c.d\n"
"The ip address to bind to. If not specified it will use the first\n"
"non-loopback ip.\n"
"\n"
"long_name = lla - ArtNet node\n"
"The long name of the node.\n"
"\n"
"short_name = lla - ArtNet node\n"
"The short name of the node (first 17 chars will be used)\n"
"\n"
"subnet = 0\n"
"The ArtNet subnet to use (0-15).\n";
}


/*
 * Load the plugin prefs and default to sensible values
 *
 */
bool ArtNetPlugin::SetDefaultPreferences() {
  bool save = false;

  if (!m_preferences)
    return false;

  // we don't worry about ip here
  // if it doesn't exist we'll choose one
  if (m_preferences->GetValue("short_name") == "") {
    m_preferences->SetValue("short_name", ARTNET_SHORT_NAME);
    save = true;
  }

  if (m_preferences->GetValue("long_name") == "") {
    m_preferences->SetValue("long_name", ARTNET_LONG_NAME);
    save = true;
  }

  if (m_preferences->GetValue("subnet") == "") {
    m_preferences->SetValue("subnet", ARTNET_SUBNET);
    save = true;
  }

  if (save)
    m_preferences->Save();

  // check if this save correctly
  // we don't want to use it if null
  if (m_preferences->GetValue("short_name") == "" ||
      m_preferences->GetValue("long_name") == "" ||
      m_preferences->GetValue("subnet") == "" )
    return false;

  return true;
}

} //plugin
} // lla
