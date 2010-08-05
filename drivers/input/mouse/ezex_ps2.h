/* 
 * Owen Kwon <pinebud77@hotmail.com>
 * modified from touchkit_ps for viliv S5 TS
 */

#ifndef _EZEX_PS2_H
#define _EZEX_PS2_H

#ifdef CONFIG_MOUSE_PS2_EZEX
int ezex_ps2_detect(struct psmouse *psmouse, int set_properties);
#else
static inline int ezex_ps2_detect(struct psmouse *psmouse,
				      int set_properties)
{
	return -ENOSYS;
}
#endif 

#endif
