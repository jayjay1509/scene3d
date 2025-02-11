#include <fstream>
#include <map>
#include <array>
#include <imgui.h>
#include <iostream>
#include <sstream>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "engine.h"
#include "file_utility.h"
#include "free_camera.h"
#include "global_utility.h"
#include "model.h"
#include "scene3d.h"
#include "shader.h"
#include <numbers>
#include <random>
#include "texture_loader.h"

namespace gpr5300
{
class Scene3D final : public Scene
{
 public:
  void Begin() override;
  void End() override;
  void Update(float dt) override;
  void OnEvent(const SDL_Event& event) override;
  void DrawImGui() override;
  void UpdateCamera(const float dt) override;

 private:

  static constexpr float Lerp(float f) {
    return 0.1f + f * (1.0f - 0.1f);
  }


  float aspect_v = 720.0f;
  float aspect_h = 1280.0f;
  float aspect = aspect_h / aspect_v;
  int kKernelSize = 32;


  float fovY = glm::radians(45.0f);
  float zNear = 0.1f;
  float zFar = 1000.0f;


  //bloom
  Shader shader_light_ = {};
  Shader shader_blur_ = {};
  Shader shader_bloom_final_ = {};

  Shader geometry_shader_ = {};
  Shader lighting_shader_ = {};
  Shader ssao_shader_ = {};
  Shader ssao_blur_shader_ = {};




  GLuint hdr_fbo_ = 0;
  GLuint color_buffer_[2] = {};
  GLuint rbo_depth_ = 0;

  unsigned int g_buffer_ = 0;
  unsigned int ssao_fbo_ = 0, ssao_blur_fbo_ = 0;
  unsigned int g_position_ = 0, g_normal_ = 0, g_albedo_ = 0;
  unsigned int noise_texture_ = 0;
  unsigned int ssao_color_buffer_ = 0, ssao_color_buffer_blur_ = 0;

  std::vector<glm::vec3> ssao_kernel_{};

  //Pingpong for blur
  GLuint pingpong_fbo_[2] = {};
  GLuint pingpong_color_buffer_[2] = {};

  std::vector<glm::vec3> light_positions_ = {};
  std::vector<glm::vec3> light_colors_ = {};


  //model
  Shader shader_model_ = {};
  Model model_;
  Model model_2_;

  //skybox
  Shader skybox_program_ = {};

  GLuint skybox_vao_ = 0;
  GLuint skybox_vbo_ = 0;

  unsigned int skybox_texture_ = -1;

  float skybox_vertices_[108] = {};

  //------------------------
  FreeCamera camera_ {};
  float elapsedTime_ = 0.0f;



  float model_scale_ = 0.05;
  float model_scale_2_ = 0.1;
  float Normal_x = 0.1;
  float Normal_y = 0.1;
  float Normal_z = 12.1;
  float Normal_Rotation_angle = 180.1;
  float scaleFactor_instancing = 0.1f;
  float gamma_ = 2.2f;
  bool bloom_state_ = false;
  bool ssao_state_ = false;
  bool Normal_state_ = true;
  float exposure_ = 0.2f;

  Frustum frustum_;

  Shader Instancing_shader_;

  unsigned int Instancing_buffer_;
  glm::mat4* modelMatrices {};
  Model Instancing_Model_;
  unsigned int Instancing_amout;
  Shader Normal_Map;

  unsigned int ground_text_ = 0;
  unsigned int ground_text_normal_ = 0;


  glm::vec3 lightPos0 = glm::vec3(0.0f, 0.5f, 10.5f);
  glm::vec3 lightPos1 = glm::vec3(-40.0f, 0.5f, -30.0f);
  glm::vec3 lightPos2 = glm::vec3(30.0f, 0.5f, 1.0f);
  glm::vec3 lightPos3 = glm::vec3(-0.8f, 20.4f, -1.0f);

  glm::vec3 lightColor0 = glm::vec3(5.0f, 5.0f, 5.0f);
  glm::vec3 lightColor1 = glm::vec3(5.0f, 5.0f, 5.0f);
  glm::vec3 lightColor2 = glm::vec3(5.0f, 5.0f, 5.0f);
  glm::vec3 lightColor3 = glm::vec3(5.0f, 5.0f, 5.0f);





};

void Scene3D::Begin()
{
  // configure global opengl state
  // -----------------------------
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glEnable(GL_CULL_FACE);
  // glCullFace(GL_FRONT);


  //Build shaders
  Normal_Map = Shader("data/shaders/scene3d/normal_map.vert","data/shaders/scene3d/normal_map.frag");
  Instancing_shader_ = Shader("data/shaders/scene3d/instancing.vert", "data/shaders/scene3d/instancing.frag");
  shader_light_ = Shader("data/shaders/bloom/bloom.vert", "data/shaders/bloom/light.frag");
  shader_blur_ = Shader("data/shaders/bloom/blur.vert", "data/shaders/bloom/blur.frag");
  shader_bloom_final_ = Shader("data/shaders/bloom/bloom_final.vert", "data/shaders/bloom/bloom_final.frag");
  shader_model_ = Shader("data/shaders/scene3d/model.vert", "data/shaders/scene3d/model.frag");
  skybox_program_ = Shader("data/shaders/scene3d/cubemaps.vert", "data/shaders/scene3d/cubemaps.frag");
  geometry_shader_ = Shader("data/shaders/ssao/geometry_pass.vert","data/shaders/ssao/geometry_pass.frag");
  lighting_shader_ = Shader("data/shaders/ssao/lightning_pass.vert","data/shaders/ssao/lightning_pass.frag");
  ssao_shader_ = Shader("data/shaders/ssao/ssao.vert","data/shaders/ssao/ssao.frag");
  ssao_blur_shader_ = Shader("data/shaders/ssao/ssao_blur.vert","data/shaders/ssao/ssao_blur.frag");


  model_ = Model("data/15/scene.gltf");
  model_2_ = Model("data/17/scene.gltf");
  Instancing_Model_ = Model("data/14/scene.gltf");

  ground_text_ = TextureFromFile("brickwall.jpg", "data/textures");
  ground_text_normal_ = TextureFromFile("brickwall_normal.jpg", "data/textures");

  //Configure FBO
  glGenFramebuffers(1, &hdr_fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
  //We need 2 floating point color buffers, for normal rendering and brightness thresholds
  glGenTextures(2, color_buffer_);
  for (unsigned int i = 0; i < 2; i++)
  {
    glBindTexture(GL_TEXTURE_2D, color_buffer_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1280, 720, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //be sure to clamp to edge!
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, color_buffer_[i], 0);
  }

  //create and attach depth buffer
  glGenRenderbuffers(1, &rbo_depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1280, 720);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth_);
  //select color attachment
  unsigned int attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, attachments);
  // finally check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    std::cout << "Framebuffer not complete!" << std::endl;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  //Pingpong for blur
  glGenFramebuffers(2, pingpong_fbo_);
  glGenTextures(2, pingpong_color_buffer_);
  for (unsigned int i = 0; i < 2; i++)
  {
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[i]);
    glBindTexture(GL_TEXTURE_2D, pingpong_color_buffer_[i]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1280, 720, 0, GL_RGBA, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpong_color_buffer_[i], 0);
    // also check if framebuffers are complete (no need for depth buffer)
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "Framebuffer not complete!" << std::endl;
  }

  // lighting info
  // -------------
  // positions
  light_positions_.push_back(lightPos0);
  light_positions_.push_back(lightPos1);
  light_positions_.push_back(lightPos2);
  light_positions_.push_back(lightPos3);
  // colors
  light_colors_.push_back(lightColor0);
  light_colors_.push_back(lightColor1);
  light_colors_.push_back(lightColor2);
  light_colors_.push_back(lightColor3);


//  light_colors_.push_back(glm::vec3(5.0f, 5.0f, 5.0f));
//  light_colors_.push_back(glm::vec3(10.0f, 0.0f, 0.0f));
//  light_colors_.push_back(glm::vec3(0.0f, 0.0f, 15.0f));
//  light_colors_.push_back(glm::vec3(0.0f, 5.0f, 0.0f));

  Instancing_amout = 1000;
  modelMatrices = new glm::mat4[Instancing_amout];
  srand(15678);
  float radius = 10.0f;
  float offset = 1.0f;
  for (unsigned int i = 0; i < Instancing_amout; i++) {
    glm::mat4 model = glm::mat4(1.0f);
    float baseAngle = glm::radians((float) i / (float) Instancing_amout * 360.0f);
    float displacement = (rand() % (int) (2 * offset * 100)) / 100.0f - offset;
    float x = sin(baseAngle) * radius + displacement;
    displacement = (rand() % (int) (2 * offset * 100)) / 100.0f - offset;
    float y = displacement * 0.4f;
    displacement = (rand() % (int) (2 * offset * 100)) / 100.0f - offset;
    float z = cos(baseAngle) * radius + displacement;
    glm::vec3 pos = glm::vec3(x, y, z);
    float scaleVal = (rand() % 20) / 100.0f + 0.05f;
    glm::mat4 corrective = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 rotationInstance = glm::rotate(glm::mat4(1.0f), baseAngle, glm::vec3(0.0f, 1.0f, 0.0f));

    model = glm::translate(glm::mat4(1.0f), pos)
        * rotationInstance
        * corrective
        * glm::scale(glm::mat4(1.0f), glm::vec3(scaleVal * scaleFactor_instancing));

    modelMatrices[i] = model;

  }


    glGenFramebuffers(1, &g_buffer_);
    glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_);


    glGenTextures(1, &g_position_);
    glBindTexture(GL_TEXTURE_2D, g_position_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, aspect_v, aspect_h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_position_, 0);

    glGenTextures(1, &g_normal_);
    glBindTexture(GL_TEXTURE_2D, g_normal_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, aspect_v, aspect_h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, g_normal_, 0);
    // color + specular color buffer
    glGenTextures(1, &g_albedo_);
    glBindTexture(GL_TEXTURE_2D, g_albedo_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, aspect_v, aspect_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, g_albedo_, 0);

    unsigned int new_attachments[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, new_attachments);

    unsigned int rbo_depth;
    glGenRenderbuffers(1, &rbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, aspect_v, aspect_h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_depth);
    // finally check if framebuffer is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    glGenFramebuffers(1, &ssao_fbo_);
    glGenFramebuffers(1, &ssao_blur_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);

    // SSAO color buffer
    glGenTextures(1, &ssao_color_buffer_);
    glBindTexture(GL_TEXTURE_2D, ssao_color_buffer_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, aspect_v, aspect_h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_color_buffer_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "SSAO Framebuffer not complete!" << std::endl;

    // and blur stage
    glBindFramebuffer(GL_FRAMEBUFFER, ssao_blur_fbo_);
    glGenTextures(1, &ssao_color_buffer_blur_);
    glBindTexture(GL_TEXTURE_2D, ssao_color_buffer_blur_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, aspect_v, aspect_h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssao_color_buffer_blur_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
      std::cout << "SSAO Blur Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // generate sample kernel
    // ----------------------
    std::uniform_real_distribution<GLfloat> random_floats(0.0, 1.0); // generates random floats between 0.0 and 1.0
    std::default_random_engine generator;

    ssao_kernel_.resize(kKernelSize);
    for (std::int32_t i = 0; i < kKernelSize; ++i) {
      glm::vec3 sample(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0,
                       random_floats(generator));
      sample = glm::normalize(sample);
      sample *= random_floats(generator);
      float scale = static_cast<float>(i) / static_cast<float>(kKernelSize);

      // scale samples s.t. they're more aligned to center of kernel
      scale = Lerp(scale * scale);
      sample *= scale;
      ssao_kernel_.push_back(sample);
    }

    std::cout << "stuck\n";

    // generate noise texture
    // ----------------------
    std::vector<glm::vec3> ssao_noise;
    for (unsigned int i = 0; i < 16; i++) {
      glm::vec3 noise(random_floats(generator) * 2.0 - 1.0, random_floats(generator) * 2.0 - 1.0,
                      0.0f); // rotate around z-axis (in tangent space)
      ssao_noise.push_back(noise);
    }
    glGenTextures(1, &noise_texture_);
    glBindTexture(GL_TEXTURE_2D, noise_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssao_noise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // shader configuration
    // --------------------
    glUseProgram(lighting_shader_.id_);
    glUniform1i(glGetUniformLocation(lighting_shader_.id_, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(lighting_shader_.id_, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(lighting_shader_.id_, "gAlbedo"), 2);
    glUniform1i(glGetUniformLocation(lighting_shader_.id_, "ssao"), 3);
    glUseProgram(ssao_shader_.id_);
    glUniform1i(glGetUniformLocation(ssao_shader_.id_, "gPosition"), 0);
    glUniform1i(glGetUniformLocation(ssao_shader_.id_, "gNormal"), 1);
    glUniform1i(glGetUniformLocation(ssao_shader_.id_, "texNoise"), 2);
    glUseProgram(ssao_blur_shader_.id_);
    glUniform1i(glGetUniformLocation(ssao_blur_shader_.id_, "ssaoInput"), 0);








  glGenBuffers(1, &Instancing_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, Instancing_buffer_);
  glBufferData(GL_ARRAY_BUFFER, Instancing_amout * sizeof(glm::mat4), &modelMatrices[0], GL_STATIC_DRAW);
  for(unsigned int i = 0; i < Instancing_Model_.meshes().size(); i++)
  {
    unsigned int VAO = Instancing_Model_.meshes()[i].VAO();
    glBindVertexArray(VAO);
    // vertex attributes
    std::size_t vec4Size = sizeof(glm::vec4);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)0);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(1 * vec4Size));
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(2 * vec4Size));
    glEnableVertexAttribArray(6);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 4 * vec4Size, (void*)(3 * vec4Size));

    glVertexAttribDivisor(3, 1);
    glVertexAttribDivisor(4, 1);
    glVertexAttribDivisor(5, 1);
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
  }

  static constexpr std::array skyboxVertices {
      // positions
      -1.0f, 1.0f, -1.0f,
      -1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, -1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,

      -1.0f, -1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, -1.0f, 1.0f,
      -1.0f, -1.0f, 1.0f,

      -1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, -1.0f,
      1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, 1.0f,
      -1.0f, 1.0f, -1.0f,

      -1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, -1.0f,
      1.0f, -1.0f, -1.0f,
      -1.0f, -1.0f, 1.0f,
      1.0f, -1.0f, 1.0f
  };

  std::ranges::copy(skyboxVertices, skybox_vertices_);


  //skybox VAO
  glGenVertexArrays(1, &skybox_vao_);
  glGenBuffers(1, &skybox_vbo_);
  glBindVertexArray(skybox_vao_);
  glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices_), &skybox_vertices_, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);


  std::vector<std::string_view> faces
      {
          "data/cubemaps/right.jpg",
          "data/cubemaps/left.jpg",
          "data/cubemaps/top.jpg",
          "data/cubemaps/bottom.jpg",
          "data/cubemaps/front.jpg",
          "data/cubemaps/back.jpg"
      };

  skybox_texture_ = GenerateCubemap(faces);



  // shader configuration
  // --------------------
  Normal_Map.Use();
  Normal_Map.SetInt("diffuseMap", 0);
  Normal_Map.SetInt("normalMap", 1);
  shader_blur_.Use();
  shader_blur_.SetInt("image", 0);
  shader_bloom_final_.Use();
  shader_bloom_final_.SetInt("scene", 0);
  shader_bloom_final_.SetInt("bloomBlur", 1);
  Instancing_shader_.Use();
  skybox_program_.Use();
  skybox_program_.SetInt("skybox", 0);

}

void Scene3D::End()
{
  shader_blur_.Delete();
  shader_light_.Delete();
  shader_bloom_final_.Delete();
  skybox_program_.Delete();
  Instancing_shader_.Delete();
  delete[] modelMatrices;
  modelMatrices = nullptr;

}

void Scene3D::Update(const float dt) {

  aspect = aspect_h / aspect_v;

  light_positions_[0] = lightPos0;
  light_positions_[1] = lightPos1;
  light_positions_[2] = lightPos2;
  light_positions_[3] = lightPos3;

  light_colors_[0] = lightColor0;
  light_colors_[1] = lightColor1;
  light_colors_[2] = lightColor2;
  light_colors_[3] = lightColor3;


  UpdateCamera(dt);
  elapsedTime_ += dt;


  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


  auto projection = glm::perspective(fovY, aspect, zNear, zFar);
  auto view = camera_.view();
  auto model = glm::mat4(1.0f);


  glActiveTexture(GL_TEXTURE0);

  glBindFramebuffer(GL_FRAMEBUFFER, 1);
  glDepthFunc(GL_LEQUAL);
  glDepthMask(GL_FALSE);

  skybox_program_.Use();


  glm::mat4 viewS = glm::mat4(glm::mat3(view));
  skybox_program_.SetMat4("view", viewS);
  skybox_program_.SetMat4("projection", projection);

  glBindVertexArray(skybox_vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);
  glDepthFunc(GL_LESS);




  shader_model_.Use();
  shader_model_.SetMat4("projection", projection);
  shader_model_.SetMat4("view", view);


  frustum_.CreateFrustumFromCamera(camera_, aspect, fovY, zNear, zFar);
  glm::mat4 projView = projection * camera_.GetViewMatrix();
  frustum_.Update(projView);
  shader_model_.SetVec3Array("lightPos", light_positions_, light_positions_.size());
  shader_model_.SetVec3Array("lightColor", light_colors_, light_colors_.size());
  shader_model_.SetVec3("viewPos", camera_.camera_position_);


  model = glm::mat4(1.0f);
  model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
  model = glm::scale(model, model_scale_ * glm::vec3(1.0f));
  shader_model_.SetMat4("model", model);
  model_.Draw(shader_model_.id_);


  glm::mat4 model2 = glm::mat4(1.0f);
  model2 = glm::translate(model2, glm::vec3(0.0f, 0.0f, 25.0f));
  model2 = glm::rotate(model2, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f));
  model2 = glm::scale(model2, glm::vec3(model_scale_2_));
  shader_model_.SetMat4("model", model2);
  model_2_.Draw(shader_model_.id_);

  glDisable((GL_CULL_FACE));
  if(Normal_state_){

  //-----------------------------------------------------------------------------------------
  Normal_Map.Use();
  Normal_Map.SetMat4("projection", projection);
  Normal_Map.SetMat4("view", view);

  // render wall
  auto model_4 = glm::mat4(1.0f);
  model_4 = glm::translate(model_4, glm::vec3(Normal_x, Normal_y, Normal_z));
  model_4 = glm::rotate(model_4, glm::radians(Normal_Rotation_angle), glm::normalize(glm::vec3(1.0, 0.0, 0.0)));
  Normal_Map.SetMat4("model", model_4);
  Normal_Map.SetVec3("viewPos", camera_.camera_position_);
  Normal_Map.SetVec3("lightPos", light_positions_[0].x, light_positions_[0].y, light_positions_[0].z);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, ground_text_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, ground_text_normal_);
  normal_renderQuad();
  }
  glDisable((GL_CULL_FACE));



  Instancing_shader_.Use();
  Instancing_shader_.SetMat4("projection", projection);
  Instancing_shader_.SetMat4("view", view);
  Instancing_shader_.SetInt("texture_diffuse1", 0);
  if (!Instancing_Model_.get_textures_loaded().empty()) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Instancing_Model_.get_textures_loaded()[0].id);
  } else {
    std::cerr << "Erreur : Aucune texture chargée !" << std::endl;
  }
  for (unsigned int i = 0; i < Instancing_Model_.meshes().size(); i++) {
    glBindVertexArray(Instancing_Model_.meshes()[i].VAO());
    if (!Instancing_Model_.meshes()[i].indices_.empty()) {
      glDrawElementsInstanced(
          GL_TRIANGLES,
          static_cast<unsigned int>(Instancing_Model_.meshes()[i].indices_.size()),
          GL_UNSIGNED_INT,
          0,
          Instancing_amout
      );
    }
  }


  if(ssao_state_) {

    glBindFramebuffer(GL_FRAMEBUFFER, g_buffer_);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(geometry_shader_.id_);
    geometry_shader_.SetMat4("projection", projection);
    geometry_shader_.SetMat4("view", view);

    glUniform1i(glGetUniformLocation(geometry_shader_.id_, "invertedNormals"), 0);
    model = glm::mat4(1.0f);
    for (int i = 0; i < Instancing_Model_.meshes_.size(); i++) {
      glUniformMatrix4fv(glGetUniformLocation(geometry_shader_.id_, "model"), 1, GL_FALSE,
                         glm::value_ptr(modelMatrices[i]));
      glBindVertexArray(Instancing_Model_.meshes_[i].VAO_);
      glDrawElementsInstanced(GL_TRIANGLES, static_cast<GLsizei>(Instancing_Model_.meshes_[i].indices_.size()),
                              GL_UNSIGNED_INT, nullptr, static_cast<GLsizei>(Instancing_amout));
      glBindVertexArray(0);
    }

    glDisable(GL_CULL_FACE);
    glFrontFace(GL_CW);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, model_scale_ * glm::vec3(1.0f));
    glUniformMatrix4fv(glGetUniformLocation(geometry_shader_.id_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    model_.Draw(shader_model_.id_);

    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 25.0f));
    model = glm::rotate(model, glm::radians(270.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::scale(model, glm::vec3(model_scale_2_));
    glUniformMatrix4fv(glGetUniformLocation(geometry_shader_.id_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    model_2_.Draw(shader_model_.id_);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, ssao_fbo_);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ssao_shader_.id_);
    // Send kernel + rotation
    for (unsigned int i = 0; i < kKernelSize; ++i) {
      std::string path = "samples[" + std::to_string(i) + "]";
      glUniform3f(glGetUniformLocation(ssao_shader_.id_, path.c_str()), ssao_kernel_[i].x, ssao_kernel_[i].y,
                  ssao_kernel_[i].z);
    }
    glUniformMatrix4fv(glGetUniformLocation(ssao_shader_.id_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_position_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_normal_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noise_texture_);
    renderQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, ssao_blur_fbo_);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(ssao_blur_shader_.id_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssao_color_buffer_);
    renderQuad();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(lighting_shader_.id_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // send light relevant uniforms
    glm::vec3 lightPosView = glm::vec3(camera_.view_ * glm::vec4(light_positions_[0], 1.0));

    glUniform3f(glGetUniformLocation(lighting_shader_.id_, "light.Position"), light_positions_[0].x,
                light_positions_[0].y, light_positions_[0].z);
    glUniform3f(glGetUniformLocation(lighting_shader_.id_, "light.Color"), light_positions_[0].x,
                light_positions_[0].y, light_positions_[0].z);

    // Update attenuation parameters
    const float linear = 0.09f;
    const float quadratic = 0.032f;

    glUniform1f(glGetUniformLocation(lighting_shader_.id_, "light.Linear"), linear);
    glUniform1f(glGetUniformLocation(lighting_shader_.id_, "light.Quadratic"), quadratic);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_position_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, g_normal_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, g_albedo_);
    glActiveTexture(GL_TEXTURE3); //
    glBindTexture(GL_TEXTURE_2D, ssao_color_buffer_blur_);
    renderQuad();
    //-------------------------------------------------------------------------------

  }

  shader_light_.Use();
  shader_light_.SetMat4("projection", projection);
  shader_light_.SetMat4("view", view);
  for (unsigned int i = 0; i < light_positions_.size(); i++) {
    model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(light_positions_[i]));
    model = glm::scale(model, glm::vec3(0.25f));
    shader_light_.SetMat4("model", model);
    shader_light_.SetVec3("lightColor", light_colors_[i]);
    renderCube();
  }
  glBindVertexArray(0);


  bool horizontal = true, first_iteration = true;
  unsigned int amount = 10;
  shader_blur_.Use();
  for (unsigned int i = 0; i < amount; i++) {
    glBindFramebuffer(GL_FRAMEBUFFER, pingpong_fbo_[horizontal]);
    shader_blur_.SetInt("horizontal", horizontal);
    glBindTexture(GL_TEXTURE_2D, first_iteration ? color_buffer_[1] : pingpong_color_buffer_[!horizontal]);
    renderQuad();
    horizontal = !horizontal;
    if (first_iteration)
      first_iteration = false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0);


  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  shader_bloom_final_.Use();
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color_buffer_[0]);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, pingpong_color_buffer_[!horizontal]);
  shader_bloom_final_.SetInt("bloom", bloom_state_);
  shader_bloom_final_.SetFloat("exposure", exposure_);
  shader_bloom_final_.SetFloat("gamma", gamma_);
  renderQuad();
}

void Scene3D::OnEvent(const SDL_Event& event)
{
  switch (event.type)
  {
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_LSHIFT)
      {
        camera_.ToggleSprint();
      }
      break;
    default:
      break;
  }
}

void Scene3D::UpdateCamera(const float dt)
{
  // Get keyboard state
  const Uint8* state = SDL_GetKeyboardState(NULL);

  // Camera controls
  if (state[SDL_SCANCODE_W])
  {
    camera_.Move(FORWARD, dt);
  }
  if (state[SDL_SCANCODE_S])
  {
    camera_.Move(BACKWARD, dt);
  }
  if (state[SDL_SCANCODE_A])
  {
    camera_.Move(LEFT, dt);
  }
  if (state[SDL_SCANCODE_D])
  {
    camera_.Move(RIGHT, dt);
  }
  if (state[SDL_SCANCODE_SPACE])
  {
    camera_.Move(UP, dt);
  }
  if (state[SDL_SCANCODE_LCTRL])
  {
    camera_.Move(DOWN, dt);
  }

  int mouseX, mouseY;
  const Uint32 mouseState = SDL_GetRelativeMouseState(&mouseX, &mouseY);
  if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT) && !ImGui::GetIO().WantCaptureMouse)
  {
    camera_.Update(mouseX, mouseY);
  }
}


void Scene3D::DrawImGui()
{
  ImGui::Begin("My Window"); // Start a new window

  //ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

  if (ImGui::Checkbox("Enable bloom", &bloom_state_)) {
    if (bloom_state_) {
      ssao_state_ = false;
    }
  }

  if (ImGui::Checkbox("Enable ssao", &ssao_state_)) {
    if (ssao_state_) {
      bloom_state_ = false;
      Normal_state_ = false;
    }
  }

  if (ImGui::Checkbox("Enable Normal", &Normal_state_)) {
    if (Normal_state_) {
      ssao_state_ = false;
    }
  }

  ImGui::SliderFloat("Exposure", &exposure_, 0.01f, 10.0f, "%.1f");
  ImGui::SliderFloat("gamma", &gamma_, 0.01f, 10.0f, "%.1f");
  ImGui::SliderFloat("fovY", &fovY, 0.01f, 3.0f, "%.1f");
  ImGui::SliderFloat("zNear", &zNear, 0.01f, 10.0f, "%.1f");
  ImGui::SliderFloat("zFar", &zFar, 0.01f, 1000.0f, "%.1f");
  ImGui::SliderFloat("aspect", &aspect, 0.01f, 5.0f, "%.1f");

  static const char* resolution_items[] = {
      "4/3 (800x600)",
      "16/9 (1280x720)",
      "16/10 (1280x800)",
      "1200x720",
      "Custom"
  };
  static int current_resolution = 3;



  if (ImGui::Combo("My Window -", &current_resolution, resolution_items, IM_ARRAYSIZE(resolution_items)))
  {

    switch (current_resolution)
    {
      case 0: // 4/3 (800x600)
        aspect_h = 800.0f;
        aspect_v = 600.0f;
        break;
      case 1: // 16/9 (1280x720)
        aspect_h = 1280.0f;
        aspect_v = 720.0f;
        break;
      case 2: // 16/10 (1280x800)
        aspect_h = 1280.0f;
        aspect_v = 800.0f;
        break;
      case 3: // 1200x720
        aspect_h = 1200.0f;
        aspect_v = 720.0f;
        break;
      case 4: // Custom
        // Ne change rien ici pour permettre la personnalisation via les sliders
        break;
    }
  }

  if (current_resolution == 4)
  {
    ImGui::SliderFloat("Width", &aspect_h, 100.0f, 2500.0f, "%.1f");
    ImGui::SliderFloat("Height", &aspect_v, 100.0f, 2500.0f, "%.1f");
  }




  if (ImGui::CollapsingHeader("Normal Settings")) {
    ImGui::SliderFloat("Normal_X", &Normal_x, -30.01f, 30.0f, "%.1f");
    ImGui::SliderFloat("Normal_Y", &Normal_y, -30.01f, 30.0f, "%.1f");
    ImGui::SliderFloat("Normal_Z", &Normal_z, -30.01f, 30.0f, "%.1f");
    ImGui::SliderFloat("Normal_Angle", &Normal_Rotation_angle, -360.01f, 360.0f, "%.1f");
  }
  if (ImGui::CollapsingHeader("light Settings")) {
    if (ImGui::CollapsingHeader("light 1")) {
      ImGui::DragFloat3("Light Position 0", glm::value_ptr(lightPos0), 0.1f);
      ImGui::ColorPicker3("Light Colour", reinterpret_cast<float *>(&lightColor0));
    }
    if (ImGui::CollapsingHeader("light 2")) {
      ImGui::DragFloat3("Light Position 1", glm::value_ptr(lightPos1), 0.1f);
      ImGui::ColorPicker3("Light Colour", reinterpret_cast<float *>(&lightColor1));
    }
    if (ImGui::CollapsingHeader("light 3")) {
      ImGui::DragFloat3("Light Position 2", glm::value_ptr(lightPos2), 0.1f);
      ImGui::ColorPicker3("Light Colour", reinterpret_cast<float *>(&lightColor2));
    }
    if (ImGui::CollapsingHeader("light 4")) {
      ImGui::DragFloat3("Light Position 3", glm::value_ptr(lightPos3), 0.1f);
      ImGui::ColorPicker3("Light Colour", reinterpret_cast<float *>(&lightColor3));
    }
  }


  // static ImVec4 LightColour = ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // Default color
  // ImGui::ColorPicker3("Light Colour", reinterpret_cast<float*>(&light_colors_[0]));
  ImGui::End(); // End the window
}
}


int main(int argc, char* argv[])
{
  gpr5300::Scene3D scene;
  gpr5300::Engine engine(&scene);
  engine.Run();

  return EXIT_SUCCESS;
}

