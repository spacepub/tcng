/*
 * target.h - Selection of output targets
 *
 * Written 2001 by Werner Almesberger
 * Copyright 2001 Werner Almesberger
 */

/*
 * NB: this is very very primitive. It really wants to be a mechanism to check
 * which code generators are available, which ones can generate the requested
 * element(s), and finally, how expensive their result is. @@@
 */

#ifndef TARGET_H
#define TARGET_H

struct ext_target {
    const char *name;
    struct ext_target *next;
};

extern struct ext_target *ext_targets;

void target_register(const char *element,const char *target,int on);
int have_target(const char *element,const char *target);
void switch_target(const char *arg); /* [element:][no]target */
void ext_target_register(const char *target);
void ext_targets_configure(void);

#endif /* TARGET_H */
