/*
 *      Copyright (C) 2015 Team XBMC
 *      http://xbmc.org
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
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "kodi/kodi_game_types.h"

#include <stdint.h>
#include <string>
#include <vector>

struct retro_input_descriptor;

namespace LIBRETRO
{
  typedef unsigned int libretro_device_t;
  typedef uint64_t     libretro_device_caps_t;

  struct SInputDevice
  {
    SInputDevice(void)
      : caps(), bDeviceConnected(true) { }
    SInputDevice(const char* deviceId, const game_input_device_caps& deviceCaps)
      : deviceId(deviceId), caps(deviceCaps), bDeviceConnected(true) { }

    std::string            deviceId; // Input device's addon ID, e.g. game.controller.default
    game_input_device_caps caps;     // Specifies what the input device is capable of
    bool                   bDeviceConnected;
  };

  class CInputManager
  {
  private:
    CInputManager(void);

  public:
    static CInputManager& Get(void);

    libretro_device_caps_t GetDeviceCaps(void);

    void OpenPorts(void);
    void ClosePorts(void);

    libretro_device_t UpdatePort(unsigned int port, bool bPortConnected);

    void EnableSource(bool bEnabled, unsigned int port, GAME_INPUT_EVENT_SOURCE source, unsigned int index);

    void InputEvent(unsigned int port, const game_input_event& event);

    void SetInputDescriptors(const retro_input_descriptor* descriptors);

    bool DigitalButtonState(unsigned int port, unsigned int index);
    bool AnalogStickState(unsigned int port, unsigned int index, float& x, float& y);
    bool AccelerometerState(unsigned int port, unsigned int index, float& x, float& y, float& z);
    int16_t MouseDeltaX(unsigned int port);
    int16_t MouseDeltaY(unsigned int port);

  private:
    unsigned int              m_maxDevices;
    std::vector<SInputDevice> m_devices;
  };
}
