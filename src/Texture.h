/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "glad/glad.h"

#include "open3d/geometry/Image.h"

class Texture {
public:
  Texture() = default;
  Texture(const open3d::geometry::Image &image);
  Texture(const Texture &other) = delete;
  Texture &operator=(const Texture &other) = delete;
  Texture(Texture &&other) noexcept;
  Texture &operator=(Texture &&other) noexcept;
  ~Texture();

  void bind() const;

private:
  // https://stackoverflow.com/questions/1108589/is-0-a-valid-opengl-texture-id
  GLuint mTexture = 0;
};
