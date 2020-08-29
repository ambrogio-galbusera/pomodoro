/***************************************************************************//**
 * @file
 * @brief Graphic render functions for spaceinvaders game.
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

#ifndef __RENDER_H_
#define __RENDER_H_

#include "stdbool.h"

#include "display.h"


void RENDER_SetFontColor(int blackFont);
void RENDER_SetFontResizeFactor(int factor);
void RENDER_Write(int posx, int posy, char *str);
void RENDER_DrawSprite(int posx, int posy, int w, int h, const unsigned char* bits);
void RENDER_ClearFramebufferLines(int firstLine, int lineCount);
void RENDER_ClearFramebufferArea(int xstart, int ystart, int xend, int yend, int color);
void RENDER_SetFramebuffer(const unsigned char *img);
void RENDER_DrawBackdrop(void);
void RENDER_UpdateDisplay(bool fullUpdate, DISPLAY_Device_t* displayDevice);

void RENDER_FillRect(int xstart, int ystart, int xend, int yend, int color);

#endif
