/* jd_exception.c */

#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "jd_private.h"
#include "jsondata.h"

jd_activation *jd_head = NULL;
jd_var jd_root_exception = JD_INIT;

jd_activation *jd_ar_push(int line, const char *file) {
  jd_activation *rec = jd_alloc(sizeof(jd_activation));
  rec->vars = NULL;
  rec->line = line;
  rec->file = file;
  rec->up = jd_head;
  return jd_head = rec;
}

jd_activation *jd_ar_pop(void) {
  jd_activation *rec = jd_head;
  if (!rec) jd_die("Exception stack underrun");
  jd_head = rec->up;
  return rec;
}

static void free_vars(jd_dvar *dv) {
  while (dv) {
    jd_dvar *next = dv->next;
    jd_release(&dv->v);
    jd_free(dv);
    dv = next;
  }
}

void jd_ar_free(jd_activation *rec) {
  free_vars(rec->vars);
  jd_free(rec);
}

void jd_ar_up(void) {
  jd_ar_free(jd_ar_pop());
}

jd_var *jd_catch(void) {
  jd_dvar *ex = jd_head->vars;
  jd_var *e = &ex->v;
  jd_head->vars = ex->next;
  if (jd_head->up) {
    ex->next = jd_head->up->vars;
    jd_head->up->vars = ex;
  }
  else {
    jd_assign(&jd_root_exception, &ex->v);
    jd_release(&ex->v);
    jd_free(ex);
    e = &jd_root_exception;
  }
  jd_ar_up();
  return e;
}

jd_var *jd_ar_var(jd_activation *rec) {
  jd_dvar *dv = jd_alloc(sizeof(jd_dvar));
  dv->next = rec->vars;
  rec->vars = dv;
  return &dv->v;
}

static void rethrow(jd_var *e, int release) {
  if (jd_head) {
    /*    printf("throw %s at %s:%d\n", jd_bytes(e, NULL), jd_head->file, jd_head->line);*/
    jd_assign(jd_ar_var(jd_head), e);
    if (release) jd_release(e);
    longjmp(jd_head->env, 1);
  }
  else {
    fprintf(stderr, "Uncaught exception: %s\n", jd_bytes(e, NULL));
    if (release) jd_release(e);
    exit(1);
  }
}

void jd_rethrow(jd_var *e) {
  rethrow(e, 0);
}

void jd_throw(const char *msg, ...) {
  jd_var e = JD_INIT;
  va_list ap;
  va_start(ap, msg);
  jd_vprintf(&e, msg, ap);
  va_end(ap);
  rethrow(&e, 1);
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */