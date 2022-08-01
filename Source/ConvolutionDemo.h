/*
  ==============================================================================

   This file is part of the JUCE examples.
   Copyright (c) 2020 - Raw Material Software Limited

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES,
   WHETHER EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR
   PURPOSE, ARE DISCLAIMED.

  ==============================================================================
*/

/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

 name:             ConvolutionDemo
 version:          1.0.0
 vendor:           JUCE
 website:          http://juce.com
 description:      Convolution demo using the DSP module.

 dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats,
                   juce_audio_processors, juce_audio_utils, juce_core,
                   juce_data_structures, juce_dsp, juce_events, juce_graphics,
                   juce_gui_basics, juce_gui_extra
 exporters:        xcode_mac, vs2019, linux_make

 moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1

 type:             Component
 mainClass:        ConvolutionDemo

 useLocalCopy:     1

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "DemoUtilities.h"
#include "DSPDemos_Common.h"

using namespace dsp;

//==============================================================================
struct ConvolutionDemoDSP {
    void prepare(const ProcessSpec &spec) {
        sampleRate = spec.sampleRate;
        convolution.prepare(spec);
        convolutionHP.prepare(spec);
        reverb.prepare(spec);

        params.damping = 0.8;
        params.dryLevel = 1;
        params.roomSize = 0.12;
        params.wetLevel = 0.02;

        reverb.setParameters(params);
        updateParameters();
    }

    void process(ProcessContextReplacing<float> context) {
        context.isBypassed = bypass;

        // Load a new IR if there's one available. Note that this doesn't lock or allocate!
        bufferTransfer.get([this](BufferWithSampleRate &buf) {
            convolution.loadImpulseResponse(std::move(buf.buffer),
                                            buf.sampleRate,
                                            Convolution::Stereo::yes,
                                            Convolution::Trim::yes,
                                            Convolution::Normalise::yes);

//            convolutionHP.loadImpulseResponse(File("/home/benni/Data/Coding/Binaural Audio/ConvolutionDemo/Builds/LinuxMakefile/build/HRTF/HpFilter.wav"),
//                                            Convolution::Stereo::yes,
//                                            Convolution::Trim::yes,
//                                            0,
//                                            Convolution::Normalise::no);
        });

        convolution.process(context);
//        convolutionHP.process(context);
        reverb.process(context);
    }

    void reset() {
        convolution.reset();
    }

    void updateParameters() {
        auto *cabinetTypeParameter = dynamic_cast<ChoiceParameter *> (parameters[0]);

        if (cabinetTypeParameter == nullptr) {
            jassertfalse;
            return;
        }

        if (cabinetTypeParameter->getCurrentSelectedID() == 1) {
            bypass = true;
        } else {
            bypass = false;

            auto selectedType = cabinetTypeParameter->getCurrentSelectedID();
            auto assetName = "";
            switch (selectedType) {
                case 2:
                    assetName = "front.wav";
                    break;
                case 3:
                    assetName = "back.wav";
                    break;
                case 4:
                    assetName = "left.wav";
                    break;
                case 5:
                    assetName = "right.wav";
                    break;
                case 6:
                    assetName = "up.wav";
                    break;
                case 7:
                    assetName = "down.wav";
                    break;
                case 8:
                    assetName = "left50up60.wav";
                    break;
                case 9:
                    assetName = "right50up60.wav";
                    break;
                case 10:
                    assetName = "left140down30.wav";
                    break;
                case 11:
                    assetName = "right132down30.wav";
                    break;
                case 12:
                    assetName = "left220.wav";
                    break;
                case 13:
                    assetName = "right140.wav";
                    break;
            }

            auto assetInputStream = createAssetInputStream(assetName);

            if (assetInputStream == nullptr) {
                jassertfalse;
                return;
            }

            AudioFormatManager manager;
            manager.registerBasicFormats();
            std::unique_ptr<AudioFormatReader> reader{manager.createReaderFor(std::move(assetInputStream))};

            if (reader == nullptr) {
                jassertfalse;
                return;
            }

            AudioBuffer<float> buffer(static_cast<int> (reader->numChannels),
                                      static_cast<int> (reader->lengthInSamples));
            reader->read(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), 0, buffer.getNumSamples());

            bufferTransfer.set(BufferWithSampleRate{std::move(buffer), reader->sampleRate});
        }
    }

    //==============================================================================
    struct BufferWithSampleRate {
        BufferWithSampleRate() = default;

        BufferWithSampleRate(AudioBuffer<float> &&bufferIn, double sampleRateIn)
                : buffer(std::move(bufferIn)), sampleRate(sampleRateIn) {}

        AudioBuffer<float> buffer;
        double sampleRate = 0.0;
    };

    class BufferTransfer {
    public:
        void set(BufferWithSampleRate &&p) {
            const SpinLock::ScopedLockType lock(mutex);
            buffer = std::move(p);
            newBuffer = true;
        }

        // Call `fn` passing the new buffer, if there's one available
        template<typename Fn>
        void get(Fn &&fn) {
            const SpinLock::ScopedTryLockType lock(mutex);

            if (lock.isLocked() && newBuffer) {
                fn(buffer);
                newBuffer = false;
            }
        }

    private:
        BufferWithSampleRate buffer;
        bool newBuffer = false;
        SpinLock mutex;
    };

    double sampleRate = 0.0;
    bool bypass = false;

    MemoryBlock currentCabinetData;
    Convolution convolution;
    Convolution convolutionHP;

    juce::dsp::Reverb reverb;
    juce::Reverb::Parameters params;

    BufferTransfer bufferTransfer;

    ChoiceParameter cabinetParam{{"Bypass", "Front", "Back", "Left", "Right", "Up", "Down", "Left-Up", "Right-Up", "Left-Back-Down", "Right-Back-Down", "Left-Back", "Right-Back",}, 1, "Position"};

    std::vector<DSPDemoParameterBase *> parameters{&cabinetParam};
};

struct ConvolutionDemo : public Component {
    ConvolutionDemo() {
        addAndMakeVisible(fileReaderComponent);
        setSize(750, 500);
    }

    void resized() override {
        fileReaderComponent.setBounds(getLocalBounds());
    }

    AudioFileReaderComponent<ConvolutionDemoDSP> fileReaderComponent;
};
