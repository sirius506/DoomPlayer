#ifndef __SDL_ENDIAN_H__
#define __SDL_ENDIAN_H__


#define SDL_BIG_ENDIAN		0
#define SDL_LITTLE_ENDIAN	1

#define	SDL_BYTEORDER 	SDL_LITTLE_ENDIAN

#define	SDL_SwapLE16(x)	(x)
#define	SDL_SwapLE32(x)	(x)
#endif
