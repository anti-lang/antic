/* The start of the runtime in src/rt/init.c, and the option of the
   backtrace that the configuration sets. */
#ifndef ANTI_RT_H
#define ANTI_RT_H

/* The state of the runtime. An executable initialises it in main. A
   static or a shared library for C initialises it in a constructor that
   runs when it loads, and a plugin takes the state of its host. A call
   after the first does nothing. */
void anti_rt_init(void);

/* 1 after anti_rt_init, else 0. */
int anti_rt_ready(void);

/* The key `backtrace` of the runtime configuration: 1 on, 0 off and -1
   when no layer named it. set_key of src/rt/conf.c writes it, from the
   command line, the file and rt.configure. */
extern int anti_rt_option_backtrace;

#endif
