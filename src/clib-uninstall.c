//
// clib-uninstall.c
//
// Copyright (c) 2014 Stephen Mathieson
// Copyright (c) 2021 clib authors
//
// MIT licensed
//

#include "asprintf/asprintf.h"
#include "commander/commander.h"
#include "http-get/http-get.h"
#include "logger/logger.h"
#include "parse-repo/parse-repo.h"
#include "parson/parson.h"
#include "version.h"
#include <stdlib.h>
#include <string.h>

#define CLIB_UNINSTALL_DEFAULT_TARGET "make uninstall"

static char *get_tarball_url(const char *owner, const char *name,
                             const char *version) {
  char *tarball = NULL;
  int size = 0;

  size = asprintf(&tarball, "https://github.com/%s/%s/archive/%s.tar.gz", owner,
                  name, version);

  if (-1 == size)
    return NULL;
  return tarball;
}

static char *get_tar_filepath(const char *name, const char *version) {
  char *file = NULL;
  int size = asprintf(&file, "%s-%s.tar.gz", name, version);
  if (-1 == size)
    return NULL;
  return file;
}

static char *get_tmp_tarball(const char *file) {
  char *tmp = NULL;
  int size = asprintf(&tmp, "/tmp/%s", file);
  if (-1 == size)
    return NULL;
  return tmp;
}

static char *get_untar_command(const char *file) {
  char *cmd = NULL;
  int size = 0;
  size = asprintf(&cmd, "cd /tmp && tar -xf %s", file);
  if (-1 == size)
    return NULL;
  return cmd;
}

static char *get_uninstall_target(const char *name, const char *version) {
  int size = 0;
  char *target = NULL;
  char *dir = NULL;
  char *pkg = NULL;
  const char *val = NULL;
  JSON_Value *root = NULL;
  JSON_Object *obj = NULL;

  size = asprintf(&dir, "/tmp/%s-%s", name, version);
  if (-1 == size)
    return NULL;

  size = asprintf(&pkg, "%s/package.json", dir);
  if (-1 == size)
    goto done;

  root = json_parse_file(pkg);
  if (!root)
    goto done;

  obj = json_value_get_object(root);
  if (!obj)
    goto done;

  val = json_object_get_string(obj, "uninstall");
  if (!val) {
    logger_warn("warning",
                "No uninstall target specified.  Defaulting to '%s'.",
                CLIB_UNINSTALL_DEFAULT_TARGET);
    // default to "make uninstall"
    val = CLIB_UNINSTALL_DEFAULT_TARGET;
  }

  size = asprintf(&target, "cd %s && %s", dir, val);
  if (-1 == size)
    return NULL;

done:
  if (root)
    json_value_free(root);
  free(dir);
  free(pkg);
  return target;
}

static int clib_uninstall(const char *owner, const char *name,
                          const char *version) {
  char *tarball = NULL;
  char *file = NULL;
  char *tarpath = NULL;
  char *cmd = NULL;
  char *target = NULL;
  int rc = -1;

  // sanity
  if (!owner || !name || !version)
    return -1;

  if (!(tarball = get_tarball_url(owner, name, version)))
    goto done;
  if (!(file = get_tar_filepath(name, version)))
    goto done;
  if (!(tarpath = get_tmp_tarball(file)))
    goto done;

  logger_info("fetch", tarball);
  if (-1 == http_get_file(tarball, tarpath)) {
    logger_error("error", "failed to fetch tarball");
    goto done;
  }

  if (!(cmd = get_untar_command(file)))
    goto done;

  logger_info("untar", tarpath);
  if (0 != system(cmd)) {
    logger_error("error", "failed to untar");
    goto done;
  }

  target = get_uninstall_target(name, version);
  if (!target)
    goto done;

  rc = system(target);

done:
  free(tarball);
  free(file);
  free(tarpath);
  free(cmd);
  free(target);
  return rc;
}

int main(int argc, char **argv) {
  int rc = 1;
  command_t program;

  command_init(&program, "clib-uninstall", CLIB_VERSION);
  program.usage = "[name ...]";
  command_parse(&program, argc, argv);

  if (0 == program.argc)
    command_help(&program);

  for (int i = 0; i < program.argc; i++) {
    char *owner = parse_repo_owner(program.argv[i], NULL);
    if (!owner)
      goto cleanup;
    char *name = parse_repo_name(program.argv[i]);
    if (!name) {
      free(owner);
      goto cleanup;
    }

    int res = clib_uninstall(owner, name, "master");
    free(owner);
    free(name);
    if (-1 == res) {
      logger_error("error", "Failed to uninstall %s", program.argv[i]);
      goto cleanup;
    }
  }

  rc = 0;

cleanup:
  command_free(&program);
  return rc;
}
