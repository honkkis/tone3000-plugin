// Standalone real-model renderer: deliberately refuses to pass on an x86 fallback.
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
#include "NAM/dsp.h"
#include "NAM/wavenet/a2_fast.h"
#ifndef ORIGINAL_CORE
#include "NAM/wavenet/a2_planar.h"
#ifndef NAM_A2_PLANAR
#error "This test requires the actual ARM NEON planar implementation."
#endif
#endif

using namespace nam::wavenet::a2_fast;

void save(const std::filesystem::path& path, const std::vector<NAM_SAMPLE>& data) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(NAM_SAMPLE));
    if (!f) throw std::runtime_error("Cannot write " + path.string());
}

int main(int argc, char** argv) {
    try {
        if (argc != 5) throw std::runtime_error("usage: render model.nam input.f64 output-dir schedules(comma-separated,0=mixed)");
        static_assert(sizeof(NAM_SAMPLE) == 8, "Match plugin double I/O, float32 internal arithmetic");
        nlohmann::json root;
        std::ifstream model(argv[1]); model >> root;
        std::vector<nlohmann::json> models;
        if (root.at("architecture") == "SlimmableContainer")
            for (const auto& sub : root.at("config").at("submodels")) models.push_back(sub.at("model"));
        else models.push_back(root);
        std::ifstream audio(argv[2], std::ios::binary | std::ios::ate);
        const auto bytes = audio.tellg();
        if (bytes <= 0 || bytes % 8) throw std::runtime_error("Invalid f64 input");
        std::vector<NAM_SAMPLE> input(static_cast<size_t>(bytes) / 8);
        audio.seekg(0); audio.read(reinterpret_cast<char*>(input.data()), bytes);
        if (!audio) throw std::runtime_error("Cannot read input");
        std::vector<int> schedules;
        std::stringstream ss(argv[4]); std::string token;
        while (std::getline(ss, token, ',')) {
            const int n = std::stoi(token);
            if (n < 0 || n > 4096) throw std::runtime_error("Invalid block size");
            schedules.push_back(n);
        }
        std::filesystem::create_directories(argv[3]);
        bool equal = true;
        for (size_t sub = 0; sub < models.size(); ++sub) {
            const auto& m = models[sub]; int channels = 0;
            if (!is_a2_shape(m.at("config"), &channels)) throw std::runtime_error("Non-A2 model: refusing to skip");
            const auto weights = m.at("weights").get<std::vector<float>>();
            const double rate = m.value("sample_rate", 48000.0);
            for (int block : schedules) {
                const int maximum = block ? block : 512;
                for (int state = 0; state < 3; ++state) {
#ifdef ORIGINAL_CORE
                    auto reference = create_a2_fast_config(m.at("config"), rate)->create(weights, rate);
#else
                    auto reference = create_a2_fast_reference_model(channels, weights, rate);
                    auto planar = create_a2_planar_model(channels, weights, rate);
                    if (!planar) throw std::runtime_error("No planar kernel: refusing to skip");
#endif
                    std::vector<nam::DSP*> engines{reference.get()};
#ifndef ORIGINAL_CORE
                    engines.push_back(planar.get());
#endif
                    std::vector<std::vector<NAM_SAMPLE>> outputs;
                    for (auto* dsp : engines) {
                        dsp->SetPrewarmOnReset(state != 0);
                        dsp->Reset(rate, maximum);
                        if (state == 2) {
                            // Dirty the state, then exercise the cached prewarm/reset path.
                            std::vector<NAM_SAMPLE> dirty(maximum, 0.13), discard(maximum);
                            NAM_SAMPLE* in[] = {dirty.data()}; NAM_SAMPLE* out[] = {discard.data()};
                            dsp->process(in, out, maximum);
                            dsp->Reset(rate, maximum);
                        }
                        outputs.emplace_back(input.size());
                        const int mixed[] = {1, 7, 8, 15, 31, 32, 33, 63, 64, 127, 128, 255, 256, 511, 512};
                        size_t pos = 0, step = 0;
                        while (pos < input.size()) {
                            const int n = std::min<size_t>(block ? block : mixed[step++ % 15], input.size() - pos);
                            NAM_SAMPLE* in[] = {input.data() + pos}; NAM_SAMPLE* out[] = {outputs.back().data() + pos};
                            dsp->process(in, out, n); pos += n;
                        }
                    }
                    const std::string key = "sub" + std::to_string(sub) + "_c" + std::to_string(channels)
                        + "_block" + std::to_string(block) + "_state" + std::to_string(state);
                    save(std::filesystem::path(argv[3]) / (key + "_reference.f64"), outputs[0]);
#ifndef ORIGINAL_CORE
                    save(std::filesystem::path(argv[3]) / (key + "_planar.f64"), outputs[1]);
                    const bool same = std::memcmp(outputs[0].data(), outputs[1].data(), input.size() * 8) == 0;
                    equal &= same;
                    std::cout << key << ": " << (same ? "BIT IDENTICAL" : "DIFFERENT") << std::endl;
#endif
                }
            }
        }
        return equal ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
}
