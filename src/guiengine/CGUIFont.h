// Copyright (C) 2002-2009 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __C_GUI_FONT_H_INCLUDED__
#define __C_GUI_FONT_H_INCLUDED__

#include "IrrCompileConfig.h"

#include "IGUIFontBitmap.h"
#include "irrString.h"
#include "irrMap.h"
#include "IXMLReader.h"
#include "IReadFile.h"
#include "irrArray.h"
#include <map>
#include <iostream>

namespace irr
{

namespace video
{
	class IVideoDriver;
	class IImage;
}

namespace gui
{

	class IGUIEnvironment;

class ScalableFont : public IGUIFontBitmap
{
    float m_scale;
    bool m_shadow;
    irr::video::SColor m_shadow_color;
    
    struct TextureInfo
    {
        irr::core::stringc m_file_name;
        bool m_has_alpha;
        float m_scale;
        
        TextureInfo()
        {
            m_has_alpha = false;
            m_scale = 1.0f;
        }
    };
    
    std::map<int /* texture file ID */, TextureInfo> m_texture_files;
    
    void lazyLoadTexture(int texID);
public:

    bool m_black_border;
    
    ScalableFont* m_fallback_font;
    float         m_fallback_font_scale;
    int           m_fallback_kerning_width;
    
	//! constructor
	ScalableFont(IGUIEnvironment* env, const io::path& filename);

	//! destructor
	virtual ~ScalableFont();

	//! loads a font from a texture file
	bool load(const io::path& filename);

	//! loads a font from a texture file
	bool load(io::IReadFile* file);

	//! loads a font from an XML file
	bool load(io::IXMLReader* xml);

	//! draws an text and clips it to the specified rectangle if wanted
	virtual void draw(const core::stringw& text, const core::rect<s32>& position,
			video::SColor color, bool hcenter=false,
			bool vcenter=false, const core::rect<s32>* clip=0);

	//! returns the dimension of a text
	virtual core::dimension2d<u32> getDimension(const wchar_t* text) const;

	//! Calculates the index of the character in the text which is on a specific position.
	virtual s32 getCharacterFromPos(const wchar_t* text, s32 pixel_x) const;

	//! Returns the type of this font
	virtual EGUI_FONT_TYPE getType() const { return EGFT_BITMAP; }

	//! set an Pixel Offset on Drawing ( scale position on width )
	virtual void setKerningWidth (s32 kerning);
	virtual void setKerningHeight (s32 kerning);

	//! set an Pixel Offset on Drawing ( scale position on width )
	virtual s32 getKerningWidth(const wchar_t* thisLetter=0, const wchar_t* previousLetter=0) const;
	virtual s32 getKerningHeight() const;

    void setShadow(irr::video::SColor col);

	//! gets the sprite bank
	virtual IGUISpriteBank* getSpriteBank() const;

	//! returns the sprite number from a given character
	virtual u32 getSpriteNoFromChar(const wchar_t *c) const;

	virtual void setInvisibleCharacters( const wchar_t *s );

    void setScale(const float scale);
    float getScale() const { return m_scale; }
    
private:

	struct SFontArea
	{
		SFontArea() : underhang(0), overhang(0), width(0), spriteno(0) {}
		s32				underhang;
		s32				overhang;
		s32				width;
		u32				spriteno;
	};
    
    int getCharWidth(const SFontArea& area, const bool fallback) const;

	//! load & prepare font from ITexture
	bool loadTexture(video::IImage * image, const io::path& name);

	void readPositions(video::IImage* texture, s32& lowerRightPositions);

	s32 getAreaFromCharacter (const wchar_t c, bool* fallback_font) const;
	void setMaxHeight();

	core::array<SFontArea>		Areas;
	core::map<wchar_t, s32>		CharacterMap;
	video::IVideoDriver*		Driver;
	IGUISpriteBank*			SpriteBank;
	IGUIEnvironment*		Environment;
	u32				WrongCharacter;
	s32				MaxHeight;
	s32				GlobalKerningWidth, GlobalKerningHeight;

	core::stringw Invisible;
};

} // end namespace gui
} // end namespace irr


#endif // __C_GUI_FONT_H_INCLUDED__

