/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// Message.cpp
//
// implementation of CHudMessage class
//

#include "hud.h"
#include "cl_util.h"
#include <string.h>
#include <stdio.h>
#include "parsemsg.h"

DECLARE_MESSAGE( m_Message, HudText )
DECLARE_MESSAGE( m_Message, GameTitle )

// 1 Global client_textmessage_t for custom messages that aren't in the titles.txt
client_textmessage_t	g_pCustomMessage;
char *g_pCustomName = "Custom";
char g_pCustomText[1024];

int CHudMessage::Init(void)
{
	HOOK_MESSAGE( HudText );
	HOOK_MESSAGE( GameTitle );

	gHUD.AddHudElem(this);

	Reset();

	return 1;
};

int CHudMessage::VidInit( void )
{
	m_HUD_title_half = gHUD.GetSpriteIndex( "title_half" );
	m_HUD_title_life = gHUD.GetSpriteIndex( "title_life" );

	return 1;
};


void CHudMessage::Reset( void )
{
 	memset( m_pMessages, 0, sizeof( m_pMessages[0] ) * maxHUDMessages );
	memset( m_startTime, 0, sizeof( m_startTime[0] ) * maxHUDMessages );
	
	m_gameTitleTime = 0;
	m_pGameTitle = NULL;
}


float CHudMessage::FadeBlend( float fadein, float fadeout, float hold, float localTime )
{
	float fadeTime = fadein + hold;
	float fadeBlend;

	if ( localTime < 0 )
		return 0;

	if ( localTime < fadein )
	{
		fadeBlend = 1 - ((fadein - localTime) / fadein);
	}
	else if ( localTime > fadeTime )
	{
		if ( fadeout > 0 )
			fadeBlend = 1 - ((localTime - fadeTime) / fadeout);
		else
			fadeBlend = 0;
	}
	else
		fadeBlend = 1;

	return fadeBlend;
}


int	CHudMessage::XPosition( float x, int width, int totalWidth )
{
	int xPos;

	if ( x == -1 )
	{
		xPos = (ScreenWidth - width) / 2;
	}
	else
	{
		if ( x < 0 )
			xPos = (1.0 + x) * ScreenWidth - totalWidth;	// Alight right
		else
			xPos = x * ScreenWidth;
	}

	if ( xPos + width > ScreenWidth )
		xPos = ScreenWidth - width;
	else if ( xPos < 0 )
		xPos = 0;

	return xPos;
}


int CHudMessage::YPosition( float y, int height )
{
	int yPos;

	if ( y == -1 )	// Centered?
		yPos = (ScreenHeight - height) * 0.5;
	else
	{
		// Alight bottom?
		if ( y < 0 )
			yPos = (1.0 + y) * ScreenHeight - height;	// Alight bottom
		else // align top
			yPos = y * ScreenHeight;
	}

	if ( yPos + height > ScreenHeight )
		yPos = ScreenHeight - height;
	else if ( yPos < 0 )
		yPos = 0;

	return yPos;
}


void CHudMessage::MessageScanNextChar( void )
{
	SetColorParams(false);

	if ( m_parms.pMessage->effect == 1 && m_parms.charTime != 0 )
	{
		if ( m_parms.x >= 0 && m_parms.y >= 0 && (m_parms.x + gHUD.m_scrinfo.charWidths[ m_parms.text ]) <= ScreenWidth )
			TextMessageDrawChar( m_parms.x, m_parms.y, m_parms.text, m_parms.pMessage->r2, m_parms.pMessage->g2, m_parms.pMessage->b2 );
	}
}

void CHudMessage::SetColorParams( bool consoleFont )
{
	int srcRed, srcGreen, srcBlue, destRed, destGreen, destBlue;
	int blend;

	srcRed = m_parms.pMessage->r1;
	srcGreen = m_parms.pMessage->g1;
	srcBlue = m_parms.pMessage->b1;
	blend = 0;	// Pure source

	switch( m_parms.pMessage->effect )
	{
	// Fade-in / Fade-out
	case 0:
	case 1:
		destRed = destGreen = destBlue = 0;
		blend = m_parms.fadeBlend;
		break;

	case 2:
		m_parms.charTime += m_parms.pMessage->fadein;
		if ( m_parms.charTime > m_parms.time )
		{
			srcRed = srcGreen = srcBlue = 0;
			blend = 0;	// pure source
			if (consoleFont)
			{
				blend = 160;
				destRed = m_parms.pMessage->r2;
				destGreen = m_parms.pMessage->g2;
				destBlue = m_parms.pMessage->b2;
			}
		}
		else
		{
			float deltaTime = m_parms.time - m_parms.charTime;

			destRed = destGreen = destBlue = 0;
			if ( m_parms.time > m_parms.fadeTime )
			{
				blend = m_parms.fadeBlend;
			}
			else if ( deltaTime > m_parms.pMessage->fxtime )
				blend = 0;	// pure dest
			else
			{
				destRed = m_parms.pMessage->r2;
				destGreen = m_parms.pMessage->g2;
				destBlue = m_parms.pMessage->b2;
				blend = 255 - (deltaTime * (1.0/m_parms.pMessage->fxtime) * 255.0 + 0.5);
			}
		}
		break;
	}
	if ( blend > 255 )
		blend = 255;
	else if ( blend < 0 )
		blend = 0;

	if (consoleFont && blend > 96)
	{
		blend = 96;
	}

	m_parms.r = ((srcRed * (255-blend)) + (destRed * blend)) >> 8;
	m_parms.g = ((srcGreen * (255-blend)) + (destGreen * blend)) >> 8;
	m_parms.b = ((srcBlue * (255-blend)) + (destBlue * blend)) >> 8;
}


void CHudMessage::MessageScanStart( void )
{
	switch( m_parms.pMessage->effect )
	{
	// Fade-in / out with flicker
	case 1:
	case 0:
		m_parms.fadeTime = m_parms.pMessage->fadein + m_parms.pMessage->holdtime;
		

		if ( m_parms.time < m_parms.pMessage->fadein )
		{
			m_parms.fadeBlend = ((m_parms.pMessage->fadein - m_parms.time) * (1.0/m_parms.pMessage->fadein) * 255);
		}
		else if ( m_parms.time > m_parms.fadeTime )
		{
			if ( m_parms.pMessage->fadeout > 0 )
				m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
			else
				m_parms.fadeBlend = 255; // Pure dest (off)
		}
		else
			m_parms.fadeBlend = 0;	// Pure source (on)
		m_parms.charTime = 0;

		if ( m_parms.pMessage->effect == 1 && (rand()%100) < 10 )
			m_parms.charTime = 1;
		break;

	case 2:
		m_parms.fadeTime = (m_parms.pMessage->fadein * m_parms.length) + m_parms.pMessage->holdtime;
		
		if ( m_parms.time > m_parms.fadeTime && m_parms.pMessage->fadeout > 0 )
			m_parms.fadeBlend = (((m_parms.time - m_parms.fadeTime) / m_parms.pMessage->fadeout) * 255);
		else
			m_parms.fadeBlend = 0;
		break;
	}
}

static bool Q_IsValidUChar32( unsigned int uVal )
{
	return ( uVal < 0x110000u ) && ( (uVal - 0x00D800u) > 0x7FFu ) && ( (uVal & 0xFFFFu) < 0xFFFEu ) && ( ( uVal - 0x00FDD0u ) > 0x1Fu );
}

static int Q_UTF8ToUChar32( const char *pUTF8_, unsigned int &uValueOut, bool &bErrorOut )
{
	const unsigned char *pUTF8 = (const unsigned char *)pUTF8_;

	int nBytes = 1;
	unsigned int uValue = pUTF8[0];
	unsigned int uMinValue = 0;

	// 0....... single byte
	if ( uValue < 0x80 )
		goto decodeFinishedNoCheck;

	// Expecting at least a two-byte sequence with 0xC0 <= first <= 0xF7 (110...... and 11110...)
	if ( (uValue - 0xC0u) > 0x37u || ( pUTF8[1] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0xC0 << 6) + pUTF8[1] - 0x80;
	nBytes = 2;
	uMinValue = 0x80;

	// 110..... two-byte lead byte
	if ( !( uValue & (0x20 << 6) ) )
		goto decodeFinished;

	// Expecting at least a three-byte sequence
	if ( ( pUTF8[2] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0x20 << 12) + pUTF8[2] - 0x80;
	nBytes = 3;
	uMinValue = 0x800;

	// 1110.... three-byte lead byte
	if ( !( uValue & (0x10 << 12) ) )
		goto decodeFinishedMaybeCESU8;

	// Expecting a four-byte sequence, longest permissible in UTF-8
	if ( ( pUTF8[3] & 0xC0 ) != 0x80 )
		goto decodeError;

	uValue = (uValue << 6) - (0x10 << 18) + pUTF8[3] - 0x80;
	nBytes = 4;
	uMinValue = 0x10000;

	// 11110... four-byte lead byte. fall through to finished.

decodeFinished:
	if ( uValue >= uMinValue && Q_IsValidUChar32( uValue ) )
	{
decodeFinishedNoCheck:
	uValueOut = uValue;
	bErrorOut = false;
	return nBytes;
	}
decodeError:
	uValueOut = '?';
	bErrorOut = true;
	return nBytes;

decodeFinishedMaybeCESU8:
	// Do we have a full UTF-16 surrogate pair that's been UTF-8 encoded afterwards?
	// That is, do we have 0xD800-0xDBFF followed by 0xDC00-0xDFFF? If so, decode it all.
	if ( ( uValue - 0xD800u ) < 0x400u && pUTF8[3] == 0xED && (unsigned char)( pUTF8[4] - 0xB0 ) < 0x10 && ( pUTF8[5] & 0xC0 ) == 0x80 )
	{
		uValue = 0x10000 + ( ( uValue - 0xD800u ) << 10 ) + ( (unsigned char)( pUTF8[4] - 0xB0 ) << 6 ) + pUTF8[5] - 0x80;
		nBytes = 6;
		uMinValue = 0x10000;
	}
	goto decodeFinished;
}

static bool Q_UnicodeValidate( const char *pUTF8 )
{
	bool bError = false;
	while ( *pUTF8 )
	{
		unsigned int uVal;
		// Our UTF-8 decoder silently fixes up 6-byte CESU-8 (improperly re-encoded UTF-16) sequences.
		// However, these are technically not valid UTF-8. So if we eat 6 bytes at once, it's an error.
		int nCharSize = Q_UTF8ToUChar32( pUTF8, uVal, bError );
		if ( bError || nCharSize == 6 )
			return false;
		pUTF8 += nCharSize;
	}
	return true;
}

void CHudMessage::MessageDrawScan( client_textmessage_t *pMessage, float time )
{
	int i, j, length, width;
	const char *pText;
	unsigned char line[80];

	pText = pMessage->pMessage;
	// Count lines
	m_parms.lines = 1;
	m_parms.time = time;
	m_parms.pMessage = pMessage;
	length = 0;
	width = 0;
	m_parms.totalWidth = 0;
	m_parms.totalHeight = 0;

	char consoleStringBuf[512] = {0};
	unsigned int consoleBufIndex = 0;

	bool useConsoleFont = false;
	const char* pCheck = pText;
	while(*pCheck)
	{
		if (*pCheck > 127 || *pCheck < 0)
		{
			useConsoleFont = Q_UnicodeValidate(pCheck);
			break;
		}
		pCheck++;
	}

	while ( *pText )
	{
		if ( *pText == '\n' )
		{
			m_parms.lines++;
			if (useConsoleFont)
			{
				consoleStringBuf[consoleBufIndex] = '\0';
				consoleBufIndex = 0;

				int height;
				gEngfuncs.pfnDrawConsoleStringLen(consoleStringBuf, &width, &height);
				m_parms.totalHeight += height;
			}
			if ( width > m_parms.totalWidth )
				m_parms.totalWidth = width;
			width = 0;
		}
		else
		{
			if (useConsoleFont) {
				if (consoleBufIndex < sizeof(consoleStringBuf)-1)
					consoleStringBuf[consoleBufIndex++] = *pText;
			} else {
				width += gHUD.m_scrinfo.charWidths[*pText];
			}
		}
		pText++;
		length++;
	}
	m_parms.length = length;
	if (!useConsoleFont)
	{
		m_parms.totalHeight = (m_parms.lines * gHUD.m_scrinfo.iCharHeight);
	}

	m_parms.y = YPosition( pMessage->y, m_parms.totalHeight );
	pText = pMessage->pMessage;

	m_parms.charTime = 0;

	MessageScanStart();

	consoleBufIndex = 0;
	for ( i = 0; i < m_parms.lines; i++ )
	{
		m_parms.lineLength = 0;
		m_parms.width = 0;
		while ( *pText && *pText != '\n' )
		{
			unsigned char c = *pText;
			if (!useConsoleFont)
				line[m_parms.lineLength] = c;
			if (useConsoleFont)
			{
				if (consoleBufIndex < sizeof(consoleStringBuf)-1)
					consoleStringBuf[consoleBufIndex++] = *pText;
			}
			else
			{
				m_parms.width += gHUD.m_scrinfo.charWidths[c];
			}
			m_parms.lineLength++;
			pText++;
		}
		pText++;		// Skip LF
		if (!useConsoleFont)
			line[m_parms.lineLength] = 0;

		int strHeight;
		if (useConsoleFont) {
			consoleStringBuf[consoleBufIndex] = '\0';
			consoleBufIndex = 0;

			int strLength;
			gEngfuncs.pfnDrawConsoleStringLen( consoleStringBuf, &strLength, &strHeight );
			m_parms.width = strLength;
		} else {
			strHeight = gHUD.m_scrinfo.iCharHeight;
		}

		m_parms.x = XPosition( pMessage->x, m_parms.width, m_parms.totalWidth );

		if (useConsoleFont) {
			SetColorParams(true);
			gEngfuncs.pfnDrawSetTextColor(m_parms.r/255.0f, m_parms.g/255.0f, m_parms.b/255.0f);
			gEngfuncs.pfnDrawConsoleString(m_parms.x, m_parms.y, consoleStringBuf);
		} else {
			for ( j = 0; j < m_parms.lineLength; j++ )
			{
				m_parms.text = line[j];
				int next = m_parms.x + gHUD.m_scrinfo.charWidths[ m_parms.text ];
				MessageScanNextChar();

				if ( m_parms.x >= 0 && m_parms.y >= 0 && next <= ScreenWidth )
					TextMessageDrawChar( m_parms.x, m_parms.y, m_parms.text, m_parms.r, m_parms.g, m_parms.b );
				m_parms.x = next;
			}
		}
		m_parms.y += strHeight;
	}
}


int CHudMessage::Draw( float fTime )
{
	int i, drawn;
	client_textmessage_t *pMessage;
	float endTime;

	drawn = 0;

	if ( m_gameTitleTime > 0 )
	{
		float localTime = gHUD.m_flTime - m_gameTitleTime;
		float brightness;

		// Maybe timer isn't set yet
		if ( m_gameTitleTime > gHUD.m_flTime )
			m_gameTitleTime = gHUD.m_flTime;

		if ( localTime > (m_pGameTitle->fadein + m_pGameTitle->holdtime + m_pGameTitle->fadeout) )
			m_gameTitleTime = 0;
		else
		{
			brightness = FadeBlend( m_pGameTitle->fadein, m_pGameTitle->fadeout, m_pGameTitle->holdtime, localTime );

			int halfWidth = gHUD.GetSpriteRect(m_HUD_title_half).right - gHUD.GetSpriteRect(m_HUD_title_half).left;
			int fullWidth = halfWidth + gHUD.GetSpriteRect(m_HUD_title_life).right - gHUD.GetSpriteRect(m_HUD_title_life).left;
			int fullHeight = gHUD.GetSpriteRect(m_HUD_title_half).bottom - gHUD.GetSpriteRect(m_HUD_title_half).top;

			int x = XPosition( m_pGameTitle->x, fullWidth, fullWidth );
			int y = YPosition( m_pGameTitle->y, fullHeight );


			SPR_Set( gHUD.GetSprite(m_HUD_title_half), brightness * m_pGameTitle->r1, brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1 );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(m_HUD_title_half) );

			SPR_Set( gHUD.GetSprite(m_HUD_title_life), brightness * m_pGameTitle->r1, brightness * m_pGameTitle->g1, brightness * m_pGameTitle->b1 );
			SPR_DrawAdditive( 0, x + halfWidth, y, &gHUD.GetSpriteRect(m_HUD_title_life) );

			drawn = 1;
		}
	}
	// Fixup level transitions
	for ( i = 0; i < maxHUDMessages; i++ )
	{
		// Assume m_parms.time contains last time
		if ( m_pMessages[i] )
		{
			pMessage = m_pMessages[i];
			if ( m_startTime[i] > gHUD.m_flTime )
				m_startTime[i] = gHUD.m_flTime + m_parms.time - m_startTime[i] + 0.2;	// Server takes 0.2 seconds to spawn, adjust for this
		}
	}

	for ( i = 0; i < maxHUDMessages; i++ )
	{
		if ( m_pMessages[i] )
		{
			pMessage = m_pMessages[i];

			// This is when the message is over
			switch( pMessage->effect )
			{
			case 0:
			case 1:
				endTime = m_startTime[i] + pMessage->fadein + pMessage->fadeout + pMessage->holdtime;
				break;
			
			// Fade in is per character in scanning messages
			case 2:
				endTime = m_startTime[i] + (pMessage->fadein * strlen( pMessage->pMessage )) + pMessage->fadeout + pMessage->holdtime;
				break;
			}

			if ( fTime <= endTime )
			{
				float messageTime = fTime - m_startTime[i];

				// Draw the message
				// effect 0 is fade in/fade out
				// effect 1 is flickery credits
				// effect 2 is write out (training room)
				MessageDrawScan( pMessage, messageTime );

				drawn++;
			}
			else
			{
				// The message is over
				m_pMessages[i] = NULL;
			}
		}
	}

	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;
	// Don't call until we get another message
	if ( !drawn )
		m_iFlags &= ~HUD_ACTIVE;

	return 1;
}


void CHudMessage::MessageAdd( const char *pName, float time )
{
	int i,j;
	client_textmessage_t *tempMessage;

	for ( i = 0; i < maxHUDMessages; i++ )
	{
		if ( !m_pMessages[i] )
		{
			// Trim off a leading # if it's there
			if ( pName[0] == '#' ) 
				tempMessage = TextMessageGet( pName+1 );
			else
				tempMessage = TextMessageGet( pName );
			// If we couldnt find it in the titles.txt, just create it
			if ( !tempMessage )
			{
				g_pCustomMessage.effect = 2;
				g_pCustomMessage.r1 = g_pCustomMessage.g1 = g_pCustomMessage.b1 = g_pCustomMessage.a1 = 100;
				g_pCustomMessage.r2 = 240;
				g_pCustomMessage.g2 = 110;
				g_pCustomMessage.b2 = 0;
				g_pCustomMessage.a2 = 0;
				g_pCustomMessage.x = -1;		// Centered
				g_pCustomMessage.y = 0.7;
				g_pCustomMessage.fadein = 0.01;
				g_pCustomMessage.fadeout = 1.5;
				g_pCustomMessage.fxtime = 0.25;
				g_pCustomMessage.holdtime = 5;
				g_pCustomMessage.pName = g_pCustomName;
				strcpy( g_pCustomText, pName );
				g_pCustomMessage.pMessage = g_pCustomText;

				tempMessage = &g_pCustomMessage;
			}

			for ( j = 0; j < maxHUDMessages; j++ )
			{
				if ( m_pMessages[j] )
				{
					// is this message already in the list
					if ( !strcmp( tempMessage->pMessage, m_pMessages[j]->pMessage ) )
					{
						return;
					}

					// get rid of any other messages in same location (only one displays at a time)
					if ( fabs( tempMessage->y - m_pMessages[j]->y ) < 0.0001 )
					{
						if ( fabs( tempMessage->x - m_pMessages[j]->x ) < 0.0001 )
						{
							m_pMessages[j] = NULL;
						}
					}
				}
			}

			m_pMessages[i] = tempMessage;
			m_startTime[i] = time;
			return;
		}
	}
}


int CHudMessage::MsgFunc_HudText( const char *pszName,  int iSize, void *pbuf )
{
	BEGIN_READ( pbuf, iSize );

	char *pString = READ_STRING();

	MessageAdd( pString, gHUD.m_flTime );
	// Remember the time -- to fix up level transitions
	m_parms.time = gHUD.m_flTime;

	// Turn on drawing
	if ( !(m_iFlags & HUD_ACTIVE) )
		m_iFlags |= HUD_ACTIVE;

	return 1;
}


int CHudMessage::MsgFunc_GameTitle( const char *pszName,  int iSize, void *pbuf )
{
	m_pGameTitle = TextMessageGet( "GAMETITLE" );
	if ( m_pGameTitle != NULL )
	{
		m_gameTitleTime = gHUD.m_flTime;

		// Turn on drawing
		if ( !(m_iFlags & HUD_ACTIVE) )
			m_iFlags |= HUD_ACTIVE;
	}

	return 1;
}

void CHudMessage::MessageAdd(client_textmessage_t * newMessage )
{
	m_parms.time = gHUD.m_flTime;

	// Turn on drawing
	if ( !(m_iFlags & HUD_ACTIVE) )
		m_iFlags |= HUD_ACTIVE;
	
	for ( int i = 0; i < maxHUDMessages; i++ )
	{
		if ( !m_pMessages[i] )
		{
			m_pMessages[i] = newMessage;
			m_startTime[i] = gHUD.m_flTime;
			return;
		}
	}

}
