/**
 * To the extent possible under law, the author has dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide. This software is distributed without any warranty.
 */

#include "Texture.h"

Texture::Texture(const open3d::geometry::Image &image) {
  GLenum type;
  switch (image.bytes_per_channel_) {
  case 1:
    type = GL_UNSIGNED_BYTE;
    break;
  case 4:
    type = GL_FLOAT;
    break;
  default:
    throw std::invalid_argument("Only uint8 and float32 images are supported.");
  }

  GLenum format;
  switch (image.num_of_channels_) {
  case 1:
    format = GL_RED;
    break;
  case 3:
    format = GL_RGB;
    break;
  case 4:
    format = GL_RGBA;
    break;
  default:
    throw std::invalid_argument("Unsupported number of channels.");
  }

  std::vector<uint8_t> buffer;
  buffer.reserve(image.data_.size());
  int stride = image.BytesPerLine();
  const uint8_t *data = image.data_.data() + stride * image.height_;
  for (int i = 0; i < image.height_; i++, data -= stride) {
    buffer.insert(buffer.end(), data, data + stride);
  }

  glGenTextures(1, &mTexture);
  if (!mTexture) {
    throw std::runtime_error("Could not generate the texture.");
  }

  glBindTexture(GL_TEXTURE_2D, mTexture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, format, image.width_, image.height_, 0, format,
               type, buffer.data());
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
}

Texture::Texture(Texture &&other) noexcept {
  mTexture = other.mTexture;
  other.mTexture = 0;
}

Texture &Texture::operator=(Texture &&other) noexcept {
  std::swap(mTexture, other.mTexture);
  return *this;
}

Texture::~Texture() {
  // "glDeleteTextures silently ignores 0's"
  glDeleteTextures(1, &mTexture);
  mTexture = 0;
}

void Texture::bind() const { glBindTexture(GL_TEXTURE_2D, mTexture); }
