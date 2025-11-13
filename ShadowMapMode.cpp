#include "ShadowMapMode.hpp"

#include "Load.hpp"
#include "Mesh.hpp"
#include "Scene.hpp"
#include "data_path.hpp" //helper to get paths relative to executable
#include "gl_errors.hpp" //helper for dumpping OpenGL error messages
#include "gl_check_fb.hpp" //helper for checking currently bound OpenGL framebuffer
#include "gl_compile_program.hpp" //helper to compile opengl shader programs
#include "load_save_png.hpp"
#include "ShadowedColorTextureProgram.hpp"
#include "DepthOnlyProgram.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <fstream>
#include <map>
#include <cstddef>
#include <random>


Load< MeshBuffer > meshes(LoadTagDefault, [](){
	return new MeshBuffer(data_path("vignette.pnct"));
});

Load< GLuint > meshes_for_shadowed_color_texture_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(shadowed_color_texture_program->program));
});

Load< GLuint > meshes_for_depth_only_program(LoadTagDefault, [](){
	return new GLuint(meshes->make_vao_for_program(depth_only_program->program));
});

GLuint load_texture(std::string const &filename) {
	glm::uvec2 size;
	std::vector< glm::u8vec4 > data;
	load_png(filename, &size, &data, LowerLeftOrigin);

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL_ERRORS();

	return tex;
}

Load< GLuint > wood_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/wood.png")));
});

Load< GLuint > marble_tex(LoadTagDefault, [](){
	return new GLuint(load_texture(data_path("textures/marble.png")));
});

Load< GLuint > white_tex(LoadTagDefault, [](){
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glm::u8vec4 white(0xff, 0xff, 0xff, 0xff);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, glm::value_ptr(white));
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	return new GLuint(tex);
});


Scene::Camera *camera = nullptr;
Scene::Transform *spot_parent_transform = nullptr;
Scene::Light *spot = nullptr;

Load< Scene > scene(LoadTagDefault, [](){
	Scene *ret = new Scene;

	//pre-build some program info (material) blocks to assign to each object:
	Scene::Drawable::Pipeline texture_pipeline;
	texture_pipeline = shadowed_color_texture_program_pipeline; //start with the ready-made pipeline from ShadowedColorTextureProgram.cpp
	texture_pipeline.vao = *meshes_for_shadowed_color_texture_program;

	Scene::Drawable::Pipeline depth_pipeline;
	depth_pipeline = depth_only_program_pipeline; //start with the ready-made pipeline from DepthProgram.cpp
	depth_pipeline.vao = *meshes_for_depth_only_program;


	//load transform hierarchy:
	ret->load(data_path("vignette.scene"), [&](Scene &s, Scene::Transform *t, std::string const &m){
		Scene::Drawable &obj = s.drawables.emplace_back(t);

		obj.pipelines[Scene::Drawable::PipelineTypeDefault] = texture_pipeline;
		if (t->name == "Platform") {
			obj.pipelines[Scene::Drawable::PipelineTypeDefault].textures[0] = Scene::Drawable::Pipeline::TextureInfo{ .texture = *wood_tex };
		} else if (t->name == "Pedestal") {
			obj.pipelines[Scene::Drawable::PipelineTypeDefault].textures[0] = Scene::Drawable::Pipeline::TextureInfo{ .texture = *marble_tex };
		} else {
			obj.pipelines[Scene::Drawable::PipelineTypeDefault].textures[0] = Scene::Drawable::Pipeline::TextureInfo{ .texture = *white_tex };
		}

		obj.pipelines[Scene::Drawable::PipelineTypeShadow] = depth_pipeline;

		Mesh const &mesh = meshes->lookup(m);
		obj.pipelines[Scene::Drawable::PipelineTypeDefault].start = mesh.start;
		obj.pipelines[Scene::Drawable::PipelineTypeDefault].count = mesh.count;

		obj.pipelines[Scene::Drawable::PipelineTypeShadow].start = mesh.start;
		obj.pipelines[Scene::Drawable::PipelineTypeShadow].count = mesh.count;
	});

	//look up spot parent transform (for spin interaction):
	for (Scene::Transform &t : ret->transforms) {
		if (t.name == "SpotParent") {
			if (spot_parent_transform) throw std::runtime_error("Multiple 'SpotParent' transforms in scene.");
			spot_parent_transform = &t;
		}

	}
	if (!spot_parent_transform) throw std::runtime_error("No 'SpotParent' transform in scene.");

	//look up the camera:
	for (Scene::Camera &c : ret->cameras) {
		if (c.transform->name == "Camera") {
			if (camera) throw std::runtime_error("Multiple 'Camera' objects in scene.");
			camera = &c;
		}
	}
	if (!camera) throw std::runtime_error("No 'Camera' camera in scene.");

	//look up the spotlight:
	for (Scene::Light &l : ret->lights) {
		if (l.transform->name == "Spot") {
			if (spot) throw std::runtime_error("Multiple 'Spot' objects in scene.");
			if (l.type != Scene::Light::Spot) throw std::runtime_error("Lamp 'Spot' is not a spotlight.");
			spot = &l;
		}
	}
	if (!spot) throw std::runtime_error("No 'Spot' spotlight in scene.");

	return ret;
});

ShadowMapMode::ShadowMapMode() {
}

ShadowMapMode::~ShadowMapMode() {
}

bool ShadowMapMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			//probably not needed (always released on mouse-up):
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		SDL_SetWindowRelativeMouseMode(Mode::window, true);
		return true;
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
		SDL_SetWindowRelativeMouseMode(Mode::window, false);
		return true;
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (evt.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
		if (evt.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) {
			spot_spin += 5.0f * evt.motion.xrel / float(window_size.x);
			return true;
		}

	}

	return false;
}

void ShadowMapMode::update(float elapsed) {
	//update spot light parent rotation based on spin value:
	spot_parent_transform->rotation = glm::angleAxis(spot_spin, glm::vec3(0.0f, 0.0f, 1.0f));


	{ //move camera:
		//combine inputs into a move:
		constexpr float PlayerSpeed = 3.0f;
		glm::vec2 move = glm::vec2(0.0f);
		if (left.pressed && !right.pressed) move.x =-1.0f;
		if (!left.pressed && right.pressed) move.x = 1.0f;
		if (down.pressed && !up.pressed) move.y =-1.0f;
		if (!down.pressed && up.pressed) move.y = 1.0f;

		//make it so that moving diagonally doesn't go faster:
		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;

		glm::mat4x3 frame = camera->transform->make_parent_from_local();
		glm::vec3 frame_right = frame[0];
		//glm::vec3 up = frame[1];
		glm::vec3 frame_forward = -frame[2];

		camera->transform->position += move.x * frame_right + move.y * frame_forward;
	}
	
	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;

}

//ShadowMapMode will render to some offscreen framebuffer(s).
//This code allocates and resizes them as needed:
struct Framebuffers {
	glm::uvec2 size = glm::uvec2(0,0); //remember the size of the framebuffer

	//This framebuffer is used for fullscreen effects:
	GLuint color_tex = 0;
	GLuint depth_rb = 0;
	GLuint fb = 0;

	//This framebuffer is used for shadow maps:
	glm::uvec2 shadow_size = glm::uvec2(0,0);
	GLuint shadow_color_tex = 0; //DEBUG
	GLuint shadow_depth_tex = 0;
	GLuint shadow_fb = 0;

	void allocate(glm::uvec2 const &new_size, glm::uvec2 const &new_shadow_size) {
		//allocate full-screen framebuffer:
		if (size != new_size) {
			size = new_size;

			if (color_tex == 0) glGenTextures(1, &color_tex);
			glBindTexture(GL_TEXTURE_2D, color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size.x, size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (depth_rb == 0) glGenRenderbuffers(1, &depth_rb);
			glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
	
			if (fb == 0) glGenFramebuffers(1, &fb);
			glBindFramebuffer(GL_FRAMEBUFFER, fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rb);
			gl_check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}

		//allocate shadow map framebuffer:
		if (shadow_size != new_shadow_size) {
			shadow_size = new_shadow_size;

			if (shadow_color_tex == 0) glGenTextures(1, &shadow_color_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_color_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, shadow_size.x, shadow_size.y, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);


			if (shadow_depth_tex == 0) glGenTextures(1, &shadow_depth_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_depth_tex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadow_size.x, shadow_size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
	
			if (shadow_fb == 0) glGenFramebuffers(1, &shadow_fb);
			glBindFramebuffer(GL_FRAMEBUFFER, shadow_fb);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, shadow_color_tex, 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_depth_tex, 0);
			gl_check_fb();
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			GL_ERRORS();
		}
	}
} fbs;

void ShadowMapMode::draw(glm::uvec2 const &drawable_size) {
	fbs.allocate(drawable_size, glm::uvec2(512, 512));

	//Draw scene to shadow map for spotlight:
	glBindFramebuffer(GL_FRAMEBUFFER, fbs.shadow_fb);
	glViewport(0,0,fbs.shadow_size.x, fbs.shadow_size.y);

	glClearColor(1.0f, 0.0f, 1.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

	//render only back faces to shadow map (prevent shadow speckles on fronts of objects):
	glCullFace(GL_FRONT);
	glEnable(GL_CULL_FACE);

	scene->draw(*spot, Scene::Drawable::PipelineTypeShadow);

	glDisable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	GL_ERRORS();


	//----- draw scene to the window -----

	glViewport(0,0,drawable_size.x, drawable_size.y);

	camera->aspect = drawable_size.x / float(drawable_size.y);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up basic OpenGL state:
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	//set up light positions:
	glUseProgram(shadowed_color_texture_program->program);

	//don't use distant directional light at all (color == 0):
	glUniform3fv(shadowed_color_texture_program->sun_color_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
	glUniform3fv(shadowed_color_texture_program->sun_direction_vec3, 1, glm::value_ptr(glm::normalize(glm::vec3(0.0f, 0.0f,-1.0f))));
	//use hemisphere light for subtle ambient light:
	glUniform3fv(shadowed_color_texture_program->sky_color_vec3, 1, glm::value_ptr(glm::vec3(0.2f, 0.2f, 0.3f)));
	glUniform3fv(shadowed_color_texture_program->sky_direction_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f)));

	glm::mat4 spot_from_world =
		//This matrix converts from the spotlight's clip space ([-1,1]^3) into depth map texture coordinates ([0,1]^2) and depth map Z values ([0,1]):
		glm::mat4(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, 0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 0.5f, 0.0f,
			0.5f, 0.5f, 0.5f+0.00001f /* <-- bias */, 1.0f
		)
		//this is the world-to-clip matrix used when rendering the shadow map:
		* spot->make_projection() * glm::mat4(spot->transform->make_local_from_world());

	glUniformMatrix4fv(shadowed_color_texture_program->SPOT_FROM_LIGHT_mat4, 1, GL_FALSE, glm::value_ptr(spot_from_world));

	glm::mat4 world_from_spot = spot->transform->make_world_from_local();
	glUniform3fv(shadowed_color_texture_program->spot_position_vec3, 1, glm::value_ptr(glm::vec3(world_from_spot[3])));
	glUniform3fv(shadowed_color_texture_program->spot_direction_vec3, 1, glm::value_ptr(-glm::vec3(world_from_spot[2])));
	glUniform3fv(shadowed_color_texture_program->spot_color_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

	glm::vec2 spot_outer_inner = glm::vec2(std::cos(0.5f * spot->spot_fov), std::cos(0.85f * 0.5f * spot->spot_fov));
	glUniform2fv(shadowed_color_texture_program->spot_outer_inner_vec2, 1, glm::value_ptr(spot_outer_inner));

	//This code binds texture index 1 to the shadow map:
	// (note that this is a bit brittle -- it depends on none of the objects in the scene having a texture of index 1 set in their material data; otherwise scene::draw would unbind this texture):
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, fbs.shadow_depth_tex);
	//The shadow_depth_tex must have these parameters set to be used as a sampler2DShadow in the shader:
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);
	//NOTE: however, these are parameters of the texture object, not the binding point, so there is no need to set them *each frame*. I'm doing it here so that you are likely to see that they are being set.
	glActiveTexture(GL_TEXTURE0);

	scene->draw(*camera);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	GL_ERRORS();
}
