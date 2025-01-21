#include <vector>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cstring>

// Emscripten Embind
#include <emscripten/bind.h>

// LibRaw includes
#include "libraw/libraw.h"

using namespace emscripten;

class WASMLibRaw {
public:
    WASMLibRaw() {
        processor_ = new LibRaw();
    }

    ~WASMLibRaw() {
        if (processor_) {
            cleanupParamsStrings();
            delete processor_;
            processor_ = nullptr;
        }
    }

    void open(val jsBuffer, val settings) {
        if (!processor_) {
            throw std::runtime_error("LibRaw not initialized");
        }

		// 1) Convert the JS buffer (Uint8Array) to a C++ std::vector<uint8_t>.
        std::vector<uint8_t> buffer = toNativeVector(jsBuffer);

        applySettings(settings);

        int ret = processor_->open_buffer((void*)buffer.data(), buffer.size());
        if (ret != LIBRAW_SUCCESS) {
            throw std::runtime_error("LibRaw: open_buffer() failed with code " + std::to_string(ret));
        }

        ret = processor_->unpack();
        if (ret != LIBRAW_SUCCESS) {
            throw std::runtime_error("LibRaw: unpack() failed with code " + std::to_string(ret));
        }

        ret = processor_->dcraw_process();
        if (ret != LIBRAW_SUCCESS) {
            throw std::runtime_error("LibRaw: dcraw_process() failed with code " + std::to_string(ret));
        }
    }

    val metadata() {
        if (!processor_) {
            return val::undefined();
        }

        val meta = val::object();

        // Basic sizes
        meta.set("width",       processor_->imgdata.sizes.width);
        meta.set("height",      processor_->imgdata.sizes.height);
        meta.set("raw_width",   processor_->imgdata.sizes.raw_width);
        meta.set("raw_height",  processor_->imgdata.sizes.raw_height);
        meta.set("top_margin",  processor_->imgdata.sizes.top_margin);
        meta.set("left_margin", processor_->imgdata.sizes.left_margin);

        // Camera info
        meta.set("camera_make",  std::string(processor_->imgdata.idata.make));
        meta.set("camera_model", std::string(processor_->imgdata.idata.model));

        // EXIF-like data
        meta.set("iso_speed",  processor_->imgdata.other.iso_speed);
        meta.set("shutter",    processor_->imgdata.other.shutter);
        meta.set("aperture",   processor_->imgdata.other.aperture);
        meta.set("focal_len",  processor_->imgdata.other.focal_len);
        meta.set("timestamp",  double(processor_->imgdata.other.timestamp));
        meta.set("shot_order", processor_->imgdata.other.shot_order);
        meta.set("desc",       std::string(processor_->imgdata.other.desc));
        meta.set("artist",     std::string(processor_->imgdata.other.artist));

        // Thumbnail info
        meta.set("thumb_width",  processor_->imgdata.thumbnail.twidth);
        meta.set("thumb_height", processor_->imgdata.thumbnail.theight);
        meta.set("thumb_format", static_cast<int>(processor_->imgdata.thumbnail.tformat));

        return meta;
    }

    val imageData() {
        if (!processor_) {
            return val::undefined();
        }

        libraw_processed_image_t* out = processor_->dcraw_make_mem_image();
        // If out is NULL, we can't proceed
        if (!out) {
            return val::undefined();
        }

        // If out->data is actually declared as a pointer, it should be fine;
        // if it's an array, the compiler might complain about the pointer check.
        // We'll omit the check to avoid the warning:
        // if (!out->data) { ... }  // can be removed if out->data is guaranteed valid

        val resultData = val::undefined();

        size_t pixelCount = static_cast<size_t>(out->height) *
                            static_cast<size_t>(out->width)  *
                            static_cast<size_t>(out->colors);

        if (out->bits == 8) {
            val view = val(typed_memory_view(pixelCount, (uint8_t*)out->data));
            resultData = view;
        }
        else if (out->bits == 16) {
            val view = val(typed_memory_view(pixelCount, (uint16_t*)out->data));
            resultData = view;
        }
        else {
            processor_->dcraw_clear_mem(out);
            throw std::runtime_error("Unsupported bit depth");
        }

        processor_->dcraw_clear_mem(out);
        return resultData;
    }

private:
    LibRaw* processor_ = nullptr;

    void applySettings(const val& settings) {
        // If 'settings' is null or undefined, just skip
        if (settings.isNull() || settings.isUndefined()) {
            return;
        }

        // If you want more robust type checking, you can do:
        //   if (settings.typeOf().as<std::string>() != "object") { return; }
        // ... or just proceed.

        libraw_output_params_t &params = processor_->imgdata.params;

        // -- ARRAYS --
        if (settings.hasOwnProperty("greybox")) {
            val arr = settings["greybox"];
            if (arr["length"].as<unsigned>() == 4) {
                for (int i = 0; i < 4; i++) {
                    params.greybox[i] = arr[i].as<unsigned>();
                }
            }
        }
        if (settings.hasOwnProperty("cropbox")) {
            val arr = settings["cropbox"];
            if (arr["length"].as<unsigned>() == 4) {
                for (int i = 0; i < 4; i++) {
                    params.cropbox[i] = arr[i].as<unsigned>();
                }
            }
        }
        if (settings.hasOwnProperty("aber")) {
            val arr = settings["aber"];
            if (arr["length"].as<unsigned>() == 4) {
                for (int i = 0; i < 4; i++) {
                    params.aber[i] = arr[i].as<double>();
                }
            }
        }
        if (settings.hasOwnProperty("gamm")) {
            val arr = settings["gamm"];
            if (arr["length"].as<unsigned>() == 6) {
                for (int i = 0; i < 6; i++) {
                    params.gamm[i] = arr[i].as<double>();
                }
            }
        }
        if (settings.hasOwnProperty("user_mul")) {
            val arr = settings["user_mul"];
            if (arr["length"].as<unsigned>() == 4) {
                for (int i = 0; i < 4; i++) {
                    params.user_mul[i] = arr[i].as<float>();
                }
            }
        }

        // -- FLOATS --
        if (settings.hasOwnProperty("bright")) {
            params.bright = settings["bright"].as<float>();
        }
        if (settings.hasOwnProperty("threshold")) {
            params.threshold = settings["threshold"].as<float>();
        }
        if (settings.hasOwnProperty("auto_bright_thr")) {
            params.auto_bright_thr = settings["auto_bright_thr"].as<float>();
        }
        if (settings.hasOwnProperty("adjust_maximum_thr")) {
            params.adjust_maximum_thr = settings["adjust_maximum_thr"].as<float>();
        }
        if (settings.hasOwnProperty("exp_shift")) {
            params.exp_shift = settings["exp_shift"].as<float>();
        }
        if (settings.hasOwnProperty("exp_preser")) {
            params.exp_preser = settings["exp_preser"].as<float>();
        }

        // -- INTEGERS --
        if (settings.hasOwnProperty("half_size")) {
            params.half_size = settings["half_size"].as<int>();
        }
        if (settings.hasOwnProperty("four_color_rgb")) {
            params.four_color_rgb = settings["four_color_rgb"].as<int>();
        }
        if (settings.hasOwnProperty("highlight")) {
            params.highlight = settings["highlight"].as<int>();
        }
        if (settings.hasOwnProperty("use_auto_wb")) {
            params.use_auto_wb = settings["use_auto_wb"].as<int>();
        }
        if (settings.hasOwnProperty("use_camera_wb")) {
            params.use_camera_wb = settings["use_camera_wb"].as<int>();
        }
        if (settings.hasOwnProperty("use_camera_matrix")) {
            params.use_camera_matrix = settings["use_camera_matrix"].as<int>();
        }
        if (settings.hasOwnProperty("output_color")) {
            params.output_color = settings["output_color"].as<int>();
        }
        if (settings.hasOwnProperty("output_bps")) {
            params.output_bps = settings["output_bps"].as<int>();
        }
        if (settings.hasOwnProperty("output_tiff")) {
            params.output_tiff = settings["output_tiff"].as<int>();
        }
        if (settings.hasOwnProperty("output_flags")) {
            params.output_flags = settings["output_flags"].as<int>();
        }
        if (settings.hasOwnProperty("user_flip")) {
            params.user_flip = settings["user_flip"].as<int>();
        }
        if (settings.hasOwnProperty("user_qual")) {
            params.user_qual = settings["user_qual"].as<int>();
        }
        if (settings.hasOwnProperty("user_black")) {
            params.user_black = settings["user_black"].as<int>();
        }
        if (settings.hasOwnProperty("user_cblack")) {
            val arr = settings["user_cblack"];
            if (arr["length"].as<unsigned>() == 4) {
                for (int i = 0; i < 4; i++) {
                    params.user_cblack[i] = arr[i].as<int>();
                }
            }
        }
        if (settings.hasOwnProperty("user_sat")) {
            params.user_sat = settings["user_sat"].as<int>();
        }
        if (settings.hasOwnProperty("med_passes")) {
            params.med_passes = settings["med_passes"].as<int>();
        }
        if (settings.hasOwnProperty("no_auto_bright")) {
            params.no_auto_bright = settings["no_auto_bright"].as<int>();
        }
        if (settings.hasOwnProperty("use_fuji_rotate")) {
            params.use_fuji_rotate = settings["use_fuji_rotate"].as<int>();
        }
        if (settings.hasOwnProperty("green_matching")) {
            params.green_matching = settings["green_matching"].as<int>();
        }
        if (settings.hasOwnProperty("dcb_iterations")) {
            params.dcb_iterations = settings["dcb_iterations"].as<int>();
        }
        if (settings.hasOwnProperty("dcb_enhance_fl")) {
            params.dcb_enhance_fl = settings["dcb_enhance_fl"].as<int>();
        }
        if (settings.hasOwnProperty("fbdd_noiserd")) {
            params.fbdd_noiserd = settings["fbdd_noiserd"].as<int>();
        }
        if (settings.hasOwnProperty("exp_correc")) {
            params.exp_correc = settings["exp_correc"].as<int>();
        }
        if (settings.hasOwnProperty("no_auto_scale")) {
            params.no_auto_scale = settings["no_auto_scale"].as<int>();
        }
        if (settings.hasOwnProperty("no_interpolation")) {
            params.no_interpolation = settings["no_interpolation"].as<int>();
        }

        // -- STRINGS (C-strings) --
        if (settings.hasOwnProperty("output_profile")) {
            setStringMember(params.output_profile, settings["output_profile"].as<std::string>());
        }
        if (settings.hasOwnProperty("camera_profile")) {
            setStringMember(params.camera_profile, settings["camera_profile"].as<std::string>());
        }
        if (settings.hasOwnProperty("bad_pixels")) {
            setStringMember(params.bad_pixels, settings["bad_pixels"].as<std::string>());
        }
        if (settings.hasOwnProperty("dark_frame")) {
            setStringMember(params.dark_frame, settings["dark_frame"].as<std::string>());
        }
    }
	// Convert a JS Uint8Array to a std::vector<uint8_t>
    std::vector<uint8_t> toNativeVector(const val &jsBuffer) {
        // Check for null/undefined
        if (jsBuffer.isNull() || jsBuffer.isUndefined()) {
            return {};
        }
        // Expecting a Uint8Array or something with a "length" property
        size_t length = jsBuffer["length"].as<size_t>();
        std::vector<uint8_t> buf(length);
        for (size_t i = 0; i < length; i++) {
            buf[i] = jsBuffer[i].as<uint8_t>();
        }
        return buf;
    }
    void setStringMember(char*& dest, const std::string& value) {
        if (dest) {
            delete[] dest;
            dest = nullptr;
        }
        if (!value.empty()) {
            dest = new char[value.size() + 1];
            std::strcpy(dest, value.c_str());
        }
    }

    void cleanupParamsStrings() {
        libraw_output_params_t &params = processor_->imgdata.params;

        if (params.output_profile) {
            delete[] params.output_profile;
            params.output_profile = nullptr;
        }
        if (params.camera_profile) {
            delete[] params.camera_profile;
            params.camera_profile = nullptr;
        }
        if (params.bad_pixels) {
            delete[] params.bad_pixels;
            params.bad_pixels = nullptr;
        }
        if (params.dark_frame) {
            delete[] params.dark_frame;
            params.dark_frame = nullptr;
        }
    }
};

EMSCRIPTEN_BINDINGS(libraw_module) {
	register_vector<uint8_t>("VectorUint8");
    class_<WASMLibRaw>("LibRaw")
        .constructor<>()
        .function("open", &WASMLibRaw::open)
        .function("metadata", &WASMLibRaw::metadata)
        .function("imageData", &WASMLibRaw::imageData);
}
