/*
 * Copyright(C) 2009 Romain Bignon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef CACA_IMAGE_H
#define CACA_IMAGE_H

#include <string>
#include <exception>

using std::string;

class CacaError : public std::exception {};
class CacaNotLoaded : public std::exception {};

class CacaImage
{
	string path;
	string buf;
	unsigned width, height, font_width, font_height;

#ifdef USE_CACA
	struct image
	{
	    char *pixels;
	    unsigned int w, h;
	    struct caca_dither *dither;
	    void *priv;
	};

	struct image * load_image(char const * name);
	void unload_image(struct image * im);
#endif /* USE_CACA */

public:

	CacaImage();
	CacaImage(string path, unsigned width = 0, unsigned height = 0, unsigned font_width = 6, unsigned font_height = 10);
	~CacaImage();

	string getIRCBuffer(unsigned width, unsigned height = 0, unsigned font_width = 6, unsigned font_height = 10);
	string getIRCBuffer();
};

#endif /* CACA_IMAGE_H */