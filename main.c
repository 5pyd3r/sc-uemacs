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
  const char *scriptfile = (char *)0;
  const char *programfile = (char *)0;
  const char *libdirs = (char *)0;
  const char *libexts = (char *)0;
  int status;
  const char *arg;
  int eoc = 0;
  int optlevel = 0;
  int debug_on_exception = 0;
  int import_notify = 0;
  int compile_imported_libraries = 0;

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
    } else if (strcmp(arg,"--retain-static-relocation") == 0) {
      Sretain_static_relocation();
    } else if (strcmp(arg,"--enable-object-counts") == 0) {
      eoc = 1;
    } else if (strcmp(arg,"--optimize-level") == 0) {
      const char *nextarg;
      if (++n == argc) {
        (void) fprintf(stderr,"%s requires argument\n", arg);
        exit(1);
      }
      nextarg = argv[n];
      if (strcmp(nextarg,"0") == 0)
        optlevel = 0;
      else if (strcmp(nextarg,"1") == 0)
        optlevel = 1;
      else if (strcmp(nextarg,"2") == 0)
        optlevel = 2;
      else if (strcmp(nextarg,"3") == 0)
        optlevel = 3;
      else {
        (void) fprintf(stderr,"invalid optimize-level %s\n", nextarg);
        exit(1);
      }
    } else if (strcmp(arg,"--debug-on-exception") == 0) {
      debug_on_exception = 1;
    } else if (strcmp(arg,"--import-notify") == 0) {
      import_notify = 1;
    } else if (strcmp(arg,"--libexts") == 0) {
      if (++n == argc) {
        (void) fprintf(stderr,"%s requires argument\n", arg);
        exit(1);
      }
      libexts = argv[n];
    } else if (strcmp(arg,"--libdirs") == 0) {
      if (++n == argc) {
        (void) fprintf(stderr,"%s requires argument\n", arg);
        exit(1);
      }
      libdirs = argv[n];
    } else if (strcmp(arg,"--compile-imported-libraries") == 0) {
      compile_imported_libraries = 1;
    } else if (strcmp(arg,"--help") == 0) {
      fprintf(stderr,"usage: %s [options and files]\n", execpath);
      fprintf(stderr,"options:\n");
#define sep ":"
      fprintf(stderr,"  --libdirs <dir>%s...                     set library directories\n", sep);
      fprintf(stderr,"  --libexts <ext>%s...                     set library extensions\n", sep);
      fprintf(stderr,"  --compile-imported-libraries            compile libraries before loading\n");
      fprintf(stderr,"  --import-notify                         enable import search messages\n");
      fprintf(stderr,"  --optimize-level <0 | 1 | 2 | 3>        set optimize-level\n");
      fprintf(stderr,"  --debug-on-exception                    on uncaught exception, call debug\n");
      fprintf(stderr,"  --enable-object-counts                  have collector maintain object counts\n");
      fprintf(stderr,"  --retain-static-relocation              keep reloc info for compute-size, etc.\n");
      fprintf(stderr,"  --help                                  print help and exit\n");
      fprintf(stderr,"  --                                      pass through remaining args\n");
      exit(0);
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
  if (eoc) {
    CALL1("enable-object-counts", Strue);
  }
  if (optlevel != 0) {
    CALL1("optimize-level", Sinteger(optlevel));
  }
  if (debug_on_exception != 0) {
    CALL1("debug-on-exception", Strue);
  }
  if (import_notify != 0) {
    CALL1("import-notify", Strue);
  }
  if (libdirs == 0) libdirs = getenv("CHEZSCHEMELIBDIRS");
  if (libdirs != 0) {
    CALL1("library-directories", Sstring(libdirs));
  }
  if (libexts == 0) libexts = getenv("CHEZSCHEMELIBEXTS");
  if (libexts != 0) {
    CALL1("library-extensions", Sstring(libexts));
  }
  if (compile_imported_libraries != 0) {
    CALL1("compile-imported-libraries", Strue);
  }

  if (scriptfile != (char *)0)
   /* Sscheme_script invokes the value of the scheme-script parameter */
    status = Sscheme_script(scriptfile, new_argc, argv);
  else if (programfile != (char *)0)
   /* Sscheme_script invokes the value of the scheme-script parameter */
    status = Sscheme_program(programfile, new_argc, argv);
  else {
   /* Sscheme_start invokes the value of the scheme-start parameter */
    status = Sscheme_start(new_argc, argv);
  }

 /* must call Scheme_deinit after saving the heap and before exiting */
  Sscheme_deinit();

  exit(status);
}
