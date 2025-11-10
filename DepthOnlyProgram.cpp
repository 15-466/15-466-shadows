#include "DepthOnlyProgram.hpp"

#include "gl_compile_program.hpp"

DepthOnlyProgram::DepthOnlyProgram() {
	program = gl_compile_program(
		"#version 330\n"
		"uniform mat4 CLIP_FROM_OBJECT;\n"
		"layout(location=0) in vec4 Position;\n" //note: layout keyword used to make sure that the location-0 attribute is always bound to something
		"in vec3 Normal;\n" //DEBUG
		"out vec3 color;\n" //DEBUG
		"void main() {\n"
		"	gl_Position = CLIP_FROM_OBJECT * Position;\n"
		"	color = 0.5 + 0.5 * Normal;\n" //DEBUG
		"}\n"
		,
		"#version 330\n"
		"in vec3 color;\n" //DEBUG
		"out vec4 fragColor;\n"
		"void main() {\n"
		"	fragColor = vec4(color, 1.0);\n"
		"}\n"
	);

	CLIP_FROM_OBJECT_mat4 = glGetUniformLocation(program, "CLIP_FROM_OBJECT");
}

Load< DepthOnlyProgram > depth_only_program(LoadTagEarly, []() -> DepthOnlyProgram const * {
	DepthOnlyProgram *ret = new DepthOnlyProgram();

	//set up template pipeline:
	depth_only_program_pipeline.program = ret->program;
	depth_only_program_pipeline.CLIP_FROM_OBJECT_mat4 = ret->CLIP_FROM_OBJECT_mat4;

	return ret;
});

Scene::Drawable::Pipeline depth_only_program_pipeline;
