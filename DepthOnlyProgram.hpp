#include "GL.hpp"
#include "Load.hpp"
#include "Scene.hpp"

struct DepthOnlyProgram {
	//opengl program object:
	GLuint program = 0;

	//uniform locations:
	GLuint CLIP_FROM_OBJECT_mat4 = -1U;

	DepthOnlyProgram();
};

extern Load< DepthOnlyProgram > depth_only_program;

extern Scene::Drawable::Pipeline depth_only_program_pipeline;
