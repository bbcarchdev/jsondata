/* path.t */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "framework.h"
#include "tap.h"
#include "jd_test.h"
#include "jd_pretty.h"
#include "jd_path.h"

static int should_save(void) {
  return !!getenv("JD_TEST_WRITEBACK");
}

static jd_var *load_string(jd_var *out, const char *filename) {
  FILE *f;
  char buf[512];
  size_t got;

  if (f = fopen(filename, "r"), !f) jd_die("Can't read %s: %m", filename);
  jd_set_empty_string(out, 100);
  while (got = fread(buf, 1, sizeof(buf), f), got) {
    jd_append_bytes(out, buf, got);
  }
  fclose(f);
  return out;
}

static jd_var *load_json(jd_var *out, const char *filename) {
  jd_var json = JD_INIT;
  jd_from_json(out, load_string(&json, filename));
  jd_release(&json);
  return out;
}

static jd_var *save_json(jd_var *data, const char *filename) {
  scope {
    FILE *f;
    if (f = fopen(filename, "w"), !f) jd_die("Can't write %s: %m", filename);
    jd_fprintf(f, "%V", jd_to_json_pretty(jd_nv(), data));
    fclose(f);
  }
  return data;
}

static void test_toker(void) {
  scope {
    JD_AV(want, 10);
    JD_AV(got, 10);
    jd_var *tok;
    jd__path_parser p;
    JD_SV(path, "$..foo[33]['a key']");

    jd_set_array_with(jd_push(want, 1), jd_niv('$'), NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(JP_DOTDOT), NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(JP_KEY), jd_nsv("foo"),  NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv('['), NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(JP_SLICE), jd_niv(33),  NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(']'), NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv('['), NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(JP_KEY), jd_nsv("a key"),  NULL);
    jd_set_array_with(jd_push(want, 1), jd_niv(']'), NULL);

    jd__path_init_parser(&p, path);
    while (tok = jd__path_token(&p), tok)
      jd_assign(jd_push(got, 1), tok);

    jdt_is(got, want, "parse %V ok", path);
  }
}

static void test_compile(void) {
  scope {
    JD_SV(path1, "$.foo.*.12");
    JD_VAR(path2);

    jd_var *c1 = jd__path_compile(path1);
    not_null(c1, "compiled path not null");
    jd_assign(path2, path1);
    jd_var *c2 = jd__path_compile(path2);
    ok(c1 == c2, "string assign keeps magic");

    jd_set_string(path1, "$.bar");
    jd_var *c3 = jd__path_compile(path1);
    not_null(c1, "compiled path not null");
    ok(c3 != c1, "change string: new magic");
  }
}

static void test_parser(void) {
  scope {
    jd_var *path = jd_nsv("$.foo.*.12");
    jd_var *want = jd_njv("[ [\"$\"], [\"foo\"], [\"that\", \"this\"], [12] ]");
    jd_var *hash = jd_njv("{\"this\":12,\"that\":true}");
    jd_var *got = jd_nav(10);
    jd_var *comp = jd_get_idx(jd__path_compile(path), 0);
    jd_var *cl = jd_nv(), *tmp = jd_nv();
    size_t cnt = jd_count(comp);
    for (unsigned i = 0; i < cnt; i++) {
      JD_AV(alt, 10);
      jd_eval(jd_get_idx(comp, i), cl, hash); /* closure returns closure... */
      for (;;) {
        jd_eval(cl, tmp, NULL); /* ...which returns literal */
        if (tmp->type == VOID) break;
        jd_assign(jd_push(alt, 1), tmp);
      }
      jd_assign(jd_push(got, 1), alt);
    }
    jdt_is(got, want, "parsed %V", path);
  }
}

static void test_traverse(void) {
  scope {
    JD_JV(data, "{\"id\":[1,2,3]}");
    jdt_is_json(jd_traverse_path(data, jd_nsv("id"), 0), "[1,2,3]", "traverse hash");
  }
  scope {
    JD_JV(data, "[1,2,3]");
    jdt_is_json(jd_traverse_path(data, jd_niv(1), 0), "2", "traverse array");
  }
  scope {
    JD_VAR(data);
    jd_assign(jd_traverse_path(data, jd_nsv("id"), 1), jd_nsv("Bongo"));
    jdt_is_json(data, "{\"id\":\"Bongo\"}", "vivify hash");
  }
  scope {
    JD_VAR(data);
    jd_assign(jd_traverse_path(data, jd_niv(3), 1), jd_nsv("Bongo"));
    jdt_is_json(data, "[null,null,null,\"Bongo\"]", "vivify array");
  }
  scope {
    JD_VAR(data);
    jd_assign(jd_traverse_path(data, jd_nsv("1"), 1), jd_nsv("Bongo"));
    jdt_is_json(data, "[null,\"Bongo\"]", "vivify array, string key");
  }
  scope {
    JD_JV(data, "{\"id\":[1,2,3]}");
    JD_JV(path, "[\"id\",1]");
    jdt_is_json(jd_traverse_path(data, path, 0), "2", "traverse hash path");
  }
  scope {
    JD_JV(data, "{\"id\":[1,2,3]}");
    JD_JV(path, "[\"id\",\"1\"]");
    jdt_is_json(jd_traverse_path(data, path, 0), "2", "traverse hash path, string key");
  }
  scope {
    JD_VAR(data);
    JD_JV(path, "[\"id\",1]");
    jd_assign(jd_traverse_path(data, path, 1), jd_nsv("Boo!"));
    jdt_is_json(data, "{\"id\":[null,\"Boo!\"]}", "vivify path");
  }
  scope {
    JD_VAR(data);
    JD_JV(path, "[\"id\",\"1\"]");
    jd_assign(jd_traverse_path(data, path, 1), jd_nsv("Boo!"));
    jdt_is_json(data, "{\"id\":[null,\"Boo!\"]}", "vivify path, string key");
  }
}

static void test_traverse_array(void) {
  scope {
    jd_var *data = jd_njv("{\"foo\":[0,\"one\",{\"name\":\"two\"}]}");
    jd_var *path = jd_njv("[[],\"foo\"]");
    jd_var *got = jd_traverse_path(data, path, 0);
    jdt_is(got, jd_rv(data, "$.foo"), "traverse array");
  }
}

static void test_iter(const char *specfile) {
  scope {
    jd_var *model = jd_nv();
    jd_var *data = jd_nv();
    jd_var *iter = jd_nv();
    jd_var *cpath = jd_nv();
    jd_var *capture = jd_nv();
    jd_var *got = jd_nv();
    unsigned i;
    int writeback = should_save();

    subtest(specfile) {
      load_json(model, specfile);

      for (i = 0; i < jd_count(model); i++) {
        jd_var *tc = jd_get_idx(model, i);

        jd_var *in = jd_get_ks(tc, "in", 0);
        jd_var *out = jd_get_ks(tc, "out", 0);

        jd_var *path = jd_get_ks(in, "path", 0);
        jd_clone(data, jd_get_ks(in, "data", 0), 1);

        subtest(jd_bytes(path, NULL)) {
          jd_path_iter(iter, data, path, jd_test(jd_get_ks(in, "vivify", 0)));

          jd_set_array(got, 1);
          jd_var *slot;
          while (slot = jd_path_next(iter, cpath, capture), slot) {
            jd_set_array_with(jd_push(got, 1), cpath, capture, NULL);
            jd_assign(slot, cpath);
          }
          if (writeback) {
            jd_assign(jd_get_ks(out, "path", 1), got);
            jd_assign(jd_get_ks(out, "data", 1), data);
          }
          else {
            jdt_is(got, jd_get_ks(out, "path", 0), "iterated");
            jdt_is(data, jd_get_ks(out, "data", 0), "data structure");
          }
        }
      }
      if (writeback)
        save_json(model, specfile);
    }
  }
}

static void test_walk(const char *specfile) {
  scope {
    jd_var *model = jd_nv();
    jd_var *data = jd_nv();
    jd_var *iter = jd_nv();
    jd_var *cpath = jd_nv();
    jd_var *capture = jd_nv();
    jd_var *got = jd_nv();
    unsigned i;
    int writeback = should_save();

    subtest(specfile) {
      load_json(model, specfile);

      for (i = 0; i < jd_count(model); i++) {
        jd_var *tc = jd_get_idx(model, i);

        jd_var *in = jd_get_ks(tc, "in", 0);
        jd_var *out = jd_get_ks(tc, "out", 0);

        jd_var *path = jd_get_ks(in, "path", 0);
        jd_clone(data, jd_get_ks(in, "data", 0), 1);

        subtest(jd_bytes(path, NULL)) {
          jd_path_iter(iter, data, path, jd_test(jd_get_ks(in, "vivify", 0)));

          jd_set_array(got, 1);
          jd_var *slot;
          while (slot = jd_path_next(iter, cpath, capture), slot) {
            jd_set_array_with(jd_push(got, 1), cpath, capture, NULL);
          }
          if (writeback)
            jd_assign(jd_get_ks(out, "path", 1), got);
          else
            jdt_is(got, jd_get_ks(out, "path", 0), "iterated");
        }
      }
      if (writeback)
        save_json(model, specfile);
    }
  }
}

void test_main(void) {
  test_toker();
  test_parser();
  test_compile();
  test_traverse();
  test_traverse_array();
  test_iter("jsonpath.json");
  test_walk("walk.json");
}

/* vim:ts=2:sw=2:sts=2:et:ft=c
 */
