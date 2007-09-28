/*
	draw.h
	these are the only functions outside the refresh
	allowed to touch the vid buffer

	$Id: draw.h,v 1.27 2007-09-28 14:30:51 sezero Exp $
*/

#ifndef __HX2_DRAW_H
#define __HX2_DRAW_H

#define MAX_DISC 18

extern	qboolean	draw_reinit;

void Draw_Init (void);
void Draw_ReInit (void);

qpic_t *Draw_PicFromWad (char *name);
qpic_t *Draw_PicFromFile (const char *name);
qpic_t *Draw_PicFileBuf (const char *name, void *p, size_t *size);

qpic_t *Draw_CachePic (const char *path);
qpic_t *Draw_CachePicNoTrans (const char *path);
qpic_t *Draw_CachePicResize (const char *path, int targetWidth, int targetHeight);

void Draw_Pic (int x, int y, qpic_t *pic);
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha);
void Draw_IntermissionPic (qpic_t *pic);
void Draw_PicCropped (int x, int y, qpic_t *pic);
void Draw_SubPic (int x, int y, qpic_t *pic, int srcx, int srcy, int width, int height);
void Draw_SubPicCropped (int x, int y, int h, qpic_t *pic);
void Draw_TransPic (int x, int y, qpic_t *pic);
void Draw_TransPicCropped (int x, int y, qpic_t *pic);
#ifndef GLQUAKE
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation);
#else
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation, int p_class, int top, int bottom);
#endif
void Draw_ConsoleBackground (int lines);
void Draw_Crosshair (void);

#if defined(GLQUAKE)
#define Draw_BeginDisc()
#define Draw_EndDisc()
#else
void Draw_BeginDisc (void);
void Draw_EndDisc (void);
#endif

void Draw_TileClear (int x, int y, int w, int h);
void Draw_Fill (int x, int y, int w, int h, int c);
void Draw_FadeScreen (void);

void Draw_Character (int x, int y, const unsigned int num);
void Draw_String (int x, int y, const char *str);
void Draw_SmallCharacter (int x, int y, const int num);
void Draw_SmallString (int x, int y, const char *str);
void Draw_RedString (int x, int y, const char *str);
void Draw_BigCharacter (int x, int y, int num);

// game/engine name to draw on the console
#define GAME_MOD_NAME		ENGINE_NAME
#define ENGINE_WATERMARK	GAME_MOD_NAME " " STRINGIFY(ENGINE_VERSION) " (" PLATFORM_STRING ")"

#endif	/* __HX2_DRAW_H */

