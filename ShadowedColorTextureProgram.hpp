#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

//ShadowedColorTextureProgram draws a surface lit by a distant directional light, a hemispherical light, and a spotlight.
// The color is the vertex color multiplied by the color from texture unit 0.
// Spotlight shadowing is computed with a shadow map bound to texture unit 1.
struct ShadowedColorTextureProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	GLuint CLIP_FROM_OBJECT_mat4 = -1U;
	GLuint LIGHT_FROM_OBJECT_mat4x3 = -1U;
	GLuint LIGHT_FROM_NORMAL_mat3 = -1U;

	GLuint sun_direction_vec3 = -1U; //direction *to* sun
	GLuint sun_color_vec3 = -1U;
	GLuint sky_direction_vec3 = -1U; //direction *to* sky
	GLuint sky_color_vec3 = -1U;

	GLuint spot_position_vec3 = -1U;
	GLuint spot_direction_vec3 = -1U; //direction *from* spotlight
	GLuint spot_color_vec3 = -1U;
	GLuint spot_outer_inner_vec2 = -1U; //color fades from zero to one as dot(spot_direction, spot_to_position) varies from outer_inner.x to outer_inner.y
	GLuint SPOT_FROM_LIGHT_mat4 = -1U; //projects from lighting space (/world space) to spot light depth map space

	//textures:
	//texture0 - texture for the surface
	//texture1 - texture for spot light shadow map

	ShadowedColorTextureProgram();
};

extern Load< ShadowedColorTextureProgram > shadowed_color_texture_program;

extern Scene::Drawable::Pipeline shadowed_color_texture_program_pipeline;
