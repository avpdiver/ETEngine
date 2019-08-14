#pragma once

#include <EtCore/Content/AssetStub.h>
#include <Engine/Graphics/Shader.h>
#include <Engine/Audio/AudioData.h>

//---------------------------------
// ForceLinking
//
// Add classes here that the linker thinks wouldn't be used by this project but are in fact used by reflection
//
void ForceLinking()
{
	FORCE_LINKING(StubAsset)
	FORCE_LINKING(ShaderAsset)
	FORCE_LINKING(AudioAsset)
}

