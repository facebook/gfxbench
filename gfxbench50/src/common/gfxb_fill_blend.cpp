/*
 * Copyright (c) 2005-2025, Kishonti Ltd
 * SPDX-License-Identifier: BSD-3-Clause
 * This file is part of GFXBench. See the top-level LICENSE file for details.
 */

#include "gfxb_fill_blend.h"

#include <algorithm>
#include <string>

#include <components/input_component.h>
#include <ngl.h>
#include <ng/log.h>
#include <kcl_os.h>
#include "gfxb_shader.h"
#include "gfxb_barrier.h"

using namespace GFXB;

namespace
{
    constexpr KCL::uint32 kQuadCount = 10;
    constexpr const char* kShaderVert = "fill_blend.vert";
    constexpr const char* kShaderFrag = "fill_blend.frag";
    constexpr const char* kShaderSubdir = "shaders/fill_blend/";

    // Run for a fixed duration; actual stop handled by play_time in configs.
    constexpr KCL::uint32 kDefaultBuffersPerFrame = 1;
    constexpr KCL::uint32 kDefaultPrerenderedFrames = 2;
}

FillBlendTest::FillBlendTest()
{
    m_command_buffers_per_frame = kDefaultBuffersPerFrame;
    m_prerendered_frame_count = kDefaultPrerenderedFrames;
    m_command_buffer = 0;
    m_active_backbuffer_id = 0;
    m_shader = 0;
}

FillBlendTest::~FillBlendTest()
{
    DestroyPipelines();
}

KCL::KCL_Status FillBlendTest::Init()
{
    m_shapes.Init();
    m_backbuffers = m_screen_manager_component->GetBackbuffers();
    m_active_backbuffer_id = m_screen_manager_component->GetActiveBackbufferId();
    const std::string bb_count = std::to_string(m_backbuffers.size());
    const std::string active_bb = std::to_string(m_active_backbuffer_id);
    NGLOG_INFO("FillBlend v2: Init backbuffers=%s active=%s", bb_count.c_str(), active_bb.c_str());

    auto status = InitPipelines();
    if (status != KCL::KCL_TESTERROR_NOERROR)
    {
        NGLOG_ERROR("FillBlend: InitPipelines failed (%d)", status);
        return status;
    }

    // Nothing else to initialize
    return KCL::KCL_TESTERROR_NOERROR;
}

KCL::KCL_Status FillBlendTest::Warmup()
{
    // no-op warmup
    return KCL::KCL_TESTERROR_NOERROR;
}

void FillBlendTest::Animate()
{
    // keep simple; no animation
}

void FillBlendTest::Render()
{
    if (m_render_jobs.empty() || m_shader == 0)
    {
        NGLOG_ERROR("FillBlend: Render skipped (jobs=%s shader=%s)",
            std::to_string(m_render_jobs.size()).c_str(),
            std::to_string(m_shader).c_str());
        return;
    }

    // Use the currently active backbuffer provided by the screen manager
    m_active_backbuffer_id = m_screen_manager_component->GetActiveBackbufferId();
    const KCL::uint32 job_index = m_active_backbuffer_id % static_cast<KCL::uint32>(m_render_jobs.size());
    const KCL::uint32 render_job = m_render_jobs[job_index];
    const KCL::uint32 command_buffer = GetLastCommandBuffer();

    const std::string cb_str = std::to_string(command_buffer);
    const std::string job_str = std::to_string(render_job);
    const std::string fb_str = std::to_string(m_backbuffers[job_index]);
    NGLOG_INFO("FillBlend v2: Render begin cb=%s job=%s backbuffer=%s", cb_str.c_str(), job_str.c_str(), fb_str.c_str());
    nglBeginCommandBuffer(command_buffer);

    const void* uniforms[4] = {};

    // Configure blend and depth state per draw
    int32_t viewport[4] = {0, 0, (int32_t)m_test_width, (int32_t)m_test_height};
    nglViewportScissor(render_job, viewport, viewport);

    // Ensure we render into the active backbuffer
    Transitions::Get()
        .TextureBarrier(m_backbuffers[job_index], NGL_COLOR_ATTACHMENT)
        .Execute(command_buffer);

    m_stat = NGLStatistic{};
    nglBegin(render_job, command_buffer);
    nglBlendStateAll(render_job, NGL_BLEND_ALFA);
    nglDepthState(render_job, NGL_DEPTH_ALWAYS, false);

    // Draw the quads
    for (KCL::uint32 i = 0; i < kQuadCount; ++i)
    {
        // For simplicity, draw the same fullscreen quad repeatedly; driver should still process blend ops.
        nglDrawTwoSided(render_job, m_shader, m_shapes.m_fullscreen_vbid, m_shapes.m_fullscreen_ibid, uniforms);
    }

    nglEnd(render_job);
    NGLOG_INFO("FillBlend: Render end");
}

void FillBlendTest::HandleUserInput(GLB::InputComponent* input_component, float /*frame_time_secs*/)
{
    if (input_component && input_component->IsKeyPressed('R'))
    {
        DestroyPipelines();
        InitPipelines();
    }
}

void FillBlendTest::GetCommandBufferConfiguration(KCL::uint32& buffers_in_frame, KCL::uint32& prerendered_frame_count)
{
    buffers_in_frame = m_command_buffers_per_frame;
    prerendered_frame_count = m_prerendered_frame_count;
}

void FillBlendTest::SetCommandBuffers(const std::vector<KCL::uint32>& buffers)
{
    if (!buffers.empty())
    {
        m_command_buffer = buffers[0];
    }
}

KCL::uint32 FillBlendTest::GetLastCommandBuffer()
{
    return m_command_buffer;
}

KCL::KCL_Status FillBlendTest::InitPipelines()
{
    DestroyPipelines();

    // Ensure shader factory exists and is configured the same way as scene5 init.
    ShaderFactory::CreateInstance();
    auto *shader_factory = ShaderFactory::GetInstance();
    shader_factory->ClearGlobals();
    shader_factory->AddShaderSubdirectory("shaders/scene5");
    shader_factory->AddShaderSubdirectory("shaders/");
    shader_factory->AddShaderSubdirectory(kShaderSubdir);
    // Keep shaders minimal; avoid pulling the large scene5 common header.

    std::stringstream header;
    switch (nglGetApi())
    {
    case NGL_OPENGL:
        header << "#version 430 core\n";
        shader_factory->AddGlobalDefine("SHADER_GLSL");
        break;
    case NGL_OPENGL_ES:
        header << "#version 310 es\n"
               << "precision highp float;\n"
               << "precision highp sampler2D;\n"
               << "precision highp sampler2DArray;\n"
               << "precision highp samplerCube;\n";
        shader_factory->AddGlobalDefine("SHADER_GLSL");
        break;
    case NGL_VULKAN:
        header << "#version 310 es\n"
               << "#extension GL_OES_shader_io_blocks : require\n";
        shader_factory->AddGlobalDefine("SHADER_VULKAN");
        break;
    default:
        break;
    }
    shader_factory->SetGlobalHeader(header.str().c_str());

    switch (nglGetInteger(NGL_RASTERIZATION_CONTROL_MODE))
    {
    case NGL_ORIGIN_LOWER_LEFT:
        shader_factory->AddGlobalDefine("NGL_ORIGIN_LOWER_LEFT");
        break;
    case NGL_ORIGIN_UPPER_LEFT:
        shader_factory->AddGlobalDefine("NGL_ORIGIN_UPPER_LEFT");
        break;
    case NGL_ORIGIN_UPPER_LEFT_AND_NDC_FLIP:
        shader_factory->AddGlobalDefine("NGL_ORIGIN_UPPER_LEFT_AND_NDC_FLIP");
        break;
    default:
        break;
    }
    switch (nglGetInteger(NGL_DEPTH_MODE))
    {
    case NGL_ZERO_TO_ONE:
        shader_factory->AddGlobalDefine("NGL_ZERO_TO_ONE");
        break;
    case NGL_NEGATIVE_ONE_TO_ONE:
        shader_factory->AddGlobalDefine("NGL_NEGATIVE_ONE_TO_ONE");
        break;
    default:
        break;
    }

    if (m_backbuffers.empty())
    {
        m_backbuffers.push_back(0);
    }

    m_render_jobs.resize(m_backbuffers.size(), 0);

    // Create one job per backbuffer so we can render into the currently active one.
    for (size_t i = 0; i < m_backbuffers.size(); ++i)
    {
        NGL_job_descriptor jd;
        {
            NGL_attachment_descriptor ad;
            ad.m_attachment.m_idx = m_backbuffers[i];
            ad.m_attachment_load_op = NGL_LOAD_OP_CLEAR;
            ad.m_attachment_store_op = NGL_STORE_OP_STORE;
            jd.m_attachments.push_back(ad);
        }
        {
            NGL_subpass sp;
            sp.m_name = "fill_blend";
            sp.m_usages.push_back(NGL_COLOR_ATTACHMENT);
            jd.m_subpasses.push_back(sp);
        }
        // Use the shared shader loader so NGL can compile the shader pair.
        jd.m_load_shader_callback = LoadShader;

        m_render_jobs[i] = nglGenJob(jd);
        const std::string job_str = std::to_string(m_render_jobs[i]);
        const std::string bb_str = std::to_string(m_backbuffers[i]);
        const std::string idx_str = std::to_string(i);
        NGLOG_INFO("FillBlend v2: Created job %s for backbuffer %s (index %s)", job_str.c_str(), bb_str.c_str(), idx_str.c_str());
    }

    // Simple shader that outputs white
    ShaderDescriptor sd(kShaderVert, kShaderFrag);
    m_shader = ShaderFactory::GetInstance()->AddDescriptor(sd);
    NGLOG_INFO("FillBlend v2: shader code %s", std::to_string(m_shader).c_str());

    const bool has_valid_job = !m_render_jobs.empty();
    if (!has_valid_job || m_shader == 0)
    {
        NGLOG_ERROR("FillBlendTest v2: failed to create job or shader (has_valid_job=%s, shader=%s)",
            has_valid_job ? "true" : "false",
            std::to_string(m_shader).c_str());
        return KCL::KCL_TESTERROR_UNKNOWNERROR;
    }

    NGLOG_INFO("FillBlend: InitPipelines ready shader=%s", std::to_string(m_shader).c_str());
    return KCL::KCL_TESTERROR_NOERROR;
}

void FillBlendTest::DestroyPipelines()
{
    for (auto& job : m_render_jobs)
    {
        if (job)
        {
            nglDeletePipelines(job);
            job = 0;
        }
    }
    m_render_jobs.clear();
    // ShaderFactory manages lifetimes; no delete here.
}
