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


  float aspect_v = 720.0f;
  float aspect_h = 1280.0f;
  float aspect = aspect_h / aspect_v;


  float fovY = glm::radians(45.0f);
  float zNear = 0.1f;
  float zFar = 1000.0f;


  //bloom
  Shader shader_light_ = {};
  Shader shader_blur_ = {};
  Shader shader_bloom_final_ = {};

  GLuint hdr_fbo_ = 0;
  GLuint color_buffer_[2] = {};
  GLuint rbo_depth_ = 0;

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

  bool Normal_state_ = false;

  float model_scale_ = 0.05;
  float model_scale_2_ = 0.1;
  float Normal_x = 0.1;
  float Normal_y = 0.1;
  float Normal_z = 12.1;
  float Normal_Rotation_angle = 180.1;
  float scaleFactor_instancing = 0.1f;
  float gamma_ = 2.2f;
  bool bloom_state_ = false;
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
  for (unsigned int i = 0; i < Instancing_amout; i++)
  {
    glm::mat4 model = glm::mat4(1.0f);
    float baseAngle = glm::radians((float)i / (float)Instancing_amout * 360.0f);
    float displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
    float x = sin(baseAngle) * radius + displacement;
    displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
    float y = displacement * 0.4f;
    displacement = (rand() % (int)(2 * offset * 100)) / 100.0f - offset;
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





  // Mise à jour de la caméra et du temps écoulé
  UpdateCamera(dt);
  elapsedTime_ += dt;

  // Effacer l'écran
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // 1. Rendu de la scène dans le framebuffer HDR
  glBindFramebuffer(GL_FRAMEBUFFER, hdr_fbo_);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Calcul des matrices de projection et vue
  auto projection = glm::perspective(fovY, aspect, zNear, zFar);
  auto view = camera_.view();
  auto model = glm::mat4(1.0f);

  // (Optionnel) Vous pouvez activer ici d'autres textures ou uniformes si nécessaire…
  glActiveTexture(GL_TEXTURE0);

  glBindFramebuffer(GL_FRAMEBUFFER, 1);
  glDepthFunc(GL_LEQUAL);    // Pour s'assurer que le skybox est dessiné derrière tout le reste
  glDepthMask(GL_FALSE);     // Désactiver l'écriture dans le buffer de profondeur

  skybox_program_.Use();
  // On retire la composante translation de la vue pour le skybox
  glm::mat4 viewS = glm::mat4(glm::mat3(view));
  skybox_program_.SetMat4("view", viewS);
  skybox_program_.SetMat4("projection", projection);

  glBindVertexArray(skybox_vao_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindVertexArray(0);

  glDepthMask(GL_TRUE);   // Réactiver l'écriture dans le buffer de profondeur
  glDepthFunc(GL_LESS);   // Restaurer la fonction de profondeur par défaut



  // On utilise ici le shader 'shader_model_' qui doit gérer le normal mapping
  shader_model_.Use();
  shader_model_.SetMat4("projection", projection);
  shader_model_.SetMat4("view", view);

  // Mise à jour du frustum et envoi des informations de lumière
  frustum_.CreateFrustumFromCamera(camera_, aspect, fovY, zNear, zFar);
  glm::mat4 projView = projection * camera_.GetViewMatrix();
  frustum_.Update(projView);
  shader_model_.SetVec3Array("lightPos", light_positions_, light_positions_.size());
  shader_model_.SetVec3Array("lightColor", light_colors_, light_colors_.size());
  shader_model_.SetVec3("viewPos", camera_.camera_position_);


  // Rendu du premier modèle (model_) avec normal mapping
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
  //glDisable((GL_CULL_FACE));
  normal_renderQuad();
  glEnable(GL_CULL_FACE);

  //----------------------------------------------------------------------------------------------

  }

  glBindVertexArray(0);


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
  glBindVertexArray(0);

  // Rendu des sources lumineuses (affichage de cubes lumineux)
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

  // 4. Application d'un flou gaussien pour l'effet bloom
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





  // 5. Rendu final : affichage du framebuffer HDR avec tonemapping et bloom
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

  ImGui::Checkbox("Enable bloom", &bloom_state_);
  ImGui::Checkbox("Enable Normal", &Normal_state_);

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
  static int current_resolution = 39; // On démarre par exemple sur 16/9



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

