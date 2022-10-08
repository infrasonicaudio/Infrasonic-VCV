#include "../plugin.hpp"
#include "../components.hpp"
#include "../dsp/PDO.hpp"

using namespace rack::simd;

static const size_t NUM_PM_RATIOS = 16;
static const unsigned int PM_RATIOS[NUM_PM_RATIOS][2] = {
	// FIRST HALF - divisions and alternate ratios
	{1, 4}, {1, 3}, {1, 2}, {3, 2},
	{5, 2}, {7, 2}, {4, 3}, {5, 3},
	// SECOND HALF - integer multiples
	{1, 1}, {2, 1}, {3, 1}, {4, 1},
	{5, 1}, {6, 1}, {7, 1}, {8, 1}
};

struct WarpCore : Module {

	using Routing = infrasonic::PhaseDistortionOscillator::Routing;
	using PDType = infrasonic::PhaseDistortionOscillator::PhaseDistType;
	using WinType = infrasonic::PhaseDistortionOscillator::WindowType;
	using OutType = infrasonic::PhaseDistortionOscillator::AltOutputType;

	enum ParamId {
		TUNE_COARSE_PARAM,
		INT_PM_PARAM,
		ALG1_PARAM,
		ALG2_PARAM,
		PD1_PARAM,
		PD2_PARAM,
		PD1_ATTEN_PARAM,
		PD2_ATTEN_PARAM,
		ROUTING_PARAM,
		WINDOW_PARAM,
		PM_RATIO_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		PD1_CV_INPUT,
		PD2_CV_INPUT,
		PITCH_CV_INPUT,
		PM_CV_INPUT,
		EXT_PM_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		OSC_0_DEG_OUTPUT,
		OSC_90_DEG_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(ALGO_LIGHT, 4 * 2),
		LIGHTS_LEN
	};
		
	bool ratioMode = false;

	std::function<void(void)> onAlgoChanged = nullptr;

	WarpCore() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(TUNE_COARSE_PARAM, 0.0f, kTuneNumOctaves, 1.0f, "Frequency", " (Hz)", 2.0f, kTuneMinFreq, 0.0f);
		configParam(INT_PM_PARAM, 0.f, 1.f, 0.f, "Internal PM Level");
		configParam(PM_RATIO_PARAM, 0, NUM_PM_RATIOS - 1, 3, "PM Ratio");
		configButton(ALG1_PARAM, "Warp Algorithm A Toggle");
		configButton(ALG2_PARAM, "Warp Algorithm B Toggle");
		configParam(PD1_PARAM, 0.f, 1.f, 0.f, "Warp A Amount");
		configParam(PD2_PARAM, 0.f, 1.f, 0.f, "Warp B Amount");
		configParam(PD1_ATTEN_PARAM, -1.f, 1.f, 0.f, "Warp A CV Attenuation");
		configParam(PD2_ATTEN_PARAM, -1.f, 1.f, 0.f, "Warp B CV Attenuation");
		configSwitch(ROUTING_PARAM, 0.f, 1.f, 1.f, "Routing", {"PM Post PD", "PM Pre PD"});
		configSwitch(WINDOW_PARAM, 0.f, 2.f, 2.f, "Windowing", {"Triangle", "Sawtooth", "Off"});
		configInput(PD1_CV_INPUT, "Warp A CV");
		configInput(PD2_CV_INPUT, "Warp B CV");
		configInput(PITCH_CV_INPUT, "V/Oct Pitch CV");
		configInput(PM_CV_INPUT, "Internal PM Level CV");
		configInput(EXT_PM_INPUT, "External PM Signal");
		configOutput(OSC_0_DEG_OUTPUT, "Main");
		configOutput(OSC_90_DEG_OUTPUT, "Auxiliary");

		for (int c = 0; c < kMaxChannels; c++)
			osc[c].Init(srConfig.sampleRate * srConfig.oversampling);

		setRatioIndex(8);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		srConfig.sampleRate = e.sampleRate;
		needsSampleRateUpdate = true;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int c = 0; c < kMaxChannels; c++)
			osc[c].Reset();
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		patch.pd_type[0] = static_cast<PDType>(rand() % PDType::PD_TYPE_LAST);
		patch.pd_type[1] = static_cast<PDType>(rand() % PDType::PD_TYPE_LAST);
		setRatioIndex(rand() % NUM_PM_RATIOS);
	}

	json_t* dataToJson() override {
		json_t* json = json_object();
		json_object_set_new(json, "oversampling", json_integer(srConfig.oversampling));
		json_object_set_new(json, "pd_type_1", json_integer(patch.pd_type[0]));
		json_object_set_new(json, "pd_type_2", json_integer(patch.pd_type[1]));
		json_object_set_new(json, "pm_ratio", json_integer(ratioIndex));
		json_object_set_new(json, "alt_out_type", json_integer(patch.alt_out_type));
		return json;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* ovs = json_object_get(rootJ, "oversampling");
		if (ovs) setOversampling(static_cast<unsigned int>(json_integer_value(ovs)));

		json_t* pdType1 = json_object_get(rootJ, "pd_type_1");
		if (pdType1) patch.pd_type[0] = static_cast<PDType>(json_integer_value(pdType1));

		json_t* pdType2 = json_object_get(rootJ, "pd_type_2");
		if (pdType2) patch.pd_type[1] = static_cast<PDType>(json_integer_value(pdType2));

		json_t* ratioIndex = json_object_get(rootJ, "pm_ratio");
		if (ratioIndex) setRatioIndex(json_integer_value(ratioIndex));

		json_t* altOutType = json_object_get(rootJ, "alt_out_type");
		if (altOutType) setAltOutputType(json_integer_value(altOutType));
	}

	void process(const ProcessArgs& args) override {

		const int numChannels = std::max(inputs[PITCH_CV_INPUT].getChannels(), 1);

		if (needsSampleRateUpdate) {
			for (int c = 0; c < kMaxChannels; c++) {
				osc[c].SetSampleRate(srConfig.sampleRate * srConfig.oversampling);
				extPMBuffers[c].clear();
			}
			outputBuffer.clear();
			needsSampleRateUpdate = false;
		}

		const float sampleRate = srConfig.sampleRate;
		const int oversampling = srConfig.oversampling;
		const int ovsBlockSize = kBlockSize * oversampling;

		// Accumulate ext PM input (needs to be processed at audio rate despite buffering)
		for (int c = 0; c < numChannels; c++) {
			float extpm = inputs[EXT_PM_INPUT].getPolyVoltage(c) / 10.0f;
			for (int i = 0; i < oversampling; i++) {
				extPMBuffers[c].push(extpm);
			}
		}

		// Process kBlockSize * oversampling samples through the engine.
		// This decimates the sample rate of inputs by kBlockSize.
		if (outputBuffer.empty()) {

			// -- PM Ratio --
			setRatioIndex(fmin(roundf(params[PM_RATIO_PARAM].getValue()), NUM_PM_RATIOS - 1));

			// -- Algorithm Selection --
			if (algo1Trigger.process(params[ALG1_PARAM].getValue())) {
				if (onAlgoChanged) onAlgoChanged();
				patch.pd_type[0] = static_cast<PDType>((patch.pd_type[0] + 1) % PDType::PD_TYPE_LAST); 
			}
			if (algo2Trigger.process(params[ALG2_PARAM].getValue())) {
				if (onAlgoChanged) onAlgoChanged();
				patch.pd_type[1] = static_cast<PDType>((patch.pd_type[1] + 1) % PDType::PD_TYPE_LAST); 
			}

			// display
			if (ratioMode) {
				setRatioLEDs();
			} else {
				for (int i = 0; i < 8; i++) {
					int active = (i % 2 == 0) ? static_cast<int>(patch.pd_type[0]) : static_cast<int>(patch.pd_type[1]);
					float brightness = static_cast<int>(floorf(i / 2)) == active ? 1.0f : 0.0f;
					lights[ALGO_LIGHT + i].setBrightness(brightness);
				}
			}

			// -- Routing + Windowing --
			patch.routing = params[ROUTING_PARAM].getValue() > 0.0f ? Routing::ROUTING_PM_PRE : Routing::ROUTING_PM_POST;
			patch.win_type = static_cast<WinType>(WinType::WIN_TYPE_LAST - 1 - params[WINDOW_PARAM].getValue());

			dsp::Frame<kMaxChannels * 2> outputFrames[ovsBlockSize];

			for (int c = 0; c < numChannels; c++) {

				// -- pitch --
				float octaves = params[TUNE_COARSE_PARAM].getValue();
				octaves += inputs[PITCH_CV_INPUT].getVoltage(c);
				patch.carrier_freq = exp2f(octaves) * kTuneMinFreq;

				// -- PD Levels --
				float pd1 = params[PD1_PARAM].getValue();
				pd1 += (inputs[PD1_CV_INPUT].getPolyVoltage(c) / 10.0f) * params[PD1_ATTEN_PARAM].getValue();
				patch.pd_amt[0] = rack::math::clamp(pd1, 0.0f, 1.0f);

				float pd2 = params[PD2_PARAM].getValue();
				pd2 += (inputs[PD2_CV_INPUT].getPolyVoltage(c) / 10.0f) * params[PD2_ATTEN_PARAM].getValue();
				patch.pd_amt[1] = rack::math::clamp(pd2, 0.0f, 1.0f);

				// -- PM --
				float pm_amt = params[INT_PM_PARAM].getValue();
				pm_amt += inputs[PM_CV_INPUT].getPolyVoltage(c) / 10.0f;
				pm_amt = rack::math::clamp(pm_amt, 0.0f, 1.0f);
				patch.pm_amt = pm_amt * pm_amt;

				// -- Output --
				dsp::Frame<2> ovsFrames[kMaxOvsBlockSize];
				osc[c].ProcessBlock(patch, (float *)extPMBuffers[c].startData(), (float *)ovsFrames, ovsBlockSize);
				extPMBuffers[c].startIncr(ovsBlockSize);
				for (int i = 0; i < ovsBlockSize; i++) {
					outputFrames[i].samples[c * 2] = ovsFrames[i].samples[0];
					outputFrames[i].samples[c * 2 + 1] = ovsFrames[i].samples[1];
				}
			}

			if (oversampling == 1) {
				for (int i = 0; i < kBlockSize; i++) {
					outputBuffer.push(outputFrames[i]);
				}
			} else {
				outputSrc.setRates(static_cast<int>(sampleRate * oversampling), 
									static_cast<int>(sampleRate));
				outputSrc.setChannels(numChannels * 2);
				int inLen = ovsBlockSize;
				int outLen = kBlockSize;
				outputSrc.process(outputFrames, &inLen, outputBuffer.endData(), &outLen);
				outputBuffer.endIncr(outLen);
			}
		}

		if (!outputBuffer.empty()) {
			dsp::Frame<kMaxChannels * 2> outputFrame = outputBuffer.shift();
			for (int c = 0; c < numChannels; c++) {
				outputs[OSC_0_DEG_OUTPUT].setVoltage(outputFrame.samples[c * 2] * 5.0f, c);
				outputs[OSC_90_DEG_OUTPUT].setVoltage(outputFrame.samples[c * 2 + 1] * 5.0f, c);
			}
		}
		outputs[OSC_0_DEG_OUTPUT].setChannels(numChannels);
		outputs[OSC_90_DEG_OUTPUT].setChannels(numChannels);
	}

	void setRatioLEDs() {
		float brightness;
		unsigned int num = PM_RATIOS[ratioIndex][0];
		unsigned int denom = PM_RATIOS[ratioIndex][1];
		for (int i = 0; i < 8; i++) {
			if (num == 16)
			{
				// ALL leds on for final ratio (16:1)
				brightness = 1.0f;
			}
			else
			{
				if (i % 2 == 0) { // numerator
					brightness = (num >> (i/2)) & 1 ? 1.0f : 0.0f;
				} else {
					// denominator
					brightness = (denom >> (i/2)) & 1 ? 1.0f : 0.0f;
				}
			}
			lights[ALGO_LIGHT + i].setBrightness(brightness);
		}
	}

	unsigned int getRatioIndex() const {
		return ratioIndex;
	}

	void setRatioIndex(unsigned int idx) {
		if (idx >= NUM_PM_RATIOS) return;
		ratioIndex = idx;
		float num = static_cast<float>(PM_RATIOS[idx][0]);
		float denom = static_cast<float>(PM_RATIOS[idx][1]);
		patch.pm_ratio = num/denom;
	}

	void setWarpAlgorithm(int ch, int idx) {
		assert(ch < 2 && idx < PDType::PD_TYPE_LAST);
		auto pdType = static_cast<PDType>(idx);
		patch.pd_type[ch] = pdType;
	}

	int getWarpAlgorithm(int ch) const {
		assert(ch < 2);
		return static_cast<int>(patch.pd_type[ch]);
	}

	void setAltOutputType(int idx) {
		patch.alt_out_type  = static_cast<OutType>(idx);
	}

	int getAltOutputType() const {
		return static_cast<int>(patch.alt_out_type);
	}

	unsigned int getOversampling() const {
		return srConfig.oversampling;
	}

	void setOversampling(unsigned int ovs) {
		srConfig.oversampling = ovs;
		needsSampleRateUpdate = true;
	}

	private:
		static const int kMaxChannels = rack::engine::PORT_MAX_CHANNELS;
		static const int kBlockSize = 8;
		static const int kMaxOvsBlockSize = kBlockSize * 16;
		static_assert(kBlockSize % 4 == 0, "Block size must be a multiple of 4 for SIMD");

		static constexpr float kTuneMinFreq = 32.7f; // C1
		static constexpr float kTuneNumOctaves = 5.0f;

		infrasonic::PhaseDistortionOscillator::Patch patch;
		infrasonic::PhaseDistortionOscillator osc[kMaxChannels];

		dsp::BooleanTrigger algo1Trigger, algo2Trigger;
		dsp::SampleRateConverter<kMaxChannels * 2> outputSrc;
		dsp::DoubleRingBuffer<float, 256> extPMBuffers[kMaxChannels];
		dsp::DoubleRingBuffer<dsp::Frame<kMaxChannels * 2>, 256> outputBuffer;

		unsigned int ratioIndex = 3;

		struct SampleRateConfig {
			float sampleRate = 48000.0f;
			unsigned int oversampling = 4;
		};
		SampleRateConfig srConfig;

		std::atomic_bool needsSampleRateUpdate = ATOMIC_VAR_INIT(false);
};


static const std::string oversamplingLabels[] = {
	"1x (Disabled)",
	"2x",
	"4x",
	"8x",
	"16x"
};

static const std::string warpAlgoLabels[] = {
	"Bend",
	"Sync",
	"Pinch",
	"Fold"
};

static const std::string outTypeLabels[] = {
	"90°",
	"Sine (Unison)",
	"Sine (Sub)",
	"Phasor"
};

struct WarpCoreWidget : ModuleWidget {


	WarpCoreWidget(WarpCore* module) {
		setModule(module);

		if (module) module->onAlgoChanged = [=]() { this->setRatioMode(false); };

		setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/WarpCore.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(12.517, 22.829)), module, WarpCore::TUNE_COARSE_PARAM));
		addParam(createParamCentered<Rogan2PSWhite>(mm2px(Vec(48.465, 22.829)), module, WarpCore::INT_PM_PARAM));
		addParam(createParamCentered<infrasonic::Button>(mm2px(Vec(24.022, 38.94)), module, WarpCore::ALG1_PARAM));
		addParam(createParamCentered<infrasonic::Button>(mm2px(Vec(36.964, 38.94)), module, WarpCore::ALG2_PARAM));
		addParam(createParamCentered<Rogan2PSBlue>(mm2px(Vec(12.498, 53.131)), module, WarpCore::PD1_PARAM));
		addParam(createParamCentered<Rogan2PSRed>(mm2px(Vec(48.465, 53.131)), module, WarpCore::PD2_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(8.454, 73.347)), module, WarpCore::PD1_ATTEN_PARAM));
		addParam(createParamCentered<Trimpot>(mm2px(Vec(52.509, 73.347)), module, WarpCore::PD2_ATTEN_PARAM));
		addParam(createParamCentered<infrasonic::Switch2>(mm2px(Vec(23.499, 86.688)), module, WarpCore::ROUTING_PARAM));
		addParam(createParamCentered<infrasonic::Switch3>(mm2px(Vec(37.461, 86.688)), module, WarpCore::WINDOW_PARAM));

		auto *ratioParam = createParamCentered<Rogan2PSGreen>(mm2px(Vec(48.465, 22.829)), module, WarpCore::PM_RATIO_PARAM);
		ratioParam->hide();
		addParam(ratioParam);

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.454, 95.1)), module, WarpCore::PD1_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(52.509, 95.1)), module, WarpCore::PD2_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.454, 110.029)), module, WarpCore::PITCH_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(19.458, 110.029)), module, WarpCore::PM_CV_INPUT));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30.461, 110.029)), module, WarpCore::EXT_PM_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.465, 110.029)), module, WarpCore::OSC_0_DEG_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(52.468, 110.029)), module, WarpCore::OSC_90_DEG_OUTPUT));

		using infrasonic::BlueRedLight;
		addChild(createLightCentered<MediumLight<BlueRedLight>>(mm2px(Vec(30.48, 47.25)), module, WarpCore::ALGO_LIGHT + 0 * 2));
		addChild(createLightCentered<MediumLight<BlueRedLight>>(mm2px(Vec(30.48, 52.295)), module, WarpCore::ALGO_LIGHT + 1 * 2));
		addChild(createLightCentered<MediumLight<BlueRedLight>>(mm2px(Vec(30.48, 57.31)), module, WarpCore::ALGO_LIGHT + 2 * 2));
		addChild(createLightCentered<MediumLight<BlueRedLight>>(mm2px(Vec(30.48, 62.334)), module, WarpCore::ALGO_LIGHT + 3 * 2));
	}

	void appendContextMenu(Menu* menu) override {
		WarpCore* module = dynamic_cast<WarpCore*>(this->module);
		
		menu->addChild(new MenuSeparator);

		menu->addChild(createBoolMenuItem("Edit Internal PM Ratio", "",
			[=]() { return this->getRatioMode(); },
			[=](bool val) { this->setRatioMode(val); }
		));

		menu->addChild(new MenuSeparator);

		std::vector<std::string> warpLabels(std::begin(warpAlgoLabels), std::end(warpAlgoLabels));
		menu->addChild(createIndexSubmenuItem("Warp A Algorithm", warpLabels,
			[=]() { return module->getWarpAlgorithm(0); },
			[=](int idx) { module->setWarpAlgorithm(0, idx); } 
		));

		menu->addChild(createIndexSubmenuItem("Warp B Algorithm", warpLabels,
			[=]() { return module->getWarpAlgorithm(1); },
			[=](int idx) { module->setWarpAlgorithm(1, idx); } 
		));

		std::vector<std::string> outLabels(std::begin(outTypeLabels), std::end(outTypeLabels));
		menu->addChild(createIndexSubmenuItem("Auxiliary Output Mode", outLabels,
			[=]() { return module->getAltOutputType(); },
			[=](int idx) { module->setAltOutputType(idx); } 
		));

		menu->addChild(new MenuSeparator);

		std::vector<std::string> ovsLabels(std::begin(oversamplingLabels), std::end(oversamplingLabels));
		menu->addChild(createIndexSubmenuItem("Oversampling", ovsLabels,
			[=]() { return log2f(module->getOversampling()); },
			[=](int idx) { module->setOversampling(exp2f(idx)); }
		));
	}

	bool getRatioMode() const {
		WarpCore* module = dynamic_cast<WarpCore*>(this->module);
		return module->ratioMode;
	}

	void setRatioMode(bool enabled) {
		WarpCore* module = dynamic_cast<WarpCore*>(this->module);
		if (enabled) {
			getParam(WarpCore::INT_PM_PARAM)->hide();
			getParam(WarpCore::PM_RATIO_PARAM)->show();
		} else {
			getParam(WarpCore::INT_PM_PARAM)->show();
			getParam(WarpCore::PM_RATIO_PARAM)->hide();
		}
		module->ratioMode = enabled;;
	}
};


Model* modelWarpCore = createModel<WarpCore, WarpCoreWidget>("WarpCore");