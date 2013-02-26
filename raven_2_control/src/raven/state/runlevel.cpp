/*
 * runlevel.cpp
 *
 *  Created on: Oct 14, 2012
 *      Author: benk
 */

#include <raven/state/runlevel.h>

#include "log.h"
#include <ros/ros.h>

#include <raven/state/device.h>

#include <boost/thread/recursive_mutex.hpp>

//std::atomic<unsigned int> RunLevel::LOOP_NUMBER(0);

#define USE_RUNLEVEL_MUTEX
boost::recursive_mutex runlevelMutex;
RunLevel* RunLevel::INSTANCE = new RunLevel(RunLevel::_E_STOP_HARDWARE_());
bool RunLevel::PEDAL = false;
bool RunLevel::SOFTWARE_ESTOP = false;
bool RunLevel::IS_INITED = false;
bool RunLevel::HAS_HOMED = false;
std::map<int,bool> RunLevel::ARMS_ACTIVE;

RunLevel RunLevel::_E_STOP_() { return RunLevel::_E_STOP_SOFTWARE_(); }
RunLevel RunLevel::_E_STOP_SOFTWARE_() { return RunLevel(0,1); }
RunLevel RunLevel::_E_STOP_HARDWARE_() { return RunLevel(0,0); }
RunLevel RunLevel::_INIT_(runlevel_t sublevel) { return RunLevel(1,sublevel); }
RunLevel RunLevel::_PEDAL_UP_() { return RunLevel(2); }
RunLevel RunLevel::_PEDAL_DOWN_() { return RunLevel(3); }

void RunLevel::updateRunlevel(runlevel_t level) {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	if (SOFTWARE_ESTOP) {
		setInitialized(false);
		if (level != 0) {
			*INSTANCE = _E_STOP_SOFTWARE_();
		} else {
			err_msg("*** ENTERED SOFTWARE E-STOP STATE ***\n");
			*INSTANCE = _E_STOP_HARDWARE_();
			SOFTWARE_ESTOP = false;
		}
	} else if (INSTANCE->value_ != level) {
		if (ROS_UNLIKELY(!HAS_HOMED) && level >= 2) {
			log_msg("Homing completed");
			HAS_HOMED = true;
		}
		*INSTANCE = RunLevel(level,0);
		if (level == 0) {
			err_msg("*** ENTERED E-STOP STATE ***\n");
		} else {
			log_msg("Entered runlevel %s", INSTANCE->str().c_str());
		}
	}
}

bool RunLevel::isEstop() const {
	return value_ == 0;
}

bool RunLevel::isHardwareEstop() const {
	return isEstop() && sublevel_ == 0;
}
bool RunLevel::isSoftwareEstop() const {
	return isEstop() && sublevel_ == 1;
}

bool RunLevel::isInit() const {
	return value_ == 1;
}

bool RunLevel::isInitSublevel(const runlevel_t& sublevel) const {
	return value_ == 1 && sublevel == sublevel_;
}

bool RunLevel::isPedalUp() const {
	return value_ == 2;
}

bool RunLevel::isPedalDown() const {
	return value_ == 3;
}

/*
bool RunLevel::isActive() const {
	return value_ >= 2;
}
*/

std::string
RunLevel::str() const {
	switch (value_) {
	case 0:
		return "E_STOP";
	case 1:
		switch (sublevel_) {
		case 0:
			return "INIT:0";
		case 1:
			return "INIT:1";
		case 2:
			return "INIT:2";
		case 3:
			return "INIT:3";
		default:
			std::stringstream ss;
			ss << "INIT:UNKNOWN[" << sublevel_ << "]";
			return ss.str();
		}
		break;
	case 2:
		return "PEDAL_UP";
	case 3:
		return "PEDAL_DOWN";
	default:
		std::stringstream ss;
		ss << "UNKNOWN[" << value_ << "]";
		return ss.str();
	}
}

RunLevel
RunLevel::get() {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	RunLevel rl(*INSTANCE);
	rl.armsActive_ = ARMS_ACTIVE;
	return rl;
}

void RunLevel::setSublevel(runlevel_t sublevel) {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	if (INSTANCE->isInit() && INSTANCE->sublevel_ != sublevel) {
		INSTANCE->sublevel_ = sublevel;
		log_msg("Entered runlevel %s", INSTANCE->str().c_str());
	}
}

bool
RunLevel::hasHomed() {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	return HAS_HOMED;
}

bool
RunLevel::isInitialized() {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	return IS_INITED;
}

void
RunLevel::setInitialized(bool value) {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	IS_INITED = value;
}

void
RunLevel::eStop() {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	SOFTWARE_ESTOP = true;
}
/*
void
RunLevel::setPedal(bool down) {
#ifdef USE_RUNLEVEL_MUTEX
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
#endif
	RunLevel::PEDAL = down;
}
*/
bool
RunLevel::getPedal() {
	bool pedal = false;
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	//pedal = RunLevel::PEDAL;
	std::map<int,bool>::const_iterator itr;
	for (itr = ARMS_ACTIVE.begin();itr!=ARMS_ACTIVE.end();itr++) {
		if (itr->second) {
			pedal = true;
			break;
		}
	}
	return pedal;
}
void
RunLevel::setArmActive(int armId,bool active) {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	if (armId == Arm::ALL_ARMS) {
		FOREACH_ARM_ID(armId) {
			RunLevel::ARMS_ACTIVE[armId] = active;
		}
	} else {
		RunLevel::ARMS_ACTIVE[armId] = active;
	}
}
bool
RunLevel::isArmActive(int armId) const {
	boost::recursive_mutex::scoped_lock l(runlevelMutex);
	if (!isPedalDown()) {
		return false;
	}
	std::map<int,bool>::const_iterator itr = armsActive_.find(armId);
	if (itr == armsActive_.end()) {
		return false;
	} else {
		return itr->second;
	}
}
