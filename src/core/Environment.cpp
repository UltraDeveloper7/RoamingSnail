#include "../precompiled.h"
#include "Environment.hpp"
#include "../core/Loader.hpp"

Environment::Environment() : fbo_{}, rbo_{},
cube_map_shader_(std::make_unique<Shader>(Config::cubemap_vertex_path, Config::cubemap_fragment_path)),
brdf_shader_(std::make_unique<Shader>(Config::brdf_vertex_path, Config::brdf_fragment_path)),
irradiance_shader_(std::make_unique<Shader>(Config::cubemap_vertex_path, Config::irradiance_fragment_path)),
prefilter_shader_(std::make_unique<Shader>(Config::cubemap_vertex_path, Config::prefilter_fragment_path))
{
    CreateCube();
    CreateQuad();
    CreateBuffers();

    const glm::mat4 capture_projection = glm::perspective(glm::half_pi<float>(), 1.0f, Config::near_clip, Config::far_clip);
    const glm::mat4 capture_views[] =
    {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    hdr_texture_ = Loader::LoadEnvironment(Config::hdr_path);
    cube_map_ = std::make_unique<Texture>(Config::cube_map_size, true);
    RenderCubeMap(capture_projection, capture_views);

    irradiance_map_ = std::make_unique<Texture>(Config::irradiance_scale, false);
    RenderIrradianceMap(capture_projection, capture_views);

    prefilter_map_ = std::make_unique<Texture>(Config::prefilter_scale, true);
    // Ensure all mip levels exist so we can render into them
    glBindTexture(GL_TEXTURE_CUBE_MAP, prefilter_map_->GetId());
    for (unsigned mip = 0; mip < Config::max_mip_levels; ++mip) {
        unsigned w = (unsigned)(Config::prefilter_scale * std::pow(0.5f, (float)mip));
        unsigned h = w;
        for (unsigned face = 0; face < 6; ++face) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, mip,
                GL_RGB16F, (GLsizei)w, (GLsizei)h, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    RenderPrefilterMap(capture_projection, capture_views);

    brdf_lut_ = std::make_unique<Texture>(nullptr, Config::cube_map_size, Config::cube_map_size);
    RenderBrdfLut();
}

void Environment::Prepare() const
{
    glActiveTexture(GL_TEXTURE1);
    irradiance_map_->Bind();
    glActiveTexture(GL_TEXTURE2);
    prefilter_map_->Bind();
    glActiveTexture(GL_TEXTURE3);
    brdf_lut_->Bind();
}

void Environment::Draw(const std::shared_ptr<Shader>& background_shader) const
{
    if (!cube_map_)
    {
        return;
    }

    GLint oldDepthFunc = GL_LESS;
    GLboolean oldDepthMask = GL_TRUE;
    GLint oldActiveTexture = GL_TEXTURE0;

    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    background_shader->Bind();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube_map_->GetId());
    background_shader->SetInt(0, "environmentMap");

    cube_->Bind();
    cube_->Draw();
    cube_->Unbind();

    background_shader->Unbind();

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(oldActiveTexture);

    glDepthMask(oldDepthMask);
    glDepthFunc(oldDepthFunc);
}

void Environment::CreateBuffers()
{
    glGenFramebuffers(1, &fbo_);
    glGenRenderbuffers(1, &rbo_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Config::cube_map_size, Config::cube_map_size);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_);
}

void Environment::CreateCube()
{
    std::vector vertices
    {
        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  0.0f, -1.0f}, glm::vec2{0.0f, 1.0f}),

        Vertex(glm::vec3{-1.0f, -1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f,  1.0f}, glm::vec3{0.0f,  0.0f,  1.0f}, glm::vec2{0.0f, 0.0f}),

        Vertex(glm::vec3{-1.0f,  1.0f,  1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f, -1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f,  1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f,  1.0f}, glm::vec3{-1.0f, 0.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),

        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f, -1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, -1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f, -1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f,  1.0f}, glm::vec3{1.0f,  0.0f,  0.0f}, glm::vec2{0.0f, 0.0f}),

        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f, -1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f,  1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f,  1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f,  1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f, -1.0f}, glm::vec3{0.0f, -1.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),

        Vertex(glm::vec3{-1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  1.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{0.0f,  1.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  1.0f,  0.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f,  1.0f}, glm::vec3{0.0f,  1.0f,  0.0f}, glm::vec2{1.0f, 0.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f, -1.0f}, glm::vec3{0.0f,  1.0f,  0.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f,  1.0f,  1.0f}, glm::vec3{ 0.0f,  1.0f,  0.0}, glm::vec2{0.0f, 0.0f})
    };

    cube_ = std::make_unique<Mesh>(std::move(vertices));
}

void Environment::CreateQuad()
{
    std::vector vertices
    {
        Vertex(glm::vec3{-1.0f,  1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{0.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{-1.0f, -1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{0.0f, 0.0f}),

        Vertex(glm::vec3{-1.0f, -1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{0.0f, 0.0f}),
        Vertex(glm::vec3{ 1.0f,  1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{1.0f, 1.0f}),
        Vertex(glm::vec3{ 1.0f, -1.0f, 0.0f}, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec2{1.0f, 0.0f})
    };

    quad_ = std::make_unique<Mesh>(vertices);
}

void Environment::RenderCubeMap(const glm::mat4& capture_projection, const glm::mat4 capture_views[6]) const
{
    cube_map_shader_->Bind();
    cube_map_shader_->SetInt(1, "equirectangularMap");
    cube_map_shader_->SetMat4(capture_projection, "projection");
    glActiveTexture(GL_TEXTURE1);
    hdr_texture_->Bind();

    glViewport(0, 0, Config::cube_map_size, Config::cube_map_size);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    for (unsigned int i = 0; i < 6; ++i)
    {
        cube_map_shader_->SetMat4(capture_views[i], "view");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cube_map_->GetId(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_->Bind();
        cube_->Draw();
        cube_->Unbind();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    cube_map_->Bind();
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    cube_map_shader_->Unbind();
}

void Environment::RenderIrradianceMap(const glm::mat4& capture_projection, const glm::mat4 capture_views[6]) const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Config::irradiance_scale, Config::irradiance_scale);

    irradiance_shader_->Bind();
    irradiance_shader_->SetInt(1, "environmentMap");
    irradiance_shader_->SetMat4(capture_projection, "projection");

    glActiveTexture(GL_TEXTURE1);
    cube_map_->Bind();

    glViewport(0, 0, Config::irradiance_scale, Config::irradiance_scale);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    for (unsigned int i = 0; i < 6; ++i)
    {
        irradiance_shader_->SetMat4(capture_views[i], "view");
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, irradiance_map_->GetId(), 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube_->Bind();
        cube_->Draw();
        cube_->Unbind();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    irradiance_shader_->Unbind();
}

void Environment::RenderPrefilterMap(const glm::mat4& capture_projection, const glm::mat4 capture_views[6]) const
{
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    prefilter_shader_->Bind();
    prefilter_shader_->SetInt(1, "environmentMap");
    prefilter_shader_->SetMat4(capture_projection, "projection");

    glActiveTexture(GL_TEXTURE1);
    cube_map_->Bind();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    for (unsigned int mip = 0; mip < Config::max_mip_levels; ++mip)
    {
        const unsigned int mip_width = static_cast<unsigned int>(Config::prefilter_scale * std::pow(0.5, mip));
        const unsigned int mip_height = static_cast<unsigned int>(Config::prefilter_scale * std::pow(0.5, mip));

        glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mip_width, mip_height);
        glViewport(0, 0, mip_width, mip_height);

        const float roughness = static_cast<float>(mip) / static_cast<float>(Config::max_mip_levels - 1);
        prefilter_shader_->SetFloat(roughness, "roughness");

        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilter_shader_->SetMat4(capture_views[i], "view");
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, prefilter_map_->GetId(), mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            cube_->Bind();
            cube_->Draw();
            cube_->Unbind();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    prefilter_shader_->Unbind();
}

void Environment::RenderBrdfLut() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Config::cube_map_size, Config::cube_map_size);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdf_lut_->GetId(), 0);

    glViewport(0, 0, Config::cube_map_size, Config::cube_map_size);

    brdf_shader_->Bind();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    quad_->Bind();
    quad_->Draw();
    quad_->Unbind();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    brdf_shader_->Unbind();
}
