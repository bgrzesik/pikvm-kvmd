/*****************************************************************************
#                                                                            #
#    KVMD - The main PiKVM daemon.                                           #
#                                                                            #
#    Copyright (C) 2018-2022  Maxim Devaev <mdevaev@gmail.com>               #
#                                                                            #
#    This program is free software: you can redistribute it and/or modify    #
#    it under the terms of the GNU General Public License as published by    #
#    the Free Software Foundation, either version 3 of the License, or       #
#    (at your option) any later version.                                     #
#                                                                            #
#    This program is distributed in the hope that it will be useful,         #
#    but WITHOUT ANY WARRANTY; without even the implied warranty of          #
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           #
#    GNU General Public License for more details.                            #
#                                                                            #
#    You should have received a copy of the GNU General Public License       #
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.  #
#                                                                            #
*****************************************************************************/

#include <memory>
#include <set>
#include <cinttypes>

#include "usb-keymap.h"
#include "factory.h"
#include "serial.h"

#include <USBMouseKeyboard.h>
#include <USB/PluggableUSBDevice.h>


namespace {
#ifdef HID_DYNAMIC
#error Unsupported
#endif

#if defined(HID_SET_USB_MOUSE_ABS)
USBMouseKeyboard gKbMouse(true, ABS_MOUSE);
#elif defined(HID_SET_USB_MOUSE_REL)
USBMouseKeyboard gKbMouse(true, REL_MOUSE);
#endif

class UsbMouse : public DRIVERS::Mouse {
public:
	UsbMouse(DRIVERS::type _type)
		: DRIVERS::Mouse(_type) {}

	virtual void begin() override {
	}

	virtual void clear() override {
		mButtons = 0;
		mPositionX = 0;
		mPositionY = 0;
		mScroll = 0;
	}

	virtual void sendButtons(
		bool leftSelect, bool leftState,
		bool rightSelect, bool rightState,
		bool middleSelect, bool middleState,
		bool upSelect, bool upState,
		bool downSelect, bool downState) override {

		if (leftState) {
			mButtons |= MOUSE_LEFT;
		} else {
			mButtons &= ~MOUSE_LEFT;
		}

		if (rightSelect) {
			mButtons |= MOUSE_RIGHT;
		} else {
			mButtons &= ~MOUSE_RIGHT;
		}

		if (middleState) {
			mButtons |= MOUSE_MIDDLE;
		} else {
			mButtons &= ~MOUSE_MIDDLE;
		}

		sendUpdate();
	}

	virtual void sendRelative(int x, int y) override {
		gKbMouse.move(x, y);
	}

	virtual void sendMove(int x, int y) override {
		mPositionX = x;
		mPositionY = y;
		sendUpdate();
	}

	virtual void sendWheel(int z) override {
		mScroll = z;
		sendUpdate();
	}

	virtual bool isOffline() override {
		return false;
	}

private:
	void sendUpdate() {
		gKbMouse.update(mPositionX, mPositionY, mButtons, mScroll);
	}

	int mPositionX;
	int mPositionY;
	int mScroll;
	int mButtons;

};

class UsbKeyboard : public DRIVERS::Keyboard {
public:
	UsbKeyboard()
		: DRIVERS::Keyboard(DRIVERS::USB_KEYBOARD) {}

	virtual void begin() override {
	}

	virtual void clear() override {
		mKeys.clear();
	}

	virtual void sendKey(uint8_t code, bool state) override {
		int mod = codeToModifier(code);

		if (mod != -1) {
			if (state) {
				mModifiers |= mod;
			} else {
				mModifiers &= ~mod;
			}
		} else {
			int key = keymapUsb(code);

			if (state) {
				if (mKeys.size() < 5) {
					mKeys.emplace(key);
				}
			} else {
				auto iter = mKeys.find(key);
				if (iter != mKeys.end()) {
					mKeys.erase(iter);
				}
			}
		}

		sendRaport();
	}

	virtual bool isOffline() override {
		return false;
	}

private:
	int codeToModifier(int code) {
		switch (code) {
		case 77:
			return KEY_CTRL; // ControlLeft
		case 78:
			return KEY_SHIFT; // ShiftLeft
		case 79:
			return KEY_ALT; // AltLeft
		case 80:
			return KEY_LOGO; // MetaLeft
		case 81:
			return KEY_RCTRL; // ControlRight
		case 82:
			return KEY_RSHIFT; // ShiftRight
		case 83:
			return KEY_RALT; // AltRight
		default:
			return -1;
		}
	}

	void sendRaport() {
		HID_REPORT report;
		report.data[0] = REPORT_ID_KEYBOARD;
		report.data[1] = mModifiers;
		report.data[2] = 0;

		report.data[3] = 0;
		report.data[4] = 0;
		report.data[5] = 0;
		report.data[6] = 0;
		report.data[7] = 0;

		report.data[8] = 0;

		auto iter = mKeys.cbegin();
		for (int i = 0; i < min(mKeys.size(), 1); i++) {
			report.data[3 + i] = *iter;
			iter++;
		}

		report.length = 9;

		gKbMouse.send(&report);
	}

	int mModifiers;
	std::set<int> mKeys;

};

class PicoBoard : public DRIVERS::Board {
public:
	PicoBoard(): DRIVERS::Board(DRIVERS::BOARD) {}

	virtual void reset() override {
		__NVIC_SystemReset();
	}

	virtual void periodic() override {
	}

	virtual void updateStatus(DRIVERS::status status) override {
	}
};

};

namespace DRIVERS {

	Keyboard *Factory::makeKeyboard(type _type) {
#if defined(HID_DYNAMIC)
		switch (_type) {
		case USB_KEYBOARD:
			return new UsbKeyboard();
		default:
			return new Keyboard(DUMMY);
		}
#elif defined(HID_SET_USB_KBD)
		return new UsbKeyboard();
#else
		return new Keyboard(DUMMY);
#endif
	}

	Mouse *Factory::makeMouse(type _type) {
#if defined(HID_DYNAMIC)
		switch(_type) {
		case USB_MOUSE_ABSOLUTE:
			return new UsbMouse(DRIVERS::USB_MOUSE_ABSOLUTE);
		case USB_MOUSE_RELATIVE:
			return new UsbMouse(DRIVERS::USB_MOUSE_RELATIVE);
		default:
			return new Mouse(DRIVERS::DUMMY);
		}
#elif defined(HID_SET_USB_MOUSE_ABS)
		return new UsbMouse(DRIVERS::USB_MOUSE_ABSOLUTE);
#elif defined(HID_SET_USB_MOUSE_REL)
		return new UsbMouse(DRIVERS::USB_MOUSE_RELATIVE);
#else
		return new Mouse(DRIVERS::DUMMY);
#endif
	}

	Storage *Factory::makeStorage(type _type) {
		switch (_type) {
		default:
			return new Storage(DRIVERS::DUMMY);
		}
	}

	Board *Factory::makeBoard(type _type) {
		switch (_type) {
		case BOARD:
			return new PicoBoard();
		default:
			return new Board(DRIVERS::DUMMY);
        }
	}

	Connection *Factory::makeConnection(type _type) {
#		ifdef CMD_SERIAL
		return new Serial();
#		else
#		error CMD phy is not defined
#		endif
	}
}
