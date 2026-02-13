//=============================================================================//
//
// Purpose: Common PNG image loader.
//
//=============================================================================//

#include "img_png_loader.h"
#include "libpng/png.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef PNG_LIBPNG_VER
void ReadPNG_CUtlBuffer( png_structp png_ptr, png_bytep data, size_t length )
{
	if ( !png_ptr )
		return;

	CUtlBuffer *pBuffer = (CUtlBuffer *)png_get_io_ptr( png_ptr );

	if ( (size_t)pBuffer->TellMaxPut() < ( (size_t)pBuffer->TellGet() + length ) ) // CUtlBuffer::CheckGet()
	{
		//png_error( png_ptr, "read error" );
		png_longjmp( png_ptr, 1 );
	}

	pBuffer->Get( data, length );
}

//
// Read a PNG image from file into buffer as RGBA.
//
bool PNGtoRGBA( IFileSystem *filesystem, const char *filename, CUtlMemory< byte > &buffer, int &width, int &height )
{
	// Read the whole image to memory
	CUtlBuffer fileBuffer;

	if ( !filesystem->ReadFile( filename, NULL, fileBuffer ) )
	{
		Warning( "Failed to read PNG file (%s)\n", filename );
		return false;
	}

	return PNGtoRGBA( fileBuffer, buffer, width, height );
}

//
// TODO: Error codes
//
bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height )
{
	if ( png_sig_cmp( (png_const_bytep)fileBuffer.Base(), 0, 8 ) )
	{
		Warning( "Bad PNG signature\n" );
		return false;
	}

	png_bytepp row_pointers = NULL;

	png_structp png_ptr = png_create_read_struct( png_get_libpng_ver(NULL), NULL, NULL, NULL );
	png_infop info_ptr = png_create_info_struct( png_ptr );

	if ( !info_ptr || !png_ptr )
	{
		Warning( "Out of memory reading PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( setjmp( png_jmpbuf( png_ptr ) ) )
	{
		Warning( "Failed to read PNG\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		if ( row_pointers )
			free( row_pointers );
		return false;
	}

	png_set_read_fn( png_ptr, &fileBuffer, ReadPNG_CUtlBuffer );
	png_read_info( png_ptr, info_ptr );

	png_uint_32 image_width, image_height;
	int bit_depth, color_type;

	png_get_IHDR( png_ptr, info_ptr, &image_width, &image_height, &bit_depth, &color_type, NULL, NULL, NULL );

	width = image_width;
	height = image_height;

	// expand palette images to RGB, low-bit-depth grayscale images to 8 bits,
	// transparency chunks to full alpha channel; strip 16-bit-per-sample
	// images to 8 bits per sample; and convert grayscale to RGB[A]

	if ( color_type == PNG_COLOR_TYPE_PALETTE )
		png_set_expand(png_ptr);
	if ( color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8 )
		png_set_expand(png_ptr);
	if ( png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS) )
		png_set_expand(png_ptr);
#ifdef PNG_READ_16_TO_8_SUPPORTED
	if ( bit_depth == 16 )
	#ifdef PNG_READ_SCALE_16_TO_8_SUPPORTED
		png_set_scale_16(png_ptr);
	#else
		png_set_strip_16(png_ptr);
	#endif
#endif
	if ( color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA )
		png_set_gray_to_rgb(png_ptr);

	// Expand RGB to RGBA
	if ( color_type == PNG_COLOR_TYPE_RGB )
		png_set_filler( png_ptr, 0xffff, PNG_FILLER_AFTER );

	png_read_update_info( png_ptr, info_ptr );

	png_uint_32 rowbytes = png_get_rowbytes( png_ptr, info_ptr );
	int channels = (int)png_get_channels( png_ptr, info_ptr );

	if ( channels != 4 )
	{
		Warning( "PNG is not RGBA\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	if ( image_height > ((size_t)(-1)) / rowbytes )
	{
		Warning( "PNG data buffer would be too large\n" );
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	buffer.Init( 0, rowbytes * image_height );
	row_pointers = (png_bytepp)malloc( image_height * sizeof(png_bytep) );

	Assert( buffer.Base() && row_pointers );

	if ( !row_pointers )
	{
		png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
		return false;
	}

	for ( png_uint_32 i = 0; i < image_height; ++i )
		row_pointers[i] = buffer.Base() + i*rowbytes;

	png_read_image( png_ptr, row_pointers );
	//png_read_end( png_ptr, NULL );

	png_destroy_read_struct( &png_ptr, &info_ptr, NULL );
	free( row_pointers );

	return true;
}

bool PNGSupported()
{
	return true;
}
#else
bool PNGtoRGBA( IFileSystem *filesystem, const char *filename, CUtlMemory< byte > &buffer, int &width, int &height )
{
	Warning( "PNG library unsupported\n" );
	return false;
}

bool PNGtoRGBA( CUtlBuffer &fileBuffer, CUtlMemory< byte > &buffer, int &width, int &height )
{
	Warning( "PNG library unsupported\n" );
	return false;
}

bool PNGSupported()
{
	return false;
}
#endif
