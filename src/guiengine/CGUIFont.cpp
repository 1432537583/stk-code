// Copyright (C) 2002-2009 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CGUIFont.h"

#include "IGUIEnvironment.h"
#include "IXMLReader.h"
#include "IReadFile.h"
#include "IVideoDriver.h"
#include "IGUISpriteBank.h"
#include <iostream>
#include "guiengine/engine.hpp"
#include "io/file_manager.hpp"

namespace irr
{
namespace gui
{

//! constructor
ScalableFont::ScalableFont(IGUIEnvironment *env, const io::path& filename)
: Driver(0), SpriteBank(0), Environment(env), WrongCharacter(0),
	MaxHeight(0), GlobalKerningWidth(0), GlobalKerningHeight(0)
{
    m_fallback_font = NULL;
    m_fallback_font_scale = 1.0f;
    m_fallback_kerning_width = 0;
    
    m_black_border = false;
    
	#ifdef _DEBUG
	setDebugName("ScalableFont");
	#endif
    
    m_scale = 1.0f;
    m_shadow = false;

	if (Environment)
	{
		// don't grab environment, to avoid circular references
		Driver = Environment->getVideoDriver();

		SpriteBank = Environment->addEmptySpriteBank(filename);
		if (SpriteBank)
			SpriteBank->grab();
	}

	if (Driver)
		Driver->grab();

	setInvisibleCharacters ( L" " );
    
    load( file_manager->createXMLReader(filename.c_str()) );
}


//! destructor
ScalableFont::~ScalableFont()
{
	if (Driver)
		Driver->drop();

	if (SpriteBank)
		SpriteBank->drop();
}
void ScalableFont::setShadow(irr::video::SColor col)
{
    m_shadow = true;
    m_shadow_color = col;
}
    
//! loads a font file from xml
bool ScalableFont::load(io::IXMLReader* xml)
{
	if (!SpriteBank)
		return false;

	while (xml->read())
	{
		if (io::EXN_ELEMENT == xml->getNodeType())
		{
			if (core::stringw(L"Texture") == xml->getNodeName())
			{
				// add a texture
                core::stringc filename = xml->getAttributeValue(L"filename");
				core::stringc fn = file_manager->getFontFile(filename.c_str()).c_str();
				u32 i = (u32)xml->getAttributeValueAsInt(L"index");
				core::stringw alpha = xml->getAttributeValue(L"hasAlpha");

                //std::cout << "---- Adding font texture " << fn.c_str() << "; alpha=" << alpha.c_str() << std::endl;

                
                // make sure the sprite bank has enough textures in it
				while (i+1 > SpriteBank->getTextureCount())
                {
					SpriteBank->addTexture(NULL);
                }

                TextureInfo info;
                info.m_file_name = fn;
                info.m_has_alpha = (alpha == core::stringw("false"));
                
				// disable mipmaps+filtering
				//bool mipmap = Driver->getTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS);
				//Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false);

                m_texture_files[i] = info;
                
                /*
				// load texture
				SpriteBank->setTexture(i, Driver->getTexture(fn));

				// set previous mip-map+filter state
				//Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmap);

				// couldn't load texture, abort.
				if (!SpriteBank->getTexture(i))
				{
                    std::cerr << "!!!!! Unable to load all textures in the font, aborting" << std::endl;
					_IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
					return false;
				}
				else
				{
					// colorkey texture rather than alpha channel?
					if (alpha == core::stringw("false"))
						Driver->makeColorKeyTexture(SpriteBank->getTexture(i), core::position2di(0,0));
				}
                 */
			}
			else if (core::stringw(L"c") == xml->getNodeName())
			{
				// adding a character to this font
				SFontArea a;
				SGUISpriteFrame f;
				SGUISprite s;
				core::rect<s32> rectangle;

				a.underhang		= xml->getAttributeValueAsInt(L"u");
				a.overhang		= xml->getAttributeValueAsInt(L"o");
				a.spriteno		= SpriteBank->getSprites().size();
				s32 texno		= xml->getAttributeValueAsInt(L"i");

				// parse rectangle
				core::stringc rectstr	= xml->getAttributeValue(L"r");
				wchar_t ch		= xml->getAttributeValue(L"c")[0];

				const c8 *c = rectstr.c_str();
				s32 val;
				val = 0;
				while (*c >= '0' && *c <= '9')
				{
					val *= 10;
					val += *c - '0';
					c++;
				}
				rectangle.UpperLeftCorner.X = val;
				while (*c == L' ' || *c == L',') c++;

				val = 0;
				while (*c >= '0' && *c <= '9')
				{
					val *= 10;
					val += *c - '0';
					c++;
				}
				rectangle.UpperLeftCorner.Y = val;
				while (*c == L' ' || *c == L',') c++;

				val = 0;
				while (*c >= '0' && *c <= '9')
				{
					val *= 10;
					val += *c - '0';
					c++;
				}
				rectangle.LowerRightCorner.X = val;
				while (*c == L' ' || *c == L',') c++;

				val = 0;
				while (*c >= '0' && *c <= '9')
				{
					val *= 10;
					val += *c - '0';
					c++;
				}
				rectangle.LowerRightCorner.Y = val;

				CharacterMap.insert(ch,Areas.size());
                //std::cout << "Inserting character '" << ch << "' with area " << Areas.size() << std::endl;

				// make frame
				f.rectNumber = SpriteBank->getPositions().size();
				f.textureNumber = texno;

				// add frame to sprite
				s.Frames.push_back(f);
				s.frameTime = 0;

				// add rectangle to sprite bank
				SpriteBank->getPositions().push_back(rectangle);
				a.width = rectangle.getWidth();

				// add sprite to sprite bank
				SpriteBank->getSprites().push_back(s);

				// add character to font
				Areas.push_back(a);
			}
		}
	}

	// set bad character
	WrongCharacter = getAreaFromCharacter(L' ', NULL);

	setMaxHeight();

	return true;
}
void ScalableFont::setScale(const float scale)
{
    m_scale = scale;
}

void ScalableFont::setMaxHeight()
{
	MaxHeight = 0;
	s32 t;

	core::array< core::rect<s32> >& p = SpriteBank->getPositions();

	for (u32 i=0; i<p.size(); ++i)
	{
		t = p[i].getHeight();
		if (t>MaxHeight)
			MaxHeight = t;
	}


}


//! loads a font file, native file needed, for texture parsing
bool ScalableFont::load(io::IReadFile* file)
{
	if (!Driver) return false;

	return loadTexture(Driver->createImageFromFile(file), file->getFileName());
}


//! loads a font file, native file needed, for texture parsing
bool ScalableFont::load(const io::path& filename)
{
	if (!Driver) return false;

	return loadTexture(Driver->createImageFromFile( filename ), filename);
}


//! load & prepare font from ITexture
bool ScalableFont::loadTexture(video::IImage* image, const io::path& name)
{
	if (!image) return false;

	s32 lowerRightPositions = 0;

	video::IImage* tmpImage=image;
    /*
	bool deleteTmpImage=false;
	switch(image->getColorFormat())
	{
	case video::ECF_R5G6B5:
		tmpImage =  new video::CImage(video::ECF_A1R5G5B5,image);
		deleteTmpImage=true;
		break;
	case video::ECF_A1R5G5B5:
	case video::ECF_A8R8G8B8:
		break;
	case video::ECF_R8G8B8:
		tmpImage = new video::CImage(video::ECF_A8R8G8B8,image);
		deleteTmpImage=true;
		break;
	}*/
	readPositions(tmpImage, lowerRightPositions);

	WrongCharacter = getAreaFromCharacter(L' ', NULL);

	// output warnings
	if (!lowerRightPositions || !SpriteBank->getSprites().size())
		std::cerr << "Either no upper or lower corner pixels in the font file. If this font was made using the new font tool, please load the XML file instead. If not, the font may be corrupted.\n";
	else
	if (lowerRightPositions != (s32)SpriteBank->getPositions().size())
		std::cerr << "The amount of upper corner pixels and the lower corner pixels is not equal, font file may be corrupted.\n";

	bool ret = ( !SpriteBank->getSprites().empty() && lowerRightPositions );

	if ( ret )
	{
		bool flag[2];
		flag[0] = Driver->getTextureCreationFlag ( video::ETCF_ALLOW_NON_POWER_2 );
		flag[1] = Driver->getTextureCreationFlag ( video::ETCF_CREATE_MIP_MAPS );

		Driver->setTextureCreationFlag(video::ETCF_ALLOW_NON_POWER_2, true);
		Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, false );

		SpriteBank->addTexture(Driver->addTexture(name, tmpImage));

		Driver->setTextureCreationFlag(video::ETCF_ALLOW_NON_POWER_2, flag[0] );
		Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, flag[1] );
	}
	//if (deleteTmpImage)
	//	tmpImage->drop();
	image->drop();

	setMaxHeight();

	return ret;
}


void ScalableFont::readPositions(video::IImage* image, s32& lowerRightPositions)
{
	const core::dimension2d<u32> size = image->getDimension();

	video::SColor colorTopLeft = image->getPixel(0,0);
	colorTopLeft.setAlpha(255);
	image->setPixel(0,0,colorTopLeft);
	video::SColor colorLowerRight = image->getPixel(1,0);
	video::SColor colorBackGround = image->getPixel(2,0);
	video::SColor colorBackGroundTransparent = 0;

	image->setPixel(1,0,colorBackGround);

	// start parsing

	core::position2d<s32> pos(0,0);
	for (pos.Y=0; pos.Y<(s32)size.Height; ++pos.Y)
	{
		for (pos.X=0; pos.X<(s32)size.Width; ++pos.X)
		{
			const video::SColor c = image->getPixel(pos.X, pos.Y);
			if (c == colorTopLeft)
			{
				image->setPixel(pos.X, pos.Y, colorBackGroundTransparent);
				SpriteBank->getPositions().push_back(core::rect<s32>(pos, pos));
			}
			else
			if (c == colorLowerRight)
			{
				// too many lower right points
				if (SpriteBank->getPositions().size()<=(u32)lowerRightPositions)
				{
					lowerRightPositions = 0;
					return;
				}

				image->setPixel(pos.X, pos.Y, colorBackGroundTransparent);
				SpriteBank->getPositions()[lowerRightPositions].LowerRightCorner = pos;
				// add frame to sprite bank
				SGUISpriteFrame f;
				f.rectNumber = lowerRightPositions;
				f.textureNumber = 0;
				SGUISprite s;
				s.Frames.push_back(f);
				s.frameTime = 0;
				SpriteBank->getSprites().push_back(s);
				// add character to font
				SFontArea a;
				a.overhang = 0;
				a.underhang = 0;
				a.spriteno = lowerRightPositions;
				a.width = SpriteBank->getPositions()[lowerRightPositions].getWidth();
				Areas.push_back(a);
				// map letter to character
				wchar_t ch = (wchar_t)(lowerRightPositions + 32);
				CharacterMap.set(ch, lowerRightPositions);

				++lowerRightPositions;
			}
			else
			if (c == colorBackGround)
				image->setPixel(pos.X, pos.Y, colorBackGroundTransparent);
		}
	}
}


//! set an Pixel Offset on Drawing ( scale position on width )
void ScalableFont::setKerningWidth(s32 kerning)
{
	GlobalKerningWidth = kerning;
}


//! set an Pixel Offset on Drawing ( scale position on width )
s32 ScalableFont::getKerningWidth(const wchar_t* thisLetter, const wchar_t* previousLetter) const
{
	s32 ret = GlobalKerningWidth;

	if (thisLetter)
	{
		ret += Areas[getAreaFromCharacter(*thisLetter, NULL)].overhang;

		if (previousLetter)
		{
			ret += Areas[getAreaFromCharacter(*previousLetter, NULL)].underhang;
		}
	}

	return ret;
}


//! set an Pixel Offset on Drawing ( scale position on height )
void ScalableFont::setKerningHeight(s32 kerning)
{
	GlobalKerningHeight = kerning;
}


//! set an Pixel Offset on Drawing ( scale position on height )
s32 ScalableFont::getKerningHeight () const
{
	return GlobalKerningHeight;
}


//! returns the sprite number from a given character
u32 ScalableFont::getSpriteNoFromChar(const wchar_t *c) const
{
	return Areas[getAreaFromCharacter(*c, NULL)].spriteno;
}


s32 ScalableFont::getAreaFromCharacter(const wchar_t c, bool* fallback_font) const
{
	core::map<wchar_t, s32>::Node* n = CharacterMap.find(c);
	if (n)
    {
        if (fallback_font != NULL) *fallback_font = false;
		return n->getValue();
    }
    else if (m_fallback_font != NULL && fallback_font != NULL)
    {
        //std::wcout << L"Font does not have this character : <" << c << L">, trying fallback font" << std::endl;
        *fallback_font = true;
        return m_fallback_font->getAreaFromCharacter(c, NULL);
    }
	else
    {
        //std::wcout << L"This font does not have this character : <" << c << L">" << std::endl;
        if (fallback_font != NULL) *fallback_font = false;
		return WrongCharacter;
    }
}

void ScalableFont::setInvisibleCharacters( const wchar_t *s )
{
	Invisible = s;
}


//! returns the dimension of text
core::dimension2d<u32> ScalableFont::getDimension(const wchar_t* text) const
{
	core::dimension2d<u32> dim(0, 0);
	core::dimension2d<u32> thisLine(0, MaxHeight);

	for (const wchar_t* p = text; *p; ++p)
	{
		bool lineBreak=false;
		if (*p == L'\r') // Windows breaks
		{
			lineBreak = true;
			if (p[1] == L'\n') // Windows breaks
				++p;
		}
		else if (*p == L'\n') // Unix breaks
		{
			lineBreak = true;
		}
		if (lineBreak)
		{
			dim.Height += thisLine.Height;
			if (dim.Width < thisLine.Width)
				dim.Width = thisLine.Width;
			thisLine.Width = 0;
			continue;
		}

        bool fallback = false;
        const int areaID = getAreaFromCharacter(*p, &fallback);
		const SFontArea &area = (fallback ? m_fallback_font->Areas[areaID] : Areas[areaID]);

		thisLine.Width += area.underhang;
        
        
        if (fallback) thisLine.Width += (area.width + area.overhang)*m_fallback_font_scale + m_fallback_kerning_width;
		else          thisLine.Width += area.width + area.overhang + GlobalKerningWidth;
	}

	dim.Height += thisLine.Height;
	if (dim.Width < thisLine.Width) dim.Width = thisLine.Width;

   // std::cout << "ScalableFont::getDimension returns : " << dim.Width << ", " << dim.Height << " --> ";

    dim.Width  = (int)(dim.Width  * m_scale);
    dim.Height = (int)(dim.Height * m_scale);

    //std::cout << dim.Width << ", " << dim.Height << std::endl;
    
	return dim;
}

//! draws some text and clips it to the specified rectangle if wanted
void ScalableFont::draw(const core::stringw& text, const core::rect<s32>& position,
					video::SColor color, bool hcenter, bool vcenter, const core::rect<s32>* clip)
{
	if (!Driver) return;
        
    if (m_shadow)
    {
        m_shadow = false; // avoid infinite recursion
        
        core::rect<s32> shadowpos = position;
        shadowpos.LowerRightCorner.X += 2;
        shadowpos.LowerRightCorner.Y += 2;

        draw(text, shadowpos, m_shadow_color, hcenter, vcenter, clip);
        
        m_shadow = true; // set back
    }
    
    //color = video::SColor(255,255,255,255);

	core::dimension2d<s32> textDimension;
	core::position2d<s32> offset = position.UpperLeftCorner;

	if (hcenter || vcenter || clip) textDimension = getDimension(text.c_str());

	if (hcenter)
    {
		offset.X += (position.getWidth() - textDimension.Width) / 2;
    }

	if (vcenter)
    {
		offset.Y += (position.getHeight() - textDimension.Height) / 2;
    }

	if (clip)
	{
		core::rect<s32> clippedRect(offset, textDimension);
		clippedRect.clipAgainst(*clip);
		if (!clippedRect.isValid()) return;
	}

    const unsigned int text_size = text.size();
	core::array<u32> indices(text_size);
	core::array<core::position2di> offsets(text_size);
    std::vector<bool> fallback(text_size);
    
	for (u32 i = 0;i < text_size;i++)
	{
		wchar_t c = text[i];

		bool lineBreak=false;
		if (c == L'\r') // Windows breaks
		{
			lineBreak = true;
			if (text[i + 1] == L'\n') c = text[++i];
		}
		else if ( c == L'\n') // Unix breaks
		{
			lineBreak = true;
		}

		if (lineBreak)
		{
			offset.Y += MaxHeight;
			offset.X = position.UpperLeftCorner.X;

			if ( hcenter )
			{
				core::dimension2d<u32> lineDim = getDimension(text.c_str());
				offset.X += (position.getWidth() - lineDim.Width) >> 1;
			}
			continue;
		}

        bool fallback_font = false;
        const int areaID = getAreaFromCharacter(c, &fallback_font);
        //std::cout << "Char " << c << " has area " << areaID << std::endl;
		SFontArea& area = (fallback_font ? m_fallback_font->Areas[areaID] : Areas[areaID]);

		offset.X += area.underhang;
		if ( Invisible.findFirst ( c ) < 0 )
		{
			indices.push_back(area.spriteno);
			offsets.push_back(offset);
            fallback[i] = fallback_font;
		}
        
        if (fallback[i]) offset.X += (int)(((area.width + area.overhang)*m_fallback_font_scale + m_fallback_kerning_width) * m_scale);
		else             offset.X += (int)((area.width + area.overhang + GlobalKerningWidth) * m_scale);
	}

	//SpriteBank->draw2DSpriteBatch(indices, offsets, clip, color);
    
    const int indiceAmount = indices.size();
    core::array< SGUISprite >& sprites = SpriteBank->getSprites();
    core::array< core::rect<s32> >& positions = SpriteBank->getPositions();
    
    core::array< SGUISprite >* fallback_sprites = (m_fallback_font != NULL ?
                                                   &m_fallback_font->SpriteBank->getSprites() :
                                                   NULL);
    core::array< core::rect<s32> >* fallback_positions = (m_fallback_font != NULL ?
                                                          &m_fallback_font->SpriteBank->getPositions() :
                                                          NULL);
    
    video::IVideoDriver* driver = GUIEngine::getDriver();
    const int spriteAmount = sprites.size();
    for (int n=0; n<indiceAmount; n++)
    {
        const int spriteID = indices[n];
        if (!fallback[n] && (spriteID < 0 || spriteID >= spriteAmount)) continue;
        
        //assert(sprites[spriteID].Frames.size() > 0);
        
        const int texID = (fallback[n] ?
                           (*fallback_sprites)[spriteID].Frames[0].textureNumber :
                           sprites[spriteID].Frames[0].textureNumber);
        
        core::rect<s32> source = (fallback[n] ?
                                  (*fallback_positions)[(*fallback_sprites)[spriteID].Frames[0].rectNumber] :
                                  positions[sprites[spriteID].Frames[0].rectNumber]);
        
        core::dimension2d<s32> size = source.getSize();
        size.Width  = (int)(size.Width  * m_scale);
        size.Height = (int)(size.Height * m_scale);
        core::rect<s32> dest(offsets[n], (fallback[n] ? size*m_fallback_font_scale : size));
        
        video::SColor colors[] = {color, color, color, color};
                
        video::ITexture* texture = (fallback[n] ?
                                    m_fallback_font->SpriteBank->getTexture(texID) :
                                    SpriteBank->getTexture(texID) );
        
        /*
        if (fallback[n])
        {
            std::cout << "USING fallback font " << core::stringc(texture->getName()).c_str() <<
            "; source area is " << source.UpperLeftCorner.X << ", " << source.UpperLeftCorner.Y <<
            ", size " << source.getWidth() << ", " << source.getHeight() << "; dest = " <<
            offsets[n].X << ", " << offsets[n].Y << std::endl;
        }*/
        
        if (texture == NULL)
        {
            // perform lazy loading
            
            if (fallback[n])
            {
                m_fallback_font->lazyLoadTexture(texID);
                texture = m_fallback_font->SpriteBank->getTexture(texID);
            }
            else
            {
                lazyLoadTexture(texID);
                texture = SpriteBank->getTexture(texID);
            }
            
            if (texture == NULL)
            {
                std::cerr << "WARNING: character not found in current font\n";
                continue; // no such character
            }
        }
        
        if (m_black_border)
        {
            // draw black border
            video::SColor black(color.getAlpha(),0,0,0);
            video::SColor black_colors[] = {black, black, black, black};
            
            for (int x_delta=-2; x_delta<=2; x_delta++)
            {
                for (int y_delta=-2; y_delta<=2; y_delta++)
                {
                    if (x_delta == 0 || y_delta == 0) continue;
                    driver->draw2DImage(texture,
                                        dest + core::position2d<s32>(x_delta, y_delta),
                                        source,
                                        clip,
                                        black_colors, true);
                }            
            }
        }
        
        if (fallback[n])
        {
            // draw text over
            static video::SColor orange(color.getAlpha(), 255, 100, 0);
            static video::SColor yellow(color.getAlpha(), 255, 220, 15);
            video::SColor title_colors[] = {yellow, orange, orange, yellow};
            driver->draw2DImage(texture,
                                dest,
                                source,
                                clip,
                                title_colors, true);
        }
        else
        {
            driver->draw2DImage(texture,
                                dest,
                                source,
                                clip,
                                colors, true);
        }
    }
}


void ScalableFont::lazyLoadTexture(int texID)
{
    Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, true);
    
    // load texture
    SpriteBank->setTexture(texID, Driver->getTexture( m_texture_files[texID].m_file_name ));
    
    // set previous mip-map+filter state
    //Driver->setTextureCreationFlag(video::ETCF_CREATE_MIP_MAPS, mipmap);
    
    // couldn't load texture, abort.
    if (!SpriteBank->getTexture(texID))
    {
        std::cerr << "!!!!! Unable to load all textures in the font" << std::endl;
        _IRR_IMPLEMENT_MANAGED_MARSHALLING_BUGFIX;
        return;
    }
    else
    {
        // colorkey texture rather than alpha channel?
        if (! m_texture_files[texID].m_has_alpha)
        {
            Driver->makeColorKeyTexture(SpriteBank->getTexture(texID), core::position2di(0,0));
        }
    }
}
    
//! Calculates the index of the character in the text which is on a specific position.
s32 ScalableFont::getCharacterFromPos(const wchar_t* text, s32 pixel_x) const
{
	s32 x = 0;
	s32 idx = 0;

	while (text[idx])
	{
		const SFontArea& a = Areas[getAreaFromCharacter(text[idx], NULL)];

		x += a.width + a.overhang + a.underhang + GlobalKerningWidth;

		if (x >= pixel_x)
			return idx;

		++idx;
	}

	return -1;
}


IGUISpriteBank* ScalableFont::getSpriteBank() const
{
	return SpriteBank;
}

} // end namespace gui
} // end namespace irr


