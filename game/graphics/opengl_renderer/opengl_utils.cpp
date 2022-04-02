#include "opengl_utils.h"
#include "game/graphics/opengl_renderer/BucketRenderer.h"
#include "common/util/Assert.h"
#include <cstdio>
#include <array>

FramebufferTexturePair::FramebufferTexturePair(int w, int h, u64 texture_format, int num_levels)
    : m_w(w), m_h(h) {
  m_framebuffers.resize(num_levels);
  glGenFramebuffers(num_levels, m_framebuffers.data());
  glGenTextures(1, &m_texture);

  GLint old_framebuffer;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_framebuffer);

  for (int i = 0; i < num_levels; i++) {
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, num_levels);
    glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA, w >> i, h >> i, 0, GL_RGBA, texture_format, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }

  for (int i = 0; i < num_levels; i++) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffers[i]);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    // I don't know if we really need to do this. whatever uses this texture should figure it out.

    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, m_texture, i);
    GLenum draw_buffers[1] = {GLenum(GL_COLOR_ATTACHMENT0 + i)};
    glDrawBuffers(1, draw_buffers);
    auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
          printf("GL_FRAMEBUFFER_UNDEFINED\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
          printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
          printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
          printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
          printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER\n");
          break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
          printf("GL_FRAMEBUFFER_UNSUPPORTED\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
          printf("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE\n");
          break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
          printf("GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS\n");
          break;
      }

      ASSERT(false);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, old_framebuffer);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, old_framebuffer);
}

FramebufferTexturePair::~FramebufferTexturePair() {
  glDeleteFramebuffers(m_framebuffers.size(), m_framebuffers.data());
  glDeleteTextures(1, &m_texture);
}

FramebufferTexturePairContext::FramebufferTexturePairContext(FramebufferTexturePair& fb, int level)
    : m_fb(&fb) {
  glGetIntegerv(GL_VIEWPORT, m_old_viewport);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_old_framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fb->m_framebuffers[level]);
  glViewport(0, 0, m_fb->m_w, m_fb->m_h);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_fb->m_texture, level);
}

void FramebufferTexturePairContext::switch_to(FramebufferTexturePair& fb) {
  if (&fb != m_fb) {
    m_fb = &fb;
    glBindFramebuffer(GL_FRAMEBUFFER, m_fb->m_framebuffers[0]);
    glViewport(0, 0, m_fb->m_w, m_fb->m_h);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_fb->m_texture, 0);
  }
}

FramebufferTexturePairContext::~FramebufferTexturePairContext() {
  glViewport(m_old_viewport[0], m_old_viewport[1], m_old_viewport[2], m_old_viewport[3]);
  glBindFramebuffer(GL_FRAMEBUFFER, m_old_framebuffer);
}

FullScreenDraw::FullScreenDraw() {
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vertex_buffer);
  glBindVertexArray(m_vao);

  struct Vertex {
    float x, y;
  };

  std::array<Vertex, 4> vertices = {
      Vertex{-1, -1},
      Vertex{-1, 1},
      Vertex{1, -1},
      Vertex{1, 1},
  };

  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4, vertices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0,               // location 0 in the shader
                        2,               // 2 floats per vert
                        GL_FLOAT,        // floats
                        GL_TRUE,         // normalized, ignored,
                        sizeof(Vertex),  //
                        nullptr          //
  );

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

FullScreenDraw::~FullScreenDraw() {
  glDeleteVertexArrays(1, &m_vao);
  glDeleteBuffers(1, &m_vertex_buffer);
}

void FullScreenDraw::draw(const math::Vector4f& color,
                          SharedRenderState* render_state,
                          ScopedProfilerNode& prof) {
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertex_buffer);
  auto& shader = render_state->shaders[ShaderId::SOLID_COLOR];
  shader.activate();
  glUniform4f(glGetUniformLocation(shader.id(), "fragment_color"), color[0], color[1], color[2],
              color[3]);
  prof.add_tri(2);
  prof.add_draw_call();
  DrawCall::draw_arrays(GL_TRIANGLE_STRIP, 0, 4);
}

namespace DrawCall {
void draw_arrays(u32 kind, u32 offset, u32 count) {
  ASSERT(count > 0);
  glDrawArrays(kind, offset, count);
}

void draw_elements(u32 kind, u32 count, u32 index_kind, u32 offset) {
  ASSERT(count > 0);
  glDrawElements(kind, count, index_kind, (void*)offset);
}

void multi_draw_elements(u32 kind,
                         GLsizei* counts,
                         u32 index_kind,
                         void** index_offsets,
                         u32 draw_count) {
  ASSERT(draw_count > 0);
  for (u32 i = 0; i < draw_count; i++) {
    ASSERT(counts[i] > 0);
  }
  // glMultiDrawElements(kind, counts, index_kind, index_offsets, draw_count);
  for (u32 i = 0; i < draw_count; i++) {
    glDrawElements(kind, counts[i], index_kind, index_offsets[i]);
  }
}

void multi_draw_elements_verify(u32 kind,
                                GLsizei* counts,
                                u32 index_kind,
                                void** index_offsets,
                                u32 draw_count,
                                u32 idx_buffer_len,
                                u32 vert_buffer_len,
                                const u32* idx_buffer_data) {
  ASSERT(draw_count > 0); // should have at least 1 draw

  for (u32 draw_idx = 0; draw_idx < draw_count; draw_idx++) {
    u64 offset = (u64)(index_offsets[draw_idx]);
    s64 count = counts[draw_idx];
    ASSERT(count >= 0);
    ASSERT((offset % 4) == 0); // should be aligned
    offset /= 4;
    ASSERT(offset < idx_buffer_len);
    ASSERT(offset + count <= idx_buffer_len);

    for (int idx = 0; idx < count; idx++) {
      u32 val = idx_buffer_data[offset + idx];
      if (val == UINT32_MAX) {

      } else if (val >= vert_buffer_len) {
        fmt::print("Verify index failed in multi_draw_elements_verify\n");
        fmt::print("  draw {} / {}\n", draw_idx, draw_count);
        fmt::print("  indices: {} to {}\n", offset, offset + count);
        fmt::print("  index {} of draw ({} in buffer)\n", idx, offset + idx);
        fmt::print("  index value: {}\n", idx_buffer_data[offset + idx]);
        fmt::print("  vertex buffer length: {}\n", vert_buffer_len);
        ASSERT(false);
      }
    }
  }

  multi_draw_elements(kind, counts, index_kind, index_offsets, draw_count);
}

}  // namespace DrawCall