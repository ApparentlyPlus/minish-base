#pragma once

struct art {
	const char *name;
	void (*play)(void);
};

const struct art *art_find(const char *name);
const struct art *art_random(void);
const char *art_names(void);
