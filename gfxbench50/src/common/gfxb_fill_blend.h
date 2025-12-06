/*
 * Copyright (c) 2005-2025, Kishonti Ltd
 * SPDX-License-Identifier: BSD-3-Clause
 * This file is part of GFXBench. See the top-level LICENSE file for details.
 */

#pragma once

#include <vector>

#include "gfxb_scene_test_base.h"
#include "gfxb_shapes.h"
#include "gfxb_shader.h"

namespace GFXB
{
    // Simple fill/blend test: draws a fixed number of fullscreen quads with alpha blending.
    class FillBlendTest : public SceneTestBase
    {
    public:
        FillBlendTest();
        virtual ~FillBlendTest();

        // SceneTestBase overrides
        virtual KCL::KCL_Status Init() override;
        virtual KCL::KCL_Status Warmup() override;
        virtual void Animate() override;
        virtual void Render() override;
        virtual void HandleUserInput(GLB::InputComponent *input_component, float frame_time_secs) override;

        virtual SceneBase *CreateScene() override { return nullptr; }
        virtual std::string ChooseTextureType() override { return "888"; }
        virtual void GetCommandBufferConfiguration(KCL::uint32 &buffers_in_frame, KCL::uint32 &prerendered_frame_count) override;
        virtual void SetCommandBuffers(const std::vector<KCL::uint32> &buffers) override;
        virtual KCL::uint32 GetLastCommandBuffer() override;

        virtual NGLStatistic& GetFrameStatistics() override { return m_stat; }

    private:
        KCL::KCL_Status InitPipelines();
        void DestroyPipelines();

        KCL::uint32 m_command_buffers_per_frame;
        KCL::uint32 m_prerendered_frame_count;

        KCL::uint32 m_command_buffer;
        KCL::uint32 m_active_backbuffer_id;

        std::vector<KCL::uint32> m_backbuffers;
        std::vector<KCL::uint32> m_render_jobs;
        KCL::uint32 m_shader;

        Shapes m_shapes;

        NGLStatistic m_stat;
    };
}
