#pragma once

/*  This file is part of EmuFramework.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with EmuFramework.  If not, see <http://www.gnu.org/licenses/> */

#include <imagine/gfx/GfxSprite.hh>
#include <imagine/pixmap/PixelFormat.hh>
#include <emuframework/VideoImageOverlay.hh>
#include <emuframework/VideoImageEffect.hh>

struct AppWindowData;
class EmuInputView;
class EmuVideo;

class EmuVideoLayer
{
public:
	EmuVideoLayer(EmuVideo &video);
	void place(const IG::WindowRect &viewportRect, const Gfx::ProjectionPlane &projP, EmuInputView *inputView);
	void draw(Gfx::RendererCommands &cmds, const Gfx::ProjectionPlane &projP);
	void setOverlay(uint effect);
	void setOverlayIntensity(Gfx::GC intensity);
	void setEffect(uint effect, IG::PixelFormatID fmt);
	void setEffectFormat(IG::PixelFormatID fmt);
	void setLinearFilter(bool on);
	void resetImage();
	void setBrightness(float b);
	void setTextureBufferMode(Gfx::TextureBufferMode mode);
	EmuVideo &emuVideo() const { return video; }

	const IG::WindowRect &gameRect() const
	{
		return gameRect_;
	}

private:
	VideoImageOverlay vidImgOverlay{};
	#ifdef CONFIG_GFX_OPENGL_SHADER_PIPELINE
	VideoImageEffect vidImgEffect{};
	#endif
	EmuVideo &video;
	Gfx::Sprite disp{};
	IG::WindowRect gameRect_{};
	Gfx::GCRect gameRectG{};
	float brightness = 1.f;
	bool useLinearFilter = true;

	void compileDefaultPrograms();
	void placeEffect();
	void placeOverlay();
};
