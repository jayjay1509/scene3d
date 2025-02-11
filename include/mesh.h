#ifndef MESH_H
#define MESH_H

#include <string>
#include <vector>
#include <GL/glew.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// Définition de la structure de sommet incluant la position, la normale,
// les coordonnées de texture et la tangente pour le normal mapping.
struct Vertex {
  glm::vec3 Position;
  glm::vec3 Normal;
  glm::vec2 TexCoords;
  glm::vec3 Tangent;
};

struct Texture {
  unsigned int id;
  std::string type;
  std::string path;
};

class Mesh {
 public:
  // Données du mesh
  std::vector<Vertex> vertices_;
  std::vector<unsigned int> indices_;
  std::vector<Texture> textures_;

  // Retourne le VAO du mesh
  [[nodiscard]] unsigned int VAO() const { return VAO_; }

  // Constructeur : stocke les données du mesh et initialise le VAO/VBO/EBO.
  Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures)
  {
    this->vertices_ = vertices;
    this->indices_ = indices;
    this->textures_ = textures;

    SetupMesh();
  }

  // Fonction de dessin : lie les textures, puis dessine le mesh
  void Draw(GLuint& shader)
  {
    unsigned int diffuseNr = 1;
    unsigned int specularNr = 1;
    for (unsigned int i = 0; i < textures_.size(); i++)
    {
      glActiveTexture(GL_TEXTURE0 + i); // Activation de l'unité de texture appropriée
      // Récupération du numéro de la texture en fonction de son type
      std::string number;
      std::string name = textures_[i].type;
      if (name == "texture_diffuse")
        number = std::to_string(diffuseNr++);
      else if (name == "texture_specular")
        number = std::to_string(specularNr++);

      // Envoi de l'uniforme au shader
      glUniform1i(glGetUniformLocation(shader, ("material." + name).append(number).c_str()), i);
      glBindTexture(GL_TEXTURE_2D, textures_[i].id);
    }
    glActiveTexture(GL_TEXTURE0);

    // Dessin du mesh
    glBindVertexArray(VAO_);
    glDrawElements(GL_TRIANGLES, indices_.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
  }

  // Retourne les sommets du mesh
  const std::vector<Vertex>& get_vertices() const
  {
    return vertices_;
  }
  unsigned int VAO_, VBO_, EBO_;
 private:
  // Données de rendu


  // Configuration du VAO, VBO et EBO et des attributs de sommet
  void SetupMesh()
  {
    glGenVertexArrays(1, &VAO_);
    glGenBuffers(1, &VBO_);
    glGenBuffers(1, &EBO_);

    glBindVertexArray(VAO_);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_);
    glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof(Vertex), &vertices_[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof(unsigned int),
                 &indices_[0], GL_STATIC_DRAW);

    // Configuration des attributs de sommet :

    // Positions (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

    // Normales (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // Coordonnées de texture (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    // Tangentes (location = 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));

    glBindVertexArray(0);
  }
};

#endif // MESH_H
