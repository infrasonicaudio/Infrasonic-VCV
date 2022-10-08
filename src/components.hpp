#pragma once
#include "plugin.hpp"
#include <app/ModuleLightWidget.hpp>

/// Custom VCV Rack component widgets

namespace infrasonic  {
	
template <typename TBase = GrayModuleLightWidget>
struct TBlueRedLight : TBase {
	TBlueRedLight() {
		this->addBaseColor(nvgRGB(0x22, 0x22, 0xf9));
		this->addBaseColor(nvgRGB(0xf9, 0x22, 0x22));
	}
};
using BlueRedLight = TBlueRedLight<>;

struct Switch2 : app::SvgSwitch {
	Switch2() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-sw-down.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-sw-up.svg")));
	}
};

struct Switch3 : app::SvgSwitch {
	Switch3() {
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-sw-down.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-sw-center.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-sw-up.svg")));
	}
};

struct Button : app::SvgSwitch {
	Button() {
		momentary = true;
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-btn-up.svg")));
		addFrame(Svg::load(asset::plugin(pluginInstance, "res/components/infrasonic-btn-down.svg")));
	}
};

}