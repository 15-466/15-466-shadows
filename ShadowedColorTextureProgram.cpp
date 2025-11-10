#include "ShadowedColorTextureProgram.hpp"

#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

ShadowedColorTextureProgram::ShadowedColorTextureProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"uniform mat4x3 LIGHT_FROM_OBJECT;\n"
		"uniform mat3 LIGHT_FROM_NORMAL;\n"
		"uniform mat4 SPOT_FROM_LIGHT;\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 Normal;\n"
		"in vec4 Color;\n"
		"in vec2 TexCoord;\n"
		"out vec3 position;\n"
		"out vec3 normal;\n"
		"out vec4 color;\n"
		"out vec2 texCoord;\n"
		"out vec4 spotPosition;\n"
		"void main() {\n"
		"	gl_Position = CLIP_FROM_OBJECT * Position;\n"
		"	position = LIGHT_FROM_OBJECT * Position;\n"
		"	spotPosition = SPOT_FROM_LIGHT * vec4(position, 1.0);\n"
		"	normal = LIGHT_FROM_NORMAL * Normal;\n"
		"	color = Color;\n"
		"	texCoord = TexCoord;\n"
		"}\n"
		,
		"#version 330\n"
		"uniform vec3 sun_direction;\n"
		"uniform vec3 sun_color;\n"
		"uniform vec3 sky_direction;\n"
		"uniform vec3 sky_color;\n"
		"uniform vec3 spot_position;\n"
		"uniform vec3 spot_direction;\n"
		"uniform vec3 spot_color;\n"
		"uniform vec2 spot_outer_inner;\n"
		"uniform sampler2D tex;\n"
		"uniform sampler2DShadow spot_depth_tex;\n"
		"in vec3 position;\n"
		"in vec3 normal;\n"
		"in vec4 color;\n"
		"in vec2 texCoord;\n"
		"in vec4 spotPosition;\n"
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	vec3 total_light = vec3(0.0, 0.0, 0.0);\n"
		"	vec3 n = normalize(normal);\n"
		"	{ //sky (hemisphere) light:\n"
		"		vec3 l = sky_direction;\n"
		"		float nl = 0.5 + 0.5 * dot(n,l);\n"
		"		total_light += nl * sky_color;\n"
		"	}\n"
		"	{ //sun (directional) light:\n"
		"		vec3 l = sun_direction;\n"
		"		float nl = max(0.0, dot(n,l));\n"
		"		total_light += nl * sun_color;\n"
		"	}\n"
		"	{ //spot (point with fov + shadow map) light:\n"
		"		vec3 l = normalize(spot_position - position);\n"
		"		float nl = max(0.0, dot(n,l));\n"
		"		//TODO: look up shadow map\n"
		"		float d = dot(l,-spot_direction);\n"
		"		float amt = smoothstep(spot_outer_inner.x, spot_outer_inner.y, d);\n"
		"		float shadow = textureProj(spot_depth_tex, spotPosition);\n"
		"		total_light += shadow * nl * amt * spot_color;\n"
		//"		fragColor = vec4(s,s,s, 1.0);\n" //DEBUG: just show shadow
		"	}\n"

		"	fragColor = texture(tex, texCoord) * vec4(color.rgb * total_light, color.a);\n"
		"}\n"
	);

	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
	LIGHT_FROM_OBJECT_mat4x3 = glGetUniformLocation(program, "LIGHT_FROM_OBJECT");
	LIGHT_FROM_NORMAL_mat3 = glGetUniformLocation(program, "LIGHT_FROM_NORMAL");

	sun_direction_vec3 = glGetUniformLocation(program, "sun_direction");
	sun_color_vec3 = glGetUniformLocation(program, "sun_color");
	sky_direction_vec3 = glGetUniformLocation(program, "sky_direction");
	sky_color_vec3 = glGetUniformLocation(program, "sky_color");

	spot_position_vec3 = glGetUniformLocation(program, "spot_position");
	spot_direction_vec3 = glGetUniformLocation(program, "spot_direction");
	spot_color_vec3 = glGetUniformLocation(program, "spot_color");
	spot_outer_inner_vec2 = glGetUniformLocation(program, "spot_outer_inner");

	SPOT_FROM_LIGHT_mat4 = glGetUniformLocation(program, "SPOT_FROM_LIGHT");

	glUseProgram(program);

	GLuint tex_sampler2D = glGetUniformLocation(program, "tex");
	glUniform1i(tex_sampler2D, 0);

	GLuint spot_depth_tex_sampler2D = glGetUniformLocation(program, "spot_depth_tex");
	glUniform1i(spot_depth_tex_sampler2D, 1);

	glUseProgram(0);

	GL_ERRORS();
}

Load< ShadowedColorTextureProgram > shadowed_color_texture_program(LoadTagEarly, []() -> ShadowedColorTextureProgram const * {
	ShadowedColorTextureProgram *ret = new ShadowedColorTextureProgram();

	shadowed_color_texture_program_pipeline.program = ret->program;

	shadowed_color_texture_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;
	shadowed_color_texture_program_pipeline.LIGHT_FROM_OBJECT_mat4x3 = ret->LIGHT_FROM_OBJECT_mat4x3;
	shadowed_color_texture_program_pipeline.LIGHT_FROM_NORMAL_mat3 = ret->LIGHT_FROM_NORMAL_mat3;

	/* This will be used later if/when we build a light loop into the Scene:
	shadowed_color_texture_program_pipeline.LIGHT_TYPE_int = ret->LIGHT_TYPE_int;
	shadowed_color_texture_program_pipeline.LIGHT_LOCATION_vec3 = ret->LIGHT_LOCATION_vec3;
	shadowed_color_texture_program_pipeline.LIGHT_DIRECTION_vec3 = ret->LIGHT_DIRECTION_vec3;
	shadowed_color_texture_program_pipeline.LIGHT_ENERGY_vec3 = ret->LIGHT_ENERGY_vec3;
	shadowed_color_texture_program_pipeline.LIGHT_CUTOFF_float = ret->LIGHT_CUTOFF_float;
	*/

	//make a 1-pixel white texture to bind by default:
	GLuint tex;
	glGenTextures(1, &tex);

	glBindTexture(GL_TEXTURE_2D, tex);
	std::vector< glm::u8vec4 > tex_data(1, glm::u8vec4(0xff));
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);


	shadowed_color_texture_program_pipeline.textures[0].texture = tex;
	shadowed_color_texture_program_pipeline.textures[0].target = GL_TEXTURE_2D;

	return ret;
});

Scene::Drawable::Pipeline shadowed_color_texture_program_pipeline;
