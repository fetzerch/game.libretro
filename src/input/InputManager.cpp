/*
 *      Copyright (C) 2015-2016 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "InputManager.h"
#include "LibretroDevice.h"
#include "LibretroDeviceInput.h"
#include "libretro/ClientBridge.h"
#include "libretro/libretro.h"
#include "libretro/LibretroEnvironment.h"
#include "libretro/LibretroTranslator.h"
#include "log/Log.h"

#include "libKODI_game.h"

#include <algorithm>

using namespace LIBRETRO;
using namespace P8PLATFORM;

CInputManager& CInputManager::Get(void)
{
  static CInputManager _instance;
  return _instance;
}

CInputManager::CInputManager()
{
  game_controller mouseControllerStruct = {};
  mouseControllerStruct.controller_id = "game.controller.mouse";
  m_mouseDevice = std::make_shared<CLibretroDevice>(&mouseControllerStruct);
}

libretro_device_caps_t CInputManager::GetDeviceCaps(void) const
{
  return 1 << RETRO_DEVICE_JOYPAD   |
         1 << RETRO_DEVICE_MOUSE    |
         1 << RETRO_DEVICE_KEYBOARD |
         1 << RETRO_DEVICE_LIGHTGUN |
         1 << RETRO_DEVICE_ANALOG   |
         1 << RETRO_DEVICE_POINTER;
}

void CInputManager::DeviceConnected(unsigned int port, bool bConnected, const game_controller* connectedDevice)
{
  if (bConnected)
    m_devices[port] = std::make_shared<CLibretroDevice>(connectedDevice);
  else
    m_devices[port].reset();
}

libretro_device_t CInputManager::GetDevice(unsigned int port)
{
  libretro_device_t deviceType = 0;

  if (m_devices[port])
    deviceType = m_devices[port]->Type();

  return deviceType;
}

bool CInputManager::OpenPort(unsigned int port)
{
  if (!CLibretroEnvironment::Get().GetFrontend())
    return false;

  CLibretroEnvironment::Get().GetFrontend()->OpenPort(port);

  return true;
}

DevicePtr CInputManager::GetPort(unsigned int port)
{
  return m_devices[port];
}

void CInputManager::ClosePort(unsigned int port)
{
  if (CLibretroEnvironment::Get().GetFrontend())
    CLibretroEnvironment::Get().GetFrontend()->ClosePort(port);

  m_devices[port].reset();
}

void CInputManager::ClosePorts(void)
{
  std::vector<unsigned int> ports;
  for (auto it = m_devices.begin(); it != m_devices.end(); ++it)
  {
    if (it->second)
      ports.push_back(it->first);
  }

  for (auto port : ports)
    ClosePort(port);
}

void CInputManager::EnableAnalogSensors(unsigned int port, bool bEnabled)
{
  // TODO
}

bool CInputManager::InputEvent(const game_input_event& event)
{
  bool bHandled = false;

  if (event.type == GAME_INPUT_EVENT_KEY)
  {
    // Report key to client
    CClientBridge* clientBridge = CLibretroEnvironment::Get().GetClientBridge();
    if (clientBridge)
    {
      const bool      down          = event.key.pressed;
      const retro_key keycode       = LibretroTranslator::GetKeyCode(event.key.character);
      const uint32_t  character     = event.key.character;
      const retro_mod key_modifiers = LibretroTranslator::GetKeyModifiers(event.key.modifiers);

      dsyslog("Key %s: %s (0x%04x)", down ? "down" : "up",
          LibretroTranslator::GetKeyName(event.key.character), character);

      clientBridge->KeyboardEvent(down, keycode, character, key_modifiers);
    }

    // Record key press for polling
    HandlePress(event.key);

    bHandled = true;
  }
  else if (event.port == GAME_INPUT_PORT_MOUSE)
  {
    bHandled = m_mouseDevice->Input().InputEvent(event);
  }
  else
  {
    const unsigned int port = event.port;

    if (m_devices[port])
      bHandled = m_devices[port]->Input().InputEvent(event);
  }

  return bHandled;
}

void CInputManager::LogInputDescriptors(const retro_input_descriptor* descriptors)
{
  dsyslog("Libretro input bindings:");
  dsyslog("------------------------------------------------------------");

  for (const retro_input_descriptor* descriptor = descriptors;
      descriptor != nullptr && descriptor->description != nullptr && !std::string(descriptor->description).empty();
      descriptor++)
  {
    std::string component = LibretroTranslator::GetComponentName(descriptor->device, descriptor->index, descriptor->id);

    if (component.empty())
    {
      dsyslog("Port: %u, Device: %s, Feature: %s, Description: %s",
          descriptor->port,
          LibretroTranslator::GetDeviceName(descriptor->device),
          LibretroTranslator::GetFeatureName(descriptor->device, descriptor->index, descriptor->id),
          descriptor->description ? descriptor->description : "");
    }
    else
    {
      dsyslog("Port: %u, Device: %s, Feature: %s, Component: %s, Description: %s",
          descriptor->port,
          LibretroTranslator::GetDeviceName(descriptor->device),
          LibretroTranslator::GetFeatureName(descriptor->device, descriptor->index, descriptor->id),
          component.c_str(),
          descriptor->description ? descriptor->description : "");
    }
  }

  dsyslog("------------------------------------------------------------");
}

std::string CInputManager::ControllerID(unsigned int port)
{
  std::string controllerId;

  if (m_devices[port])
    controllerId = m_devices[port]->ControllerID();

  return controllerId;
}

bool CInputManager::ButtonState(libretro_device_t device, unsigned int port, unsigned int buttonIndex)
{
  bool bState = false;

  if (device == RETRO_DEVICE_KEYBOARD)
  {
    bState = IsPressed(buttonIndex);
  }
  else if (device == RETRO_DEVICE_MOUSE)
  {
    bState = m_mouseDevice->Input().ButtonState(buttonIndex);
  }
  else
  {
    if (m_devices[port])
    {
      bState = m_devices[port]->Input().ButtonState(buttonIndex);
    }
  }

  return bState;
}

int CInputManager::DeltaX(libretro_device_t device, unsigned int port)
{
  int deltaX = 0;

  if (device == RETRO_DEVICE_MOUSE)
  {
    deltaX = m_mouseDevice->Input().RelativePointerDeltaX();
  }
  else if (device == RETRO_DEVICE_LIGHTGUN)
  {
    if (m_devices[port])
    {
      deltaX = m_devices[port]->Input().RelativePointerDeltaX();
    }
  }

  return deltaX;
}

int CInputManager::DeltaY(libretro_device_t device, unsigned int port)
{
  int deltaY = 0;

  if (device == RETRO_DEVICE_MOUSE || device == RETRO_DEVICE_LIGHTGUN)
  {
    deltaY = m_mouseDevice->Input().RelativePointerDeltaY();
  }
  else if (device == RETRO_DEVICE_LIGHTGUN)
  {
    if (m_devices[port])
    {
      deltaY = m_devices[port]->Input().RelativePointerDeltaX();
    }
  }

  return deltaY;
}

bool CInputManager::AnalogStickState(unsigned int port, unsigned int analogStickIndex, float& x, float& y)
{
  bool bSuccess = false;

  if (m_devices[port])
  {
    bSuccess = m_devices[port]->Input().AnalogStickState(analogStickIndex, x, y);
  }

  return bSuccess;
}

bool CInputManager::AbsolutePointerState(unsigned int port, unsigned int pointerIndex, float& x, float& y)
{
  bool bSuccess = false;

  if (m_devices[port])
  {
    bSuccess = m_devices[port]->Input().AbsolutePointerState(pointerIndex, x, y);
  }

  return bSuccess;
}

bool CInputManager::AccelerometerState(unsigned int port, float& x, float& y, float& z)
{
  bool bSuccess = false;

  if (m_devices[port])
  {
    bSuccess = m_devices[port]->Input().AccelerometerState(x, y, z);
  }

  return bSuccess;
}

void CInputManager::SetControllerInfo(const retro_controller_info* info)
{
  dsyslog("Libretro controller info:");
  dsyslog("------------------------------------------------------------");

  for (unsigned int i = 0; i < info->num_types; i++)
  {
    const retro_controller_description& type = info->types[i];

    libretro_device_t baseType = type.id & RETRO_DEVICE_MASK;
    unsigned int subclass = type.id >> RETRO_DEVICE_TYPE_SHIFT;
    std::string description = type.desc ? type.desc : "";

    dsyslog("Device: %s, Subclass: %u, Description: %s",
        LibretroTranslator::GetDeviceName(baseType), subclass, description.c_str());
  }

  dsyslog("------------------------------------------------------------");
}

void CInputManager::HandlePress(const game_key_event& key)
{
  CLockObject lock(m_keyMutex);

  if (key.pressed)
  {
    m_pressedKeys.push_back(key);
  }
  else
  {
    m_pressedKeys.erase(std::remove_if(m_pressedKeys.begin(), m_pressedKeys.end(),
      [&key](const game_key_event& pressedKey)
      {
        return pressedKey.character == key.character;
      }), m_pressedKeys.end());
  }
}

bool CInputManager::IsPressed(uint32_t character)
{
  CLockObject lock(m_keyMutex);

  return std::count_if(m_pressedKeys.begin(), m_pressedKeys.end(),
    [character](const game_key_event& keyEvent)
    {
      return keyEvent.character == character;
    }) > 0;
}
