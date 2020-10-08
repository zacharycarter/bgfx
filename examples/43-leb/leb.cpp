/*
 * Copyright 2011-2020 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include <bx/uint32_t.h>
#include <bx/allocator.h>
#include <bx/math.h>
#include <vector>
#include <map>

#include "bgfx_utils.h"
#include "camera.h"
#include "common.h"
#include "imgui/imgui.h"

#define CBT_IMPLEMENTATION
#include "cbt.h"

#define LEB_IMPLEMENTATION
#include "leb.h"

namespace
{

	enum
	{
		PROGRAM_TERRAIN,

		SHADING_COUNT
	};

	enum
	{
		PROGRAM_SPLIT,
		PROGRAM_MERGE,
		PROGRAM_INIT_INDIRECT,
		PROGRAM_UPDATE_INDIRECT,
		PROGRAM_UPDATE_DRAW,
		PROGRAM_LEB_REDUCTION_PREPASS,
		PROGRAM_LEB_REDUCTION,

		PROGRAM_COUNT
	};

	enum
	{
		TERRAIN_DMAP_SAMPLER,
		TERRAIN_SMAP_SAMPLER,

		SAMPLER_COUNT
	};

	enum
	{
		TEXTURE_DMAP,
		TEXTURE_SMAP,

		TEXTURE_COUNT
	};

	constexpr int32_t kNumVec4 = 2;

	struct Uniforms
	{
		void init()
		{
			u_params = bgfx::createUniform("u_params", bgfx::UniformType::Vec4, kNumVec4);

			cull = 1;
			freeze = 0;
			gpuSubd = 3;
			passId = 0;
		}

		void submit()
		{
			bgfx::setUniform(u_params, params, kNumVec4);
		}

		void destroy()
		{
			bgfx::destroy(u_params);
		}

		union {
			struct
			{
				float dmapFactor;
				float lodFactor;
				float cull;
				float freeze;

				float gpuSubd;
				float passId;
				float padding1;
				float padding2;
			};

			float params[kNumVec4 * 4];
		};

		bgfx::UniformHandle u_params;
	};

	class ExampleTessellation : public entry::AppI
	{
	public:
		ExampleTessellation(const char *_name, const char *_description, const char *_url)
			: entry::AppI(_name, _description, _url)
		{
		}

		void init(int32_t _argc, const char *const *_argv, uint32_t _width, uint32_t _height) override
		{
			Args args(_argc, _argv);

			m_width = _width;
			m_height = _height;
			m_debug = BGFX_DEBUG_NONE;
			m_reset = BGFX_RESET_VSYNC;

			bgfx::Init init;
			init.type = args.m_type;
			init.vendorId = args.m_pciId;
			init.resolution.width = m_width;
			init.resolution.height = m_height;
			init.resolution.reset = m_reset;
			// init.type = bgfx::RendererType::OpenGL;
			bgfx::init(init);

			m_dmap = {"textures/kauai.png", 1.0f};
			m_shading = PROGRAM_TERRAIN;
			m_primitivePixelLengthTarget = 7.0f;
			m_fovy = 80.0f;
			m_maxDepth = 25;
			m_pingPong = 0;
			m_restart = true;
			m_wireframe = false;
			m_dirty = true;
			m_textures[TEXTURE_DMAP].idx = bgfx::kInvalidHandle;

			m_heightMap = (uint8_t*)BX_ALLOC(entry::getAllocator(), 256*256);
			bx::memSet(m_heightMap, 0, sizeof(uint8_t) * 256 * 256);

			// Enable debug text.
			bgfx::setDebug(m_debug);

			// Set view 0 clear state.
			bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

			bgfx::setViewClear(1, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

			imguiCreate();

			cameraCreate();
			cameraSetPosition({ 256 / 2.0f, 100.0f, 0.0f });
            cameraSetVerticalAngle(-bx::kPiQuarter);

			loadPrograms();
			loadBuffers();
			loadVertexArrays();

			m_dispatchIndirect = bgfx::createIndirectBuffer(2);
		}

		virtual int shutdown() override
		{
			cameraDestroy();
			imguiDestroy();

			m_uniforms.destroy();

			bgfx::destroy(m_dispatchIndirect);

			for (uint32_t i = 0; i < PROGRAM_COUNT; ++i)
			{
				bgfx::destroy(m_programsCompute[i]);
			}

			for (uint32_t i = 0; i < SHADING_COUNT; ++i)
			{
				bgfx::destroy(m_programsDraw[i]);
			}

			for (uint32_t i = 0; i < SAMPLER_COUNT; ++i)
			{
				bgfx::destroy(m_samplers[i]);
			}

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		bool update() override
		{
			if (!entry::processEvents(m_width, m_height, m_debug, m_reset, &m_mouseState))
			{
				int64_t now = bx::getHPCounter();
				static int64_t last = now;
				const int64_t frameTime = now - last;
				last = now;
				const double freq = double(bx::getHPFrequency());
				const float deltaTime = float(frameTime / freq);

				imguiBeginFrame(
					m_mouseState.m_mx
					, m_mouseState.m_my
					, (m_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
					| (m_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
					| (m_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
					, m_mouseState.m_mz
					, uint16_t(m_width)
					, uint16_t(m_height)
				);

				/*showExampleDialog(this);*/

				ImGui::Begin("Settings", NULL, 0);

				if (ImGui::Checkbox("Debug wireframe", &m_wireframe))
				{
					bgfx::setDebug(m_wireframe
						? BGFX_DEBUG_WIREFRAME
						: BGFX_DEBUG_NONE
					);
				}

				ImGui::End();

				if (!ImGui::MouseOverArea())
				{
					// Update camera.
					cameraUpdate(deltaTime * 0.01f, m_mouseState);

					if (!!m_mouseState.m_buttons[entry::MouseButton::Left])
					{
						mousePickTerrain();
					}
				}

				// Update terrain.
				if (m_dirty)
				{
					updateTerrain();
					m_dirty = false;
				}

				bgfx::touch(0);
				bgfx::touch(1);

				configureUniforms();

				cameraGetViewMtx(m_viewMtx);

				float model[16];

				float rotate[16];
				bx::mtxRotateX(rotate, bx::toRad(90));

				float scale[16];
				bx::mtxScale(scale, 256.0f, 32.0f, 256.0f);

				float translate[16];
				bx::mtxTranslate(translate, 0.0, 0.0, 256.0f);

				float tmp[16];
				bx::mtxMul(tmp, rotate, scale);

				
				bx::mtxMul(model, tmp, translate);

				bx::mtxProj(m_projMtx, m_fovy, float(m_width) / float(m_height), 1.0f, 64000.0f, bgfx::getCaps()->homogeneousDepth);

				// Set view 0
				bgfx::setViewRect(0, 0, 0, uint16_t(m_width), uint16_t(m_height));
				bgfx::setViewTransform(0, m_viewMtx, m_projMtx);

				// Set view 1
				bgfx::setViewRect(1, 0, 0, uint16_t(m_width), uint16_t(m_height));
				bgfx::setViewTransform(1, m_viewMtx, m_projMtx);

                m_uniforms.submit();
                
				if (m_restart)
				{
					// Initialize the indirect buffers
					bgfx::setBuffer(0, m_dispatchIndirect, bgfx::Access::Write);
					bgfx::dispatch(0, m_programsCompute[PROGRAM_INIT_INDIRECT], 1, 1, 1);

					m_restart = false;
				}
                else
                {
                    // update indirect
                    bgfx::setBuffer(0, m_bufferLeb, bgfx::Access::Read);
                    bgfx::setBuffer(1, m_dispatchIndirect, bgfx::Access::Write);
                    bgfx::dispatch(0, m_programsCompute[PROGRAM_UPDATE_INDIRECT], 1, 1, 1);
                }

                bgfx::setBuffer(1, m_bufferLeb, bgfx::Access::ReadWrite);
                bgfx::setTransform(model);
                
                bgfx::setTexture(0, m_samplers[TERRAIN_DMAP_SAMPLER], m_textures[TEXTURE_DMAP], BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

                m_uniforms.submit();

                // update the cbt buffer
                bgfx::dispatch(0, m_programsCompute[PROGRAM_SPLIT + m_pingPong], m_dispatchIndirect, 1);

                
                // sum reduction prepass
				int it = m_maxDepth;
				int cnt = ((1 << it) >> 5);
				int numGroup = (cnt >= 256) ? (cnt >> 8) : 1;

				bgfx::setBuffer(0, m_bufferLeb, bgfx::Access::ReadWrite);

                m_uniforms.passId = (float) it;
				m_uniforms.submit();

				bgfx::dispatch(0, m_programsCompute[PROGRAM_LEB_REDUCTION_PREPASS], numGroup, 1, 1);

				it -= 5;

                // sum reduction
				while (--it >= 0)
				{
					cnt = 1 << it;
					numGroup = (cnt >= 256) ? (cnt >> 8) : 1;

					bgfx::setBuffer(0, m_bufferLeb, bgfx::Access::ReadWrite);

                    m_uniforms.passId = (float)it;
					m_uniforms.submit();

					bgfx::dispatch(0, m_programsCompute[PROGRAM_LEB_REDUCTION], numGroup, 1, 1);
				}
                
                // update draw
                bgfx::setBuffer(0, m_bufferLeb, bgfx::Access::Read);
                bgfx::setBuffer(1, m_dispatchIndirect, bgfx::Access::Write);

                m_uniforms.submit();

                bgfx::dispatch(1, m_programsCompute[PROGRAM_UPDATE_DRAW], 1, 1, 1);

				// render terrain
                bgfx::setTexture(0, m_samplers[TERRAIN_DMAP_SAMPLER], m_textures[TEXTURE_DMAP], BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

				bgfx::setTransform(model);
                bgfx::setVertexBuffer(0, m_meshletVertices);
                bgfx::setIndexBuffer(m_meshletIndices);
				bgfx::setBuffer(1, m_bufferLeb, bgfx::Access::Read);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);

				m_uniforms.submit();

				bgfx::submit(1, m_programsDraw[PROGRAM_TERRAIN], m_dispatchIndirect);

				m_pingPong = 1 - m_pingPong;

				imguiEndFrame();

				// Advance to next frame. Rendering thread will be kicked to
				// process submitted rendering primitives.
				bgfx::frame(false);

				return true;
			}

			return false;
		}

		void updateTerrain()
		{
			if (!bgfx::isValid(m_textures[TEXTURE_DMAP]))
			{
				m_textures[TEXTURE_DMAP] = bgfx::createTexture2D(256, 256, false, 1, bgfx::TextureFormat::R8);
			}
			const bgfx::Memory* mem = bgfx::makeRef(&m_heightMap[0], sizeof(uint8_t) * 256 * 256);
			bgfx::updateTexture2D(m_textures[TEXTURE_DMAP], 0, 0, 0, 0, 256, 256, mem);
		}

		void paintTerrainHeight(uint32_t _x, uint32_t _y)
		{
			for (int32_t area_y = -10; area_y < 10; ++area_y)
			{
				for (int32_t area_x = -10; area_x < 10; ++area_x)
				{
					int32_t brush_x = _x + area_x;
					if (brush_x < 0
						|| brush_x >= (int32_t)256)
					{
						continue;
					}

					int32_t brush_y = _y + area_y;
					if (brush_y < 0
						|| brush_y >= (int32_t)256)
					{
						continue;
					}

					uint32_t heightMapPos = (brush_y * 256) + brush_x;
					float height = (float)m_heightMap[heightMapPos];

					// Brush attenuation
					float a2 = (float)(area_x * area_x);
					float b2 = (float)(area_y * area_y);
					float brushAttn = 10 - bx::sqrt(a2 + b2);

					// Raise/Lower and scale by brush power.
					height += 0.0f < bx::clamp(brushAttn * 5, 0.0f, 5.0f) && true
						? 1.0f
						: -1.0f
						;

					m_heightMap[heightMapPos] = (uint8_t)bx::clamp(height, 0.0f, 255.0f);
					m_dirty = true;
				}
			}
		}

		void mousePickTerrain()
		{
			float ray_clip[4];
			ray_clip[0] = ((2.0f * m_mouseState.m_mx) / m_width - 1.0f) * -1.0f;
			ray_clip[1] = ((1.0f - (2.0f * m_mouseState.m_my) / m_height)) * -1.0f;
			ray_clip[2] = -1.0f;
			ray_clip[3] = 1.0f;

			float invProjMtx[16];
			bx::mtxInverse(invProjMtx, m_projMtx);

			float ray_eye[4];
			bx::vec4MulMtx(ray_eye, ray_clip, invProjMtx);
			ray_eye[2] = -1.0f;
			ray_eye[3] = 0.0f;

			float invViewMtx[16];
			bx::mtxInverse(invViewMtx, m_viewMtx);

			float ray_world[4];
			bx::vec4MulMtx(ray_world, ray_eye, invViewMtx);

			const bx::Vec3 rayDir = bx::mul(bx::normalize(bx::load<bx::Vec3>(ray_world)), -1.0f);

			bx::Vec3 pos = cameraGetPosition();
			for (int i = 0; i < 1000; ++i)
			{
				pos = bx::add(pos, rayDir);

				if (pos.x < 0
					|| pos.x >= 256
					|| pos.z < 0
					|| pos.z >= 256)
				{
					continue;
				}

				uint32_t heightMapPos = ((uint32_t)pos.z * 256) + (uint32_t)pos.x;
				if (pos.y < m_heightMap[heightMapPos])
				{
					paintTerrainHeight((uint32_t)pos.x, 256 - (uint32_t)pos.z);
					return;
				}
			}
		}

		void configureUniforms()
		{
			float lodFactor = 2.0f * bx::tan(bx::toRad(m_fovy) / 2.0f)
				/ m_height * (1 << (int)m_uniforms.gpuSubd)
				* m_primitivePixelLengthTarget;

			m_uniforms.lodFactor = -2.0f * bx::log2(lodFactor) + 2.0f;
			m_uniforms.dmapFactor = m_dmap.scale;
		}

		void loadPrograms()
		{
			m_samplers[TERRAIN_DMAP_SAMPLER] = bgfx::createUniform("u_DmapSampler", bgfx::UniformType::Sampler);
			// m_samplers[TERRAIN_SMAP_SAMPLER] = bgfx::createUniform("u_SmapSampler", bgfx::UniformType::Sampler);

			m_uniforms.init();

			m_programsDraw[PROGRAM_TERRAIN] = loadProgram("vs_leb_render", "fs_leb_render");

			m_programsCompute[PROGRAM_SPLIT] = bgfx::createProgram(loadShader("cs_leb_update_split"), true);
            
			m_programsCompute[PROGRAM_MERGE] = bgfx::createProgram(loadShader("cs_leb_update_merge"), true);
			m_programsCompute[PROGRAM_UPDATE_INDIRECT] = bgfx::createProgram(loadShader("cs_leb_update_indirect"), true);
			m_programsCompute[PROGRAM_UPDATE_DRAW] = bgfx::createProgram(loadShader("cs_leb_update_draw"), true);
			m_programsCompute[PROGRAM_INIT_INDIRECT] = bgfx::createProgram(loadShader("cs_leb_init"), true);
			m_programsCompute[PROGRAM_LEB_REDUCTION_PREPASS] = bgfx::createProgram(loadShader("cs_sum_reduction_prepass"), true);
			m_programsCompute[PROGRAM_LEB_REDUCTION] = bgfx::createProgram(loadShader("cs_sum_reduction"), true);
			
		}

		void loadSmapTexture()
		{
			int w = dmap->m_width;
			int h = dmap->m_height;

			const uint16_t *texels = (const uint16_t *)dmap->m_data;

			int mipcnt = dmap->m_numMips;

			const bgfx::Memory *mem = bgfx::alloc(w * h * 2 * sizeof(float));
			float *smap = (float *)mem->data;

			for (int j = 0; j < h; ++j)
			{
				for (int i = 0; i < w; ++i)
				{
					int i1 = bx::max(0, i - 1);
					int i2 = bx::min(w - 1, i + 1);
					int j1 = bx::max(0, j - 1);
					int j2 = bx::min(h - 1, j + 1);
					uint16_t px_l = texels[i1 + w * j]; // in [0,2^16-1]
					uint16_t px_r = texels[i2 + w * j]; // in [0,2^16-1]
					uint16_t px_b = texels[i + w * j1]; // in [0,2^16-1]
					uint16_t px_t = texels[i + w * j2]; // in [0,2^16-1]
					float z_l = (float)px_l / 65535.0f; // in [0, 1]
					float z_r = (float)px_r / 65535.0f; // in [0, 1]
					float z_b = (float)px_b / 65535.0f; // in [0, 1]
					float z_t = (float)px_t / 65535.0f; // in [0, 1]
					float slope_x = (float)w * 0.5f * (z_r - z_l);
					float slope_y = (float)h * 0.5f * (z_t - z_b);

					smap[2 * (i + w * j)] = slope_x;
					smap[1 + 2 * (i + w * j)] = slope_y;
				}
			}

			m_textures[TEXTURE_SMAP] = bgfx::createTexture2D(
				(uint16_t)w, (uint16_t)h, mipcnt > 1, 1, bgfx::TextureFormat::RG32F, BGFX_TEXTURE_NONE, mem);
		}

		void mipmap(uint16_t* source, int size, int channels, uint16_t* dest)
		{
			int mipsize = size / 2;

			for (int y = 0; y < mipsize; ++y)
			{
				for (int x = 0; x < mipsize; ++x)
				{
					for (int c = 0; c < channels; ++c)
					{
						int index = channels * ((y * 2) * size + (x * 2)) + c;
						int sum_value = 4 >> 1;
						sum_value += source[index + channels * (0 * size + 0)];
						sum_value += source[index + channels * (0 * size + 1)];
						sum_value += source[index + channels * (1 * size + 0)];
						sum_value += source[index + channels * (1 * size + 1)];
						dest[channels * (y * mipsize + x) + c] = (uint16_t)(sum_value / 4);
					}
				}
			}
		}

		void loadLebBuffer()
		{
			cbt_Tree *cbt = cbt_CreateAtDepth(m_maxDepth, 1);

			m_bufferLeb = bgfx::createIndexBuffer(
				bgfx::copy(cbt_GetHeap(cbt), (uint32_t) cbt_HeapByteSize(cbt)), BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);

			cbt_Release(cbt);
		}

		/**
        * Load All Buffers
        *
        */
		void loadBuffers()
		{
			loadLebBuffer();
		}

		void loadMeshletVertexArray()
		{
			std::vector<uint16_t> indexBuffer;
			std::vector<bx::Vec3> vertexBuffer;
			std::map<uint32_t, uint16_t> hashMap;
			int lebDepth = 2 * 3;
			int triangleCount = 1 << lebDepth;
			int edgeTessellationFactor = 1 << 3;

			// compute index and vertex buffer
			for (int i = 0; i < triangleCount; ++i)
			{
				cbt_Node node = {(uint64_t)(triangleCount + i), 2 * 3};
				float attribArray[][3] = {{0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}};

				leb_DecodeNodeAttributeArray(node, 2, attribArray);

				for (int j = 0; j < 3; ++j)
				{
					uint32_t vertexID = (uint32_t) (attribArray[0][j] * (edgeTessellationFactor + 1) + attribArray[1][j] * (edgeTessellationFactor + 1) * (edgeTessellationFactor + 1));
					auto it = hashMap.find(vertexID);

					if (it != hashMap.end())
					{
						indexBuffer.push_back(it->second);
					}
					else
					{
						uint16_t newIndex = (uint16_t)vertexBuffer.size();

						indexBuffer.push_back(newIndex);
						hashMap.insert(std::pair<uint32_t, uint16_t>(vertexID, newIndex));
						vertexBuffer.push_back(bx::Vec3(attribArray[0][j], attribArray[1][j], 0.0f));
					}
				}
			}

			m_meshletLayout
				.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.end();

			m_meshletVertices = bgfx::createVertexBuffer(
				bgfx::copy(&vertexBuffer[0], (uint32_t)(sizeof(bx::Vec3) * vertexBuffer.size())), m_meshletLayout);

			m_meshletIndices = bgfx::createIndexBuffer(
				bgfx::copy(&indexBuffer[0], (uint32_t)(sizeof(uint16_t) * indexBuffer.size())));
		}

		/**
        * Load All Buffers
        *
        */
		void loadVertexArrays()
		{
			loadMeshletVertexArray();
		}

		Uniforms m_uniforms;

		bgfx::ProgramHandle m_programsCompute[PROGRAM_COUNT];
		bgfx::ProgramHandle m_programsDraw[SHADING_COUNT];
		bgfx::TextureHandle m_textures[TEXTURE_COUNT];
		bgfx::UniformHandle m_samplers[SAMPLER_COUNT];

		bgfx::IndexBufferHandle m_bufferLeb;

		bgfx::IndexBufferHandle m_meshletIndices;
		bgfx::VertexBufferHandle m_meshletVertices;
		bgfx::VertexLayout m_meshletLayout;

		bgfx::IndirectBufferHandle m_dispatchIndirect;

		bimg::ImageContainer *dmap;

		float m_viewMtx[16];
		float m_projMtx[16];

		entry::MouseState m_mouseState;

		uint32_t m_width;
		uint32_t m_height;
		uint32_t m_debug;
		uint32_t m_reset;

		struct DMap
		{
			bx::FilePath pathToFile;
			float scale;
		};

		DMap m_dmap;

		int m_shading;
		int m_maxDepth;
		int m_pingPong;

		float m_primitivePixelLengthTarget;
		float m_fovy;

		bool m_restart;
		bool m_wireframe;
		bool m_dirty;

		uint8_t* m_heightMap;
	};

} // namespace

ENTRY_IMPLEMENT_MAIN(
	ExampleTessellation, "41-tess", "Adaptive GPU Tessellation.", "https://bkaradzic.github.io/bgfx/examples.html#tess");
