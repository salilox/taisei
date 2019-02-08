/*
 * This software is licensed under the terms of the MIT-License
 * See COPYING for further information.
 * ---
 * Copyright (c) 2011-2019, Lukas Weber <laochailan@web.de>.
 * Copyright (c) 2012-2019, Andrei Alexeyev <akari@alienslab.net>.
 */

#ifndef IGUARD_renderer_common_shaderlib_lang_spirv_private_h
#define IGUARD_renderer_common_shaderlib_lang_spirv_private_h

#include "taisei.h"

#include "lang_spirv.h"

bool _spirv_compile(const ShaderSource *in, ShaderSource *out, const SPIRVCompileOptions *options) attr_nonnull(1, 2, 3);
bool _spirv_decompile(const ShaderSource *in, ShaderSource *out, const SPIRVDecompileOptions *options) attr_nonnull(1, 2, 3);

#endif // IGUARD_renderer_common_shaderlib_lang_spirv_private_h
