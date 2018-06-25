
// Copyright (c) 2012 Christopher Lux <christopherlux@gmail.com>
// Distributed under the Modified BSD License, see license.txt.

#include <iostream>
#include <sstream>
#include <vector>

#include <boost/assign/list_of.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include <thread>
#include <mutex>

#include <boost/log/trivial.hpp>

#include <scm/core.h>
#include <scm/log.h>
#include <scm/core/pointer_types.h>
#include <scm/core/io/tools.h>
#include <scm/core/time/accum_timer.h>
#include <scm/core/time/high_res_timer.h>

#include <scm/gl_core.h>

#include <scm/gl_util/data/imaging/texture_loader.h>
#include <scm/gl_util/manipulators/trackball_manipulator.h>
#include <scm/gl_util/primitives/box.h>
#include <scm/gl_util/primitives/quad.h>
#include <scm/gl_util/primitives/wavefront_obj.h>

#include <GLFW/glfw3.h>

struct window_group {
  GLFWwindow* window = nullptr;
  GLFWwindow* offscreen_window = nullptr;
};

std::shared_ptr<window_group> windows = nullptr;
std::mutex texture_write;

static int const initial_window_width = 1920;
static int const initial_window_height = 1080;

const scm::math::vec3f diffuse(0.7f, 0.7f, 0.7f);
const scm::math::vec3f specular(0.2f, 0.7f, 0.9f);
const scm::math::vec3f ambient(0.1f, 0.1f, 0.1f);
const scm::math::vec3f position(1, 1, 1);

class demo_app
{
public:
  demo_app() {
    _initx = 0;
    _inity = 0;

    _window_width = initial_window_width;
    _window_height = initial_window_height;

    _lb_down = false;
    _mb_down = false;
    _rb_down = false;

    _dolly_sens = 10.0f;

    _projection_matrix = scm::math::mat4f::identity();
  }
  virtual ~demo_app();

  int window_width() const { return _window_width; };
  int window_height() const { return _window_height; };

  bool initialize();
  void initialize_framebuffer();

  void render_to_texture();
  void postprocess_frame();
  void render_from_texture();

  void resize(int w, int h);
  void mouse_func(GLFWwindow* window, int button, int action, int mods);
  void mouse_motion_func(GLFWwindow* window, double xpos, double ypos);
  void keyboard(unsigned char key, int x, int y);

private:
  scm::gl::trackball_manipulator _trackball_manip;
  float _initx;
  float _inity;

  int _window_width;
  int _window_height;

  bool _lb_down;
  bool _mb_down;
  bool _rb_down;

  float _dolly_sens;

  scm::shared_ptr<scm::gl::render_device>     _device;

  scm::shared_ptr<scm::gl::render_context>    _fast_context;
  scm::shared_ptr<scm::gl::render_context>    _slow_context;

  scm::gl::program_ptr        _shader_program;

  scm::gl::buffer_ptr         _index_buffer;
  scm::gl::vertex_array_ptr   _vertex_array;

  scm::math::mat4f            _projection_matrix;

  scm::shared_ptr<scm::gl::box_geometry>  _box;
  scm::shared_ptr<scm::gl::wavefront_obj_geometry>  _obj;
  scm::gl::depth_stencil_state_ptr     _dstate_less;
  scm::gl::depth_stencil_state_ptr     _dstate_disable;

  scm::gl::blend_state_ptr            _no_blend;
  scm::gl::blend_state_ptr            _blend_omsa;
  scm::gl::blend_state_ptr            _color_mask_green;

  scm::gl::texture_2d_ptr             _color_texture;

  scm::gl::sampler_state_ptr          _filter_lin_mip;
  scm::gl::sampler_state_ptr          _filter_aniso;
  scm::gl::sampler_state_ptr          _filter_nearest;
  scm::gl::sampler_state_ptr          _filter_linear;

  scm::gl::texture_2d_ptr             _color_buffer;
  scm::gl::texture_2d_ptr             _color_buffer_resolved;
  scm::gl::texture_2d_ptr             _depth_buffer;
  scm::gl::frame_buffer_ptr           _framebuffer;
  scm::gl::frame_buffer_ptr           _framebuffer_resolved;
  scm::shared_ptr<scm::gl::quad_geometry>  _quad;
  scm::gl::program_ptr                _pass_through_shader;
  scm::gl::depth_stencil_state_ptr    _depth_no_z;
  scm::gl::rasterizer_state_ptr       _ms_back_cull;


}; // class demo_app


namespace {

  scm::scoped_ptr<demo_app> _application;

} // namespace

///////////////////////////////////////////////////////////////////////////////
demo_app::~demo_app()
{
  _shader_program.reset();
  _index_buffer.reset();
  _vertex_array.reset();

  _box.reset();
  _obj.reset();

  _filter_lin_mip.reset();
  _filter_aniso.reset();
  _filter_nearest.reset();
  _color_texture.reset();

  _filter_linear.reset();
  _color_buffer.reset();
  _depth_buffer.reset();
  _framebuffer.reset();
  _quad.reset();
  _pass_through_shader.reset();
  _depth_no_z.reset();
  _ms_back_cull.reset();
  _color_buffer_resolved.reset();
  _framebuffer_resolved.reset();

  _fast_context.reset();
  _slow_context.reset();
  _device.reset();
}

///////////////////////////////////////////////////////////////////////////////
bool demo_app::initialize()
{
  using namespace scm;
  using namespace scm::gl;
  using namespace scm::math;
  using boost::assign::list_of;

  std::string vs_source;
  std::string fs_source;

  if (!io::read_text_file("../res/shaders/phong_lighting.glslv", vs_source)
    || !io::read_text_file("../res/shaders/phong_lighting.glslf", fs_source)) {
    scm::err() << "error reading shader files" << log::end;
    return (false);
  }

  _device.reset(new scm::gl::render_device());

  _fast_context = _device->main_context();
  _slow_context = _device->create_context();

  _shader_program = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, vs_source))
    (_device->create_shader(STAGE_FRAGMENT_SHADER, fs_source)));

  if (!_shader_program) {
    scm::err() << "error creating shader program" << log::end;
    return (false);
  }

  _shader_program->uniform("light_ambient", ambient);
  _shader_program->uniform("light_diffuse", diffuse);
  _shader_program->uniform("light_specular", specular);
  _shader_program->uniform("light_position", position);

  _shader_program->uniform("material_ambient", ambient);
  _shader_program->uniform("material_diffuse", diffuse);
  _shader_program->uniform("material_specular", specular);
  _shader_program->uniform("material_shininess", 128.0f);
  _shader_program->uniform("material_opacity", 1.0f);

  scm::out() << *_device << scm::log::end;

  std::vector<scm::math::vec3f>   positions_normals;
  std::vector<unsigned short>              indices;

  positions_normals.push_back(scm::math::vec3f(0.0f, 0.0f, 0.0f));
  positions_normals.push_back(scm::math::vec3f(0.0f, 0.0f, 1.0f));

  positions_normals.push_back(scm::math::vec3f(1.0f, 0.0f, 0.0f));
  positions_normals.push_back(scm::math::vec3f(0.0f, 0.0f, 1.0f));

  positions_normals.push_back(scm::math::vec3f(1.0f, 1.0f, 0.0f));
  positions_normals.push_back(scm::math::vec3f(0.0f, 0.0f, 1.0f));

  positions_normals.push_back(scm::math::vec3f(0.0f, 1.0f, 0.0f));
  positions_normals.push_back(scm::math::vec3f(0.0f, 0.0f, 1.0f));

  indices.push_back(0);
  indices.push_back(1);
  indices.push_back(2);
  indices.push_back(0);
  indices.push_back(2);
  indices.push_back(3);

  buffer_ptr positions_normals_buf;

  positions_normals_buf = _device->create_buffer(BIND_VERTEX_BUFFER, USAGE_STATIC_DRAW, positions_normals.size() * sizeof(scm::math::vec3f), &positions_normals.front());
  _index_buffer = _device->create_buffer(BIND_INDEX_BUFFER, USAGE_STATIC_DRAW, indices.size() * sizeof(unsigned short), &indices.front());

  _vertex_array = _device->create_vertex_array(vertex_format(0, 0, TYPE_VEC3F, 2 * sizeof(scm::math::vec3f))
    (0, 1, TYPE_VEC3F, 2 * sizeof(scm::math::vec3f)),
    list_of(positions_normals_buf));

  _dstate_less = _device->create_depth_stencil_state(true, true, COMPARISON_LESS);
  depth_stencil_state_desc dstate = _dstate_less->descriptor();
  dstate._depth_test = false;

  _dstate_disable = _device->create_depth_stencil_state(dstate);
  //_dstate_disable = _device->create_depth_stencil_state(false);
  _no_blend = _device->create_blend_state(false, FUNC_ONE, FUNC_ZERO, FUNC_ONE, FUNC_ZERO);
  _blend_omsa = _device->create_blend_state(true, FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ONE, FUNC_ZERO);
  _color_mask_green = _device->create_blend_state(true, FUNC_SRC_ALPHA, FUNC_ONE_MINUS_SRC_ALPHA, FUNC_ONE, FUNC_ZERO,
    EQ_FUNC_ADD, EQ_FUNC_ADD, COLOR_GREEN | COLOR_BLUE);

  _box.reset(new box_geometry(_device, vec3f(-0.5f), vec3f(0.5f)));
  _obj.reset(new wavefront_obj_geometry(_device, "../res/geometry/box.obj"));

  texture_loader tex_loader;
  _color_texture = tex_loader.load_texture_2d(*_device,
    "../res/textures/0001MM_diff.jpg", true, false);

  _filter_lin_mip = _device->create_sampler_state(FILTER_MIN_MAG_MIP_LINEAR, WRAP_CLAMP_TO_EDGE);
  _filter_aniso = _device->create_sampler_state(FILTER_ANISOTROPIC, WRAP_CLAMP_TO_EDGE, 16);
  _filter_nearest = _device->create_sampler_state(FILTER_MIN_MAG_NEAREST, WRAP_CLAMP_TO_EDGE);
  _filter_linear = _device->create_sampler_state(FILTER_MIN_MAG_LINEAR, WRAP_CLAMP_TO_EDGE);

  _quad.reset(new quad_geometry(_device, vec2f(0.0f, 0.0f), vec2f(1.0f, 1.0f)));
  _depth_no_z = _device->create_depth_stencil_state(false, false);
  _ms_back_cull = _device->create_rasterizer_state(FILL_SOLID, CULL_NONE, ORIENT_CCW, true);

  if (!io::read_text_file("../res/shaders/texture_program.glslv", vs_source)
    || !io::read_text_file("../res/shaders/texture_program.glslf", fs_source)) {
    scm::err() << "error reading shader files" << log::end;
    return (false);
  }

  _pass_through_shader = _device->create_program(list_of(_device->create_shader(STAGE_VERTEX_SHADER, vs_source))
    (_device->create_shader(STAGE_FRAGMENT_SHADER, fs_source)));


  _trackball_manip.dolly(2.5f);

  initialize_framebuffer();

  return (true);
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::initialize_framebuffer() 
{
  using namespace scm::gl;
  using namespace scm::math;

  _color_buffer = _device->create_texture_2d(vec2ui(_window_width, _window_height) * 1, FORMAT_RGBA_8, 1, 1, 8);
  _depth_buffer = _device->create_texture_2d(vec2ui(_window_width, _window_height) * 1, FORMAT_D24, 1, 1, 8);
  _framebuffer = _device->create_frame_buffer();
  _framebuffer->attach_color_buffer(0, _color_buffer);
  _framebuffer->attach_depth_stencil_buffer(_depth_buffer);

  _color_buffer_resolved = _device->create_texture_2d(vec2ui(_window_width, _window_height) * 1, FORMAT_RGBA_8);
  _framebuffer_resolved = _device->create_frame_buffer();
  _framebuffer_resolved->attach_color_buffer(0, _color_buffer_resolved);
}

unsigned plah = 0;

///////////////////////////////////////////////////////////////////////////////
void demo_app::render_to_texture()
{
  using namespace scm::gl;
  using namespace scm::math;

  // clear the color and depth buffer
  glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

  mat4f    view_matrix = _trackball_manip.transform_matrix();
  mat4f    model_matrix = mat4f::identity();
  mat4f    model_view_matrix = view_matrix * model_matrix;
  mat4f    mv_inv_transpose = transpose(inverse(model_view_matrix));

  _shader_program->uniform("projection_matrix", _projection_matrix);
  _shader_program->uniform("model_view_matrix", model_view_matrix);
  _shader_program->uniform("model_view_matrix_inverse_transpose", mv_inv_transpose);
  _shader_program->uniform_sampler("color_texture_aniso", 0);
  _shader_program->uniform_sampler("color_texture_nearest", 1);

  _slow_context->clear_default_color_buffer(FRAMEBUFFER_BACK, vec4f(.2f, .2f, .2f, 1.0f));
  _slow_context->clear_default_depth_stencil_buffer();

  _slow_context->reset();

  // multi sample pass
  { 
    context_state_objects_guard csg(_slow_context);
    context_texture_units_guard tug(_slow_context);
    context_framebuffer_guard   fbg(_slow_context);

    _slow_context->clear_default_color_buffer(FRAMEBUFFER_BACK, vec4f(.2f, .2f, .2f, 1.0f));

    _slow_context->clear_color_buffer(_framebuffer, 0, vec4f(.2f, .2f, .2f, 1.0f));
    _slow_context->clear_depth_stencil_buffer(_framebuffer, 1.0);
    _slow_context->set_frame_buffer(_framebuffer);

    _slow_context->set_viewport(viewport(vec2ui(0, 0), 1 * vec2ui(_window_width, _window_height)));

    _slow_context->set_depth_stencil_state(_dstate_less);
    _slow_context->set_blend_state(_no_blend);
    _slow_context->set_rasterizer_state(_ms_back_cull);

    _slow_context->bind_program(_shader_program);

    _slow_context->bind_texture(_color_texture, _filter_aniso, 0);
    _slow_context->bind_texture(_color_texture, _filter_nearest, 1);

    _obj->draw(_slow_context);
  }

}

///////////////////////////////////////////////////////////////////////////////
void demo_app::postprocess_frame()
{
  // blit multisample texture to texture and generate mipmap pyramid
  _slow_context->resolve_multi_sample_buffer(_framebuffer, _framebuffer_resolved);
  _slow_context->generate_mipmaps(_color_buffer_resolved);
  _slow_context->reset();
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::render_from_texture()
{
  using namespace scm::gl;
  using namespace scm::math;

  mat4f   pass_mvp = mat4f::identity();
  ortho_matrix(pass_mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

  const opengl::gl_core& glapi = _fast_context->opengl_api();

  _pass_through_shader->uniform_sampler("in_texture", 0);
  _pass_through_shader->uniform("mvp", pass_mvp);

  _fast_context->set_default_frame_buffer();

  _fast_context->set_depth_stencil_state(_depth_no_z);
  _fast_context->set_blend_state(_no_blend);

  _fast_context->bind_program(_pass_through_shader);

  _fast_context->bind_texture(_color_buffer_resolved, _filter_nearest, 0);
  _fast_context->apply();
  _quad->draw(_fast_context);
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::resize(int w, int h)
{
  // safe the new dimensions
  _window_width = w;
  _window_height = h;

  // set the new viewport into which now will be rendered
  using namespace scm::gl; 
  using namespace scm::math;

  _fast_context->set_viewport(viewport(vec2ui(0, 0), vec2ui(w, h)));

  scm::math::perspective_matrix(_projection_matrix, 60.f, float(w) / float(h), 0.1f, 1000.0f);

  initialize_framebuffer();
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::mouse_func(GLFWwindow* window, int button, int action, int mods)
{
  switch (button) {
  case GLFW_MOUSE_BUTTON_LEFT:
  {
    _lb_down = (action == GLFW_PRESS) ? true : false;
  }break;
  case GLFW_MOUSE_BUTTON_MIDDLE:
  {
    _mb_down = (action == GLFW_PRESS) ? true : false;
  }break;
  case GLFW_MOUSE_BUTTON_RIGHT:
  {
    _rb_down = (action == GLFW_PRESS) ? true : false;
  }break;
  }

  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);

  _initx = 2.f * float(xpos - (_window_width / 2)) / float(_window_width);
  _inity = 2.f * float(_window_height - ypos - (_window_height / 2)) / float(_window_height);
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::mouse_motion_func(GLFWwindow* window, double xpos, double ypos)
{
  float nx = 2.f * float(xpos - (_window_width / 2)) / float(_window_width);
  float ny = 2.f * float(_window_height - ypos - (_window_height / 2)) / float(_window_height);

  if (_lb_down) {
    _trackball_manip.rotation(_initx, _inity, nx, ny);
  }
  if (_rb_down) {
    _trackball_manip.dolly(_dolly_sens * (ny - _inity));
  }
  if (_mb_down) {
    _trackball_manip.translation(nx - _initx, ny - _inity);
  }

  _inity = ny;
  _initx = nx;
}

///////////////////////////////////////////////////////////////////////////////
void demo_app::keyboard(unsigned char key, int x, int y)
{}

///////////////////////////////////////////////////////////////////////////////
static void resize_callback(GLFWwindow* window, int w, int h)
{
  if (_application)
    _application->resize(w, h);
}

///////////////////////////////////////////////////////////////////////////////
static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
  if (_application)
    _application->mouse_func(window, button, action, mods);
}

///////////////////////////////////////////////////////////////////////////////
static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
  if (_application)
    _application->mouse_motion_func(window, xpos, ypos);
}

///////////////////////////////////////////////////////////////////////////////
void init_window(std::shared_ptr<window_group> const& wgroup)
{
  /* Configure OpenGL context */
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  /* Create a windowed mode window and its OpenGL context */
  auto win = glfwCreateWindow(_application->window_width(), _application->window_height(), "Async Rendering Window", NULL, NULL);

  if (win) {
    BOOST_LOG_TRIVIAL(info) << "Initialize fast client window succeed." << std::endl;
  }
  else {
    BOOST_LOG_TRIVIAL(error) << "Initialize fast client window failed." << std::endl;
  }
  wgroup->window = win;

  // Make the window's context current */
  // glfwMakeContextCurrent(wgroup->window);

  // set callbacks
  glfwSetMouseButtonCallback(wgroup->window, mouse_button_callback);
  glfwSetCursorPosCallback(wgroup->window, cursor_position_callback);
  glfwSetWindowSizeCallback(wgroup->window, resize_callback);
}

///////////////////////////////////////////////////////////////////////////////
void init_offscreen_window(std::shared_ptr<window_group> const& wgroup)
{
  int  a = 0;
  if (!wgroup->window) {
    return;
  }

  // context creation configuration
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  glfwWindowHint(GLFW_VISIBLE, false);

  // create window
  // glfwMakeContextCurrent(wgroup->window);
  auto win = glfwCreateWindow(_application->window_width(), _application->window_height(), "Async Rendering Offscreen", NULL, wgroup->window);

  if (win) {
    BOOST_LOG_TRIVIAL(info) << "Initialize slow client window succeed." << std::endl;
    wgroup->offscreen_window = win;
  }
  else {
    BOOST_LOG_TRIVIAL(error) << "Initialize slow client window failed." << std::endl;
  }
}

///////////////////////////////////////////////////////////////////////////////
void fast_client(std::shared_ptr<window_group> const& wgroup)
{
  if (!wgroup->window) {
    init_window(wgroup);
  }


  // init the GL context
  glfwMakeContextCurrent(wgroup->window);
  if (!_application->initialize()) {
    BOOST_LOG_TRIVIAL(error) << "error initializing gl context" << std::endl;
  }


  // force resize
  _application->resize(initial_window_width, initial_window_height);


  // render loop
  while (!glfwWindowShouldClose(wgroup->window))
  {
    // Make the window's context current */
    glfwMakeContextCurrent(wgroup->window);

    {
      //BOOST_LOG_TRIVIAL(info) << "Fast Client : Render to texture." << std::endl;
      _application->render_to_texture();
      _application->postprocess_frame();
      _application->render_from_texture();
    }

    glfwSwapBuffers(wgroup->window);
    glfwPollEvents();
  }
}

///////////////////////////////////////////////////////////////////////////////
void slow_client(std::shared_ptr<window_group> const& wgroup)
{
  while (!wgroup->offscreen_window) {
    init_offscreen_window(wgroup);
  }

  while (true) {
    // std::lock_guard<std::mutex> lock(texture_write);
    //BOOST_LOG_TRIVIAL(info) << "Slow Client : Render to texture." << std::endl;
  }
}

void error_callback(int error, const char* description)
{
  BOOST_LOG_TRIVIAL(error) << " error code : " << int(error) << " description:" << description << std::endl;
}


///////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
  /* Initialize the library */
  scm::shared_ptr<scm::core>      scm_core(new scm::core(argc, argv));

  if (!glfwInit()) {
    BOOST_LOG_TRIVIAL(error) << "Failed to init GLFW" << std::endl;
  }

  glfwSetErrorCallback(error_callback);
  
  _application.reset(new demo_app());

  windows = std::make_shared<window_group>();

  init_window(windows);
  init_offscreen_window(windows);

  //glfwMakeContextCurrent(windows->window);

  std::thread fast_thread(std::bind(fast_client, std::ref(windows)));
  std::thread slow_thread(std::bind(slow_client, std::ref(windows)));

  fast_thread.join();
  slow_thread.join();

  glfwTerminate();

  return (0);
}