/*

  Copyright (C) 2012 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/

#include "string.h"
#include "cairo_guido2img.h"
#include "CairoSystem.h"
#include "CairoDevice.h"
#include <cairo.h>
#include "FreeImage.h"

#include <assert.h>

using namespace std;
namespace guidohttpd
{

// ------------------
// freeimage

// as size of character is 1, we don't ever need to multiply position by size in the buffer
unsigned DLL_CALLCONV myReadProc(void *buffer, unsigned size, unsigned count, fi_handle handle) {
  png_stream_to_byte_array_closure_t *closure =
          (png_stream_to_byte_array_closure_t *) handle;

  int to_read = size * count;
  int room = closure->size_ - closure->pos_;
  if (to_read > room) {
    to_read = room;
  }
  memcpy(buffer, closure->start_ + closure->pos_, to_read);
  closure->pos_ += to_read;
  return to_read;
}
  
unsigned DLL_CALLCONV myWriteProc(void *buffer, unsigned size, unsigned count, fi_handle handle) {
  png_stream_to_byte_array_closure_t *closure =
          (png_stream_to_byte_array_closure_t *) handle;

  int to_write = size * count;
  closure->size_ += to_write;
  memcpy(closure->start_ + closure->pos_, buffer, to_write);
  closure->pos_ += to_write;
  return to_write;
}
    
int DLL_CALLCONV mySeekProc(fi_handle handle, long offset, int origin) {
  png_stream_to_byte_array_closure_t *closure =
          (png_stream_to_byte_array_closure_t *) handle;

  int place = 0;
  if (origin == SEEK_CUR)
    place = closure->pos_;
  else if (origin == SEEK_END)
    place = closure->size_;

  closure->pos_ = place + offset;

  return 0;
}
      
long DLL_CALLCONV myTellProc(fi_handle handle) {
  png_stream_to_byte_array_closure_t *closure =
          (png_stream_to_byte_array_closure_t *) handle;

  return closure->pos_;
}


// ------------------
static cairo_status_t
write_png_stream_to_byte_array (void *in_closure, const unsigned char *data,
                                                unsigned int length)
{
    png_stream_to_byte_array_closure_t *closure =
        (png_stream_to_byte_array_closure_t *) in_closure;

    memcpy (closure->data_, data, length);
    closure->data_ += length;
    closure->size_ += length;
    return CAIRO_STATUS_SUCCESS;
}

//--------------------------------------------------------------------------
cairo_guido2img::cairo_guido2img (string svgfontfile) : guido2img(svgfontfile) {
  fBuffer.data_ = new char[1048576];
  fBuffer.start_ = fBuffer.data_;
  fBuffer.size_ = 0;
  fBuffer.pos_ = 0;
}

cairo_guido2img::~cairo_guido2img () {
  delete[] fBuffer.start_;
}

int cairo_guido2img::convert (guidosession* const currentSession)
{
    GuidoWebApiFormat format = currentSession->getFormat();
    fBuffer.reset();
    if (format == GUIDO_WEB_API_SVG) {
      string svg;
      int err = currentSession->simpleSVGHelper(fSvgFontFile, &svg);
      const char* cc_svg = svg.c_str();
      fBuffer.size_ = svg.size();
      strcpy(fBuffer.data_, cc_svg);
      return err;
    }

    GuidoPageFormat pf;
    currentSession->fillGuidoPageFormatUsingCurrentSettings(&pf);

    int width = (int) pf.width;
    int height = (int) pf.height;

    cairo_surface_t *surface;
    cairo_t *cr;
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
    cr = cairo_create (surface);

    CairoSystem sys (cr);

    VGDevice * dev = sys.CreateDisplayDevice();
    dev->SelectFillColor(VGColor(255,255,255));
    dev->Rectangle (0, 0, width, height);
    dev->SelectFillColor(VGColor(0,0,0));
    GuidoOnDrawDesc desc;

    desc.handle = currentSession->getGRHandler();
    desc.hdc = dev;
    desc.page = 1;
    desc.updateRegion.erase = true;
    desc.scrollx = desc.scrolly = 0;
    desc.sizex = width;
    desc.sizey = height;
    desc.isprint = false;
    GuidoErrCode err = GuidoOnDraw (&desc);

    cairo_surface_write_to_png_stream (surface, write_png_stream_to_byte_array, &fBuffer);

    if ((format == GUIDO_WEB_API_JPEG)
        || (format == GUIDO_WEB_API_GIF)) {
      // freeimage
      FreeImageIO io;
      io.read_proc  = myReadProc;
      io.write_proc = myWriteProc;
      io.seek_proc  = mySeekProc;
      io.tell_proc  = myTellProc;
      FREE_IMAGE_FORMAT fif = FreeImage_GetFileTypeFromHandle(&io, (fi_handle)&fBuffer, 0);
      if(fif != FIF_UNKNOWN) {
        FIBITMAP *dib = FreeImage_LoadFromHandle(fif, &io, (fi_handle) &fBuffer, 0);
        fBuffer.reset(); // need to reset for writing
        FREE_IMAGE_FORMAT fmt = format == GUIDO_WEB_API_JPEG ? FIF_JPEG : FIF_GIF;
        FreeImage_SaveToHandle(fmt, dib, &io, (fi_handle) &fBuffer, 0);
        FreeImage_Unload(dib);
      }
      else
        err = guidoErrActionFailed;
    }

    return err;
}

} // end namespoace
