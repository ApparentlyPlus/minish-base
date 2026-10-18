#pragma once

// An animation for the about command. play() never returns, kill it to stop.
struct art {
	const char *name;
	void (*play)(void);
};

const struct art *art_find(const char *name); // NULL if there's no such animation
const struct art *art_random(void);
const char *art_names(void); // "nyan|fork|..." for usage messages
