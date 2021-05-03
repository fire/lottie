/*************************************************************************/
/*  resource_importer_lottie.cpp                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "resource_importer_lottie.h"

#include "core/core_bind.h"
#include "core/io/file_access_pack.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include <stdint.h>

#if _MSC_VER && !__INTEL_COMPILER
#pragma warning(push)
#pragma warning(disable : 4251)
#endif
#include "thirdparty/rlottie/inc/rlottie.h"
#include "thirdparty/rlottie/inc/rlottiecommon.h"
#include <climits>

String ResourceImporterLottie::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterLottie::get_import_options(List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "3d"), false));
	Dictionary d = Engine::get_singleton()->get_version_info();
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "start_time", PROPERTY_HINT_RANGE, "0,65536,0.01,or_greater"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "frame_rate", PROPERTY_HINT_RANGE, "1,65536,1,or_greater"), 15));
	r_options->push_back(ImportOption(PropertyInfo(Variant::VECTOR2, "scale"), Vector2(1.0f, 1.0f)));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "animation/import"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "animation/begin_playing"), true));
}

bool ResourceImporterLottie::get_option_visibility(const String &p_option, const Map<StringName, Variant> &p_options) const {
	return true;
}

String ResourceImporterLottie::get_importer_name() const {
	return "lottie_sprite";
}

String ResourceImporterLottie::get_visible_name() const {
	return "Lottie Sprite";
}

void ResourceImporterLottie::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("json");
}

String ResourceImporterLottie::get_save_extension() const {
	return "scn";
}

String ResourceImporterLottie::get_resource_type() const {
	return "PackedScene";
}

int ResourceImporterLottie::get_preset_count() const {
	return 0;
}

Error ResourceImporterLottie::import(const String &p_source_file, const String &p_save_path, const Map<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	FileAccess *file = FileAccess::create(FileAccess::ACCESS_RESOURCES);
	String data;
	//Backport code
	//String data = file->get_file_as_string(p_source_file, &err);
	Vector<uint8_t> array = file->get_file_as_array(p_source_file);
	data.parse_utf8((const char *)array.ptr(), array.size());
	//End backport code
	std::unique_ptr<rlottie::Animation> lottie =
			rlottie::Animation::loadFromData(data.utf8().ptrw(), p_source_file.utf8().ptr());
	ERR_FAIL_COND_V(!lottie, FAILED);
	size_t width = 0;
	size_t height = 0;
	lottie->size(width, height);
	ERR_FAIL_COND_V(!width, FAILED);
	ERR_FAIL_COND_V(!height, FAILED);
	Vector2 scale = p_options["scale"];
	width *= scale.width;
	height *= scale.height;
	ERR_FAIL_COND_V(!width, FAILED);
	ERR_FAIL_COND_V(!height, FAILED);
	Ref<SpriteFrames> frames;
	frames.instance();
	List<StringName> animations;
	frames->get_animation_list(&animations);
	String name = animations[0];
	double_t frame_rate = p_options["frame_rate"];
	frames->set_animation_speed(name, frame_rate);
	Vector<Ref<ImageTexture> > image_textures;
	const double increment = 1.0 / frame_rate;
	double time = p_options["start_time"];
	bool last = false;
	double length = lottie->totalFrame() / lottie->frameRate();
	time = CLAMP(time, 0.0, length);
	while (true) {
		Vector<uint32_t> buffer;
		buffer.resize(width * height);
		rlottie::Surface surface(buffer.ptrw(), width, height, width * 4);
		int64_t original_frame = floor(time * lottie->frameRate());
		lottie->renderSync(original_frame, surface);
		PackedByteArray pixels;
		int32_t buffer_byte_size = buffer.size() * sizeof(uint32_t);
		pixels.resize(buffer_byte_size);
		memcpy(pixels.ptrw(), buffer.ptr(), buffer_byte_size);
		uint8_t *ptr_pixel_write = pixels.ptrw();
		for (int32_t pixel_i = 0; pixel_i < pixels.size(); pixel_i += 4) {
			SWAP(ptr_pixel_write[pixel_i + 2], ptr_pixel_write[pixel_i + 0]);
		}
		Ref<Image> img;
		img.instance();
		img->create((int)width, (int)height, false, Image::FORMAT_RGBA8, pixels);
		Dictionary d = Engine::get_singleton()->get_version_info();
		Ref<ImageTexture> tex;
		tex.instance();
		tex->create_from_image(img);
		frames->add_frame(name, tex);
		image_textures.push_back(tex);
		if (last) {
			break;
		}
		time += increment;
		if (time >= length) {
			last = true;
			time = length;
		}
	}

	Node *root = nullptr;
	if (p_options["3d"] && !p_options["animation/import"]) {
		root = memnew(Sprite3D);
		Sprite3D *sprite = cast_to<Sprite3D>(root);
		int32_t frame = p_options["start_frame"];
		Ref<Texture> tex = frames->get_frame("default", frame);
		ERR_FAIL_COND_V(tex.is_null(), FAILED);
		sprite->set_texture(tex);
		sprite->set_draw_flag(SpriteBase3D::FLAG_SHADED, true);
	} else if (!p_options["3d"] && !p_options["animation/import"]) {
		root = memnew(Sprite2D);
		Sprite2D *sprite = cast_to<Sprite2D>(root);
		int32_t frame = p_options["start_frame"];
		Ref<Texture> tex = frames->get_frame("default", frame);
		ERR_FAIL_COND_V(tex.is_null(), FAILED);
		sprite->set_texture(tex);
	} else if (p_options["3d"] && p_options["animation/import"]) {
		root = memnew(AnimatedSprite3D);
		AnimatedSprite3D *animate_sprite = cast_to<AnimatedSprite3D>(root);
		if (p_options["animation/begin_playing"]) {
			animate_sprite->call("_set_playing", true);
		}
		animate_sprite->set_draw_flag(SpriteBase3D::FLAG_SHADED, true);
		animate_sprite->set_frame(p_options["start_frame"]);
		animate_sprite->set_sprite_frames(frames);
	} else {
		root = memnew(AnimatedSprite2D);
		AnimatedSprite2D *animate_sprite = cast_to<AnimatedSprite2D>(root);
		if (p_options["animation/begin_playing"]) {
			animate_sprite->call("_set_playing", true);
		}
		animate_sprite->set_frame(p_options["start_frame"]);
		animate_sprite->set_sprite_frames(frames);
	}
	Ref<PackedScene> scene;
	scene.instance();
	scene->pack(root);
	String save_path = p_save_path + ".scn";
	r_gen_files->push_back(save_path);
	return ResourceSaver::save(save_path, scene);
}