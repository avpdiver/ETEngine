#pragma once

#include "OutlineRenderer.h"
#include "ShadowRenderer.h"
#include "DebugRenderer.h"
#include "TextRenderer.h"
#include "SpriteRenderer.h"
#include "OverlayRenderer.h"

#include <EtCore/Helper/Singleton.h>
#include <EtCore/Content/AssetPointer.h>

#include <Engine/Graphics/SpriteFont.h>
#include <Engine/Graphics/TextureData.h>
#include <Engine/Graphics/Camera.h>
#include <Engine/GraphicsContext/ViewportRenderer.h>


class PostProcessingRenderer;
class ScreenSpaceReflections;


//---------------------------------
// SceneRenderer
//
// Renders a Scene to the viewport
//
class SceneRenderer final : public I_ViewportRenderer, public I_ShadowRenderer, public render::I_OverlayRenderer
{
	// GlobalAccess
	//---------------
	static std::vector<SceneRenderer*> s_AllSceneRenderers;

public:
	static SceneRenderer* GetCurrent();
	static std::vector<SceneRenderer*>& GetAll() { return s_AllSceneRenderers; }

	// construct destruct
	//--------------------
	SceneRenderer();
	~SceneRenderer();

	// functionality
	//-----------------------------
	void InitWithSplashScreen();
	void InitRenderingSystems();

	void ShowSplashScreen();
	void HideSplashScreen();

private:
	void Draw();

	// Shadow Renderer Interface
	//-----------------------------
public:
	void DrawShadow(NullMaterial* const nullMaterial) override;

	Camera const& GetCamera() const override { return m_Camera; }

	// Overlay Renderer Interface
	//-----------------------------
	void DrawOverlays(T_FbLoc const targetFb) override;

	// Viewport Renderer Interface
	//-----------------------------
protected:
	std::type_info const& GetType() const override { return typeid(SceneRenderer); }
	void OnInit() override {}
	void OnDeinit() override {}
	void OnResize(ivec2 const dim) override;
	void OnRender(T_FbLoc const targetFb) override;

	// accessors
	//--------------
public:
	Camera& GetCamera() { return m_Camera; }

	std::vector<AbstractScene*> const& GetRenderScenes() { return m_RenderScenes; }

	Gbuffer* GetGBuffer() { return m_GBuffer; }
	ShadowRenderer& GetShadowRenderer() { return m_ShadowRenderer; }
	TextRenderer& GetTextRenderer() { return m_TextRenderer; }
	SpriteRenderer& GetSpriteRenderer() { return m_SpriteRenderer; }

	DebugRenderer& GetDebugRenderer() { return m_DebugRenderer; }

	// Data
	///////
private:

	bool m_IsInitialized = false;

	// splash screen
	AssetPtr<TextureData> m_SplashBackgroundTex;
	AssetPtr<SpriteFont> m_SplashTitleFont;
	AssetPtr<SpriteFont> m_SplashRegFont;

	bool m_IsShowingSpashScreen = false;

	// scene rendering
	vec3 m_ClearColor;
	ivec2 m_Dimensions;

	Camera m_Camera;

	std::vector<AbstractScene*> m_RenderScenes;
	T_FbLoc m_TargetFb = 0u;

	Gbuffer* m_GBuffer = nullptr;
	ShadowRenderer m_ShadowRenderer;
	PostProcessingRenderer* m_PostProcessing = nullptr;
	ScreenSpaceReflections* m_SSR = nullptr;
	TextRenderer m_TextRenderer;
	SpriteRenderer m_SpriteRenderer;

	DebugRenderer m_DebugRenderer;
};