/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@alienslab.net>.
 */

#include "taisei.h"

#include "shaderlib.h"
#include "lang_spirv_private.h"

#include "util.h"

#include <shaderc/shaderc.h>
#include <crossc.h>

static shaderc_compiler_t spirv_compiler;

static inline shaderc_optimization_level resolve_opt_level(SPIRVOptimizationLevel lvl) {
	switch(lvl) {
		case SPIRV_OPTIMIZE_NONE:        return shaderc_optimization_level_zero;
		case SPIRV_OPTIMIZE_SIZE:        return shaderc_optimization_level_size;
		case SPIRV_OPTIMIZE_PERFORMANCE: return shaderc_optimization_level_performance;
		default: UNREACHABLE;
	}
}

static inline shaderc_profile resolve_glsl_profile(const GLSLVersion *v) {
	switch(v->profile) {
		case GLSL_PROFILE_COMPATIBILITY: return shaderc_profile_compatibility;
		case GLSL_PROFILE_CORE:          return shaderc_profile_core;
		case GLSL_PROFILE_ES:            return shaderc_profile_es;
		case GLSL_PROFILE_NONE:          return shaderc_profile_none;
		default: UNREACHABLE;
	}
}

static inline shaderc_target_env resolve_env(SPIRVTarget target, uint32_t *out_version) {
	switch(target) {
		case SPIRV_TARGET_OPENGL_450:
			*out_version = shaderc_env_version_opengl_4_5;
			return shaderc_target_env_opengl;

		case SPIRV_TARGET_VULKAN_10:
			*out_version = shaderc_env_version_vulkan_1_0;
			return shaderc_target_env_vulkan;

		case SPIRV_TARGET_VULKAN_11:
			*out_version = shaderc_env_version_vulkan_1_1;
			return shaderc_target_env_vulkan;

		default: UNREACHABLE;
	}
}

static inline shaderc_shader_kind resolve_kind(ShaderStage stage) {
	switch(stage) {
		case SHADER_STAGE_FRAGMENT: return shaderc_fragment_shader;
		case SHADER_STAGE_VERTEX:   return shaderc_vertex_shader;
		default: UNREACHABLE;
	}
}

void spirv_init_compiler(void) {
	if(spirv_compiler == NULL) {
		spirv_compiler = shaderc_compiler_initialize();
		if(spirv_compiler == NULL) {
			log_error("Failed to initialize the compiler");
		}
	}
}

void spirv_shutdown_compiler(void) {
	if(spirv_compiler != NULL) {
		shaderc_compiler_release(spirv_compiler);
		spirv_compiler = NULL;
	}
}

bool _spirv_compile(const ShaderSource *in, ShaderSource *out, const SPIRVCompileOptions *options) {
	if(in->lang.lang != SHLANG_GLSL) {
		log_error("Unsupported source language");
		return false;
	}

	if(spirv_compiler == NULL) {
		log_error("Compiler is not initialized");
		return false;
	}

	shaderc_compile_options_t opts = shaderc_compile_options_initialize();

	if(opts == NULL) {
		log_error("Failed to initialize compiler options");
		return false;
	}

	shaderc_compile_options_set_source_language(opts, shaderc_source_language_glsl);
	shaderc_compile_options_set_optimization_level(opts, resolve_opt_level(options->optimization_level));
	shaderc_compile_options_set_forced_version_profile(opts, in->lang.glsl.version.version, resolve_glsl_profile(&in->lang.glsl.version));
	shaderc_compile_options_set_auto_map_locations(opts, true);

	uint32_t env_version;
	shaderc_target_env env = resolve_env(options->target, &env_version);
	shaderc_compile_options_set_target_env(opts, env, env_version);

	if(options->debug_info) {
		shaderc_compile_options_set_generate_debug_info(opts);
	}

	const char *filename = options->filename ? options->filename : "<main>";

	shaderc_compilation_result_t result = shaderc_compile_into_spv(
		spirv_compiler,
		in->content,
		in->content_size - 1,
		resolve_kind(in->stage),
		filename,
		"main",
		opts
	);

	shaderc_compile_options_release(opts);

	if(result == NULL) {
		log_error("Failed to allocate compilation result");
		return false;
	}

	shaderc_compilation_status status = shaderc_result_get_compilation_status(result);

	switch(status) {
		case shaderc_compilation_status_compilation_error:
			log_error("Compilation failed: errors in the shader");
			break;

		case shaderc_compilation_status_internal_error:
			log_error("Compilation failed: internal compiler error");
			break;

		case shaderc_compilation_status_invalid_assembly:
			log_error("Compilation failed: invalid assembly");
			break;

		case shaderc_compilation_status_invalid_stage:
			log_error("Compilation failed: invalid shader stage");
			break;

		case shaderc_compilation_status_null_result_object:
			UNREACHABLE;

		case shaderc_compilation_status_success:
			break;

		default:
			log_error("Compilation failed: unknown error");
			break;
	}

	const char *err_str = shaderc_result_get_error_message(result);

	if(err_str != NULL && *err_str) {
		size_t num_warnings = shaderc_result_get_num_warnings(result);
		size_t num_errors = shaderc_result_get_num_errors(result);
		LogLevel lvl = status == shaderc_compilation_status_success ? LOG_WARN : LOG_ERROR;
		log_custom(lvl, "%s: %zu warnings, %zu errors:\n\n%s", filename, num_warnings, num_errors, err_str);
	}

	if(status != shaderc_compilation_status_success) {
		shaderc_result_release(result);
		return false;
	}

	size_t data_len = shaderc_result_get_length(result);
	const char *data = shaderc_result_get_bytes(result);

	out->stage = in->stage;
	out->lang.lang = SHLANG_SPIRV;
	out->lang.spirv.target = options->target;
	out->content_size = data_len + 1;
	out->content = calloc(1, out->content_size);
	memcpy(out->content, data, data_len);
	shaderc_result_release(result);

	return true;
}

bool _spirv_decompile(const ShaderSource *in, ShaderSource *out, const SPIRVDecompileOptions *options) {
	if(in->lang.lang != SHLANG_SPIRV) {
		log_error("Source is not a SPIR-V binary");
		return false;
	}

	if(options->lang->lang != SHLANG_GLSL) {
		log_error("Target language is not supported");
		return false;
	}

	size_t spirv_size = (in->content_size - 1) / sizeof(uint32_t);
	const uint32_t *spirv = (uint32_t*)(void*)in->content;

	crossc_compiler *cc = crossc_glsl_create(spirv, spirv_size);

	if(cc == NULL) {
		log_error("Failed to initialize crossc");
		return false;
	}

	if(!crossc_has_valid_program(cc)) {
		goto crossc_error;
	}

	crossc_glsl_profile profile = (
		options->lang->glsl.version.profile == GLSL_PROFILE_ES
			? CROSSC_GLSL_PROFILE_ES
			: CROSSC_GLSL_PROFILE_CORE
	);

	crossc_glsl_set_version(cc, options->lang->glsl.version.version, profile);

	const char *code = crossc_compile(cc);

	if(code == NULL) {
		goto crossc_error;
	}

	out->content = strdup(code);
	out->content_size = strlen(code) + 1;
	out->stage = in->stage;
	out->lang = *options->lang;

	crossc_destroy(cc);
	return true;

crossc_error:
	log_error("crossc error: %s", crossc_strerror(cc));
	crossc_destroy(cc);
	return false;
}
