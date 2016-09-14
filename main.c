/* main.c
 * Copyright 1984-2016 Cisco Systems, Inc.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/****
  This is the default custom.c file defining main, which must be present
  in order to build an executable file.

  See the file custom/sample.c for a customized variant of this file.
****/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "scheme.h"

extern void S_uemacs_init(void);

/****
  ABNORMAL_EXIT may be defined as a function with the signature shown to
  take some action, such as printing a special error message or performing
  a nonlocal exit with longjmp, when the Scheme system exits abnormally,
  i.e., when an unrecoverable error occurs.  If left null, the default
  is to call exit(1).
****/
#ifndef ABNORMAL_EXIT
#define ABNORMAL_EXIT ((void (*)(void))0)
#endif /* ABNORMAL_EXIT */

static const char *path_last(const char *p) {
  const char *s;
  for (s = p; *s != 0; s += 1) if (*s == '/') p = ++s;
  return p;
}

int main(int argc, const char *argv[]) {
  int n, new_argc = 1;
  const char *execpath = argv[0];
  int status;
  const char *arg;

  if (strcmp(Skernel_version(), VERSION) != 0) {
    (void) fprintf(stderr, "unexpected shared library version %s for %s version %s\n", Skernel_version(), execpath, VERSION);
    exit(1);
  }

  Sscheme_init(ABNORMAL_EXIT);

  /* process command-line arguments, registering boot and heap files */
  for (n = 1; n < argc; n += 1) {
    arg = argv[n];
    if (strcmp(arg,"--") == 0) {
      while (++n < argc) argv[new_argc++] = argv[n];
    } else {
      argv[new_argc++] = arg;
    }
  }

 /* must call Sbuild_heap after registering boot and heap files.
  * Sbuild_heap() completes the initialization of the Scheme system
  * and loads the boot or heap files.  If no boot or heap files have
  * been registered, the first argument to Sbuild_heap must be a
  * non-null path string; in this case, Sbuild_heap looks for
  * a heap or boot file named <name>.boot, where <name> is the last
  * component of the path.  If no heap files are loaded and
  * CUSTOM_INIT is non-null, Sbuild_heap calls CUSTOM_INIT just
  * prior to loading the boot file(s). */
  Sbuild_heap(execpath, S_uemacs_init);

#define CALL0(who) Scall0(Stop_level_value(Sstring_to_symbol(who)))
#define CALL1(who, arg) Scall1(Stop_level_value(Sstring_to_symbol(who)), arg)
  CALL1("suppress-greeting", Strue);
  CALL1("waiter-prompt-string", Sstring(""));

  Senable_expeditor("");
  
  /* Sscheme_start invokes the value of the scheme-start parameter */
  status = Sscheme_start(new_argc, argv);

 /* must call Scheme_deinit after saving the heap and before exiting */
  Sscheme_deinit();

  exit(status);
}
