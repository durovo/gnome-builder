/* ide-autotools-pipeline-addin.c
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "ide-autotools-pipeline-addin"

#include "ide-autotools-autogen-stage.h"
#include "ide-autotools-build-system.h"
#include "ide-autotools-pipeline-addin.h"

static gboolean
register_autoreconf_stage (IdeAutotoolsPipelineAddin  *self,
                           IdeBuildPipeline           *pipeline,
                           GError                    **error)
{
  g_autofree gchar *configure_path = NULL;
  g_autoptr(IdeBuildStage) stage = NULL;
  IdeContext *context;
  const gchar *srcdir;
  gboolean completed;
  guint stage_id;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (self));
  configure_path = ide_build_pipeline_build_srcdir_path (pipeline, "configure", NULL);
  completed = g_file_test (configure_path, G_FILE_TEST_IS_REGULAR);
  srcdir = ide_build_pipeline_get_srcdir (pipeline);

  stage = g_object_new (IDE_TYPE_AUTOTOOLS_AUTOGEN_STAGE,
                        "completed", completed,
                        "context", context,
                        "srcdir", srcdir,
                        NULL);

  stage_id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_AUTOGEN, 0, stage);

  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_configure_stage (IdeAutotoolsPipelineAddin  *self,
                          IdeBuildPipeline           *pipeline,
                          GError                    **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeBuildStage) stage = NULL;
  IdeConfiguration *configuration;
  g_autofree gchar *configure_path = NULL;
  g_autofree gchar *makefile_path = NULL;
  const gchar *config_opts;
  gboolean completed = TRUE;
  guint stage_id;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  launcher = ide_build_pipeline_create_launcher (pipeline, error);

  if (launcher == NULL)
    return FALSE;

  configure_path = ide_build_pipeline_build_srcdir_path (pipeline, "configure", NULL);
  makefile_path = ide_build_pipeline_build_builddir_path (pipeline, "Makefile", NULL);
  ide_subprocess_launcher_push_argv (launcher, configure_path);
  ide_subprocess_launcher_set_cwd (launcher, ide_build_pipeline_get_builddir (pipeline));

  /*
   * Parse the configure options as defined in the build configuration and append
   * them to configure.
   */

  configuration = ide_build_pipeline_get_configuration (pipeline);
  config_opts = ide_configuration_get_config_opts (configuration);

  if (config_opts != NULL)
    {
      g_auto(GStrv) argv = NULL;
      gint argc;

      if (!g_shell_parse_argv (config_opts, &argc, &argv, error))
        return FALSE;

      for (guint i = 0; i < argc; i++)
        ide_subprocess_launcher_push_argv (launcher, argv[i]);
    }

  /*
   * If the Makefile exists within the builddir, we will assume the
   * project has been initially configured correctly. Otherwise, every
   * time the user opens the project they have to go through a full
   * re-configure and build.
   *
   * Should the user need to perform an autogen, a manual rebuild is
   * easily achieved so this seems to be the sensible default.
   *
   * If we were to do this "correctly", we would look at config.status to
   * match the "ac_cs_config" variable to what we set. However, that is
   * influenced by environment variables, so its a bit non-trivial.
   */
  completed = g_file_test (makefile_path, G_FILE_TEST_IS_REGULAR);

  stage = g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                        "completed", completed,
                        "context", ide_object_get_context (IDE_OBJECT (self)),
                        "launcher", launcher,
                        NULL);

  stage_id = ide_build_pipeline_connect (pipeline, IDE_BUILD_PHASE_AUTOGEN, 0, stage);

  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static gboolean
register_make_stage (IdeAutotoolsPipelineAddin  *self,
                     IdeBuildPipeline           *pipeline,
                     IdeRuntime                 *runtime,
                     IdeBuildPhase               phase,
                     const gchar                *target,
                     GError                    **error)
{
  g_autoptr(IdeSubprocessLauncher) launcher = NULL;
  g_autoptr(IdeBuildStage) stage = NULL;
  IdeConfiguration *configuration;
  g_autofree gchar *j = NULL;
  const gchar *make = "make";
  guint stage_id;
  gint parallel;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (IDE_IS_RUNTIME (runtime));

  launcher = ide_build_pipeline_create_launcher (pipeline, error);

  if (launcher == NULL)
    return FALSE;

  if (ide_runtime_contains_program_in_path (runtime, "gmake", NULL))
    make = "gmake";

  parallel = ide_configuration_get_parallelism (configuration);

  if (parallel == -1)
    j = g_strdup_printf ("-j%u", g_get_num_processors () + 1);
  else if (parallel == 0)
    j = g_strdup_printf ("-j%u", g_get_num_processors ());
  else
    j = g_strdup_printf ("-j%u", parallel);

  ide_subprocess_launcher_set_cwd (launcher, ide_build_pipeline_get_builddir (pipeline));

  ide_subprocess_launcher_push_argv (launcher, make);
  ide_subprocess_launcher_push_argv (launcher, target);
  ide_subprocess_launcher_push_argv (launcher, j);

  stage_id = ide_build_pipeline_connect_launcher (pipeline, phase, 0, launcher);

  ide_build_pipeline_addin_track (IDE_BUILD_PIPELINE_ADDIN (self), stage_id);

  return TRUE;
}

static void
ide_autotools_pipeline_addin_load (IdeBuildPipelineAddin *addin,
                                   IdeBuildPipeline      *pipeline)
{
  IdeAutotoolsPipelineAddin *self = (IdeAutotoolsPipelineAddin *)addin;
  g_autoptr(GError) error = NULL;
  IdeConfiguration *config;
  IdeBuildSystem *build_system;
  IdeContext *context;
  IdeRuntime *runtime;

  g_assert (IDE_IS_AUTOTOOLS_PIPELINE_ADDIN (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  context = ide_object_get_context (IDE_OBJECT (addin));
  build_system = ide_context_get_build_system (context);
  config = ide_build_pipeline_get_configuration (pipeline);
  runtime = ide_configuration_get_runtime (config);

  if (!IDE_IS_AUTOTOOLS_BUILD_SYSTEM (build_system))
    return;

  if (!register_autoreconf_stage (self, pipeline, &error) ||
      !register_configure_stage (self, pipeline, &error) ||
      !register_make_stage (self, pipeline, runtime, IDE_BUILD_PHASE_BUILD, "all", &error) ||
      !register_make_stage (self, pipeline, runtime, IDE_BUILD_PHASE_INSTALL, "install", &error))
    {
      g_assert (error != NULL);
      g_warning ("Failed to create autotools launcher: %s", error->message);
      return;
    }
}

/* GObject Boilerplate */

static void
addin_iface_init (IdeBuildPipelineAddinInterface *iface)
{
  iface->load = ide_autotools_pipeline_addin_load;
}

struct _IdeAutotoolsPipelineAddin { IdeObject parent; };

G_DEFINE_TYPE_WITH_CODE (IdeAutotoolsPipelineAddin, ide_autotools_pipeline_addin, IDE_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (IDE_TYPE_BUILD_PIPELINE_ADDIN, addin_iface_init))

static void
ide_autotools_pipeline_addin_class_init (IdeAutotoolsPipelineAddinClass *klass)
{
}

static void
ide_autotools_pipeline_addin_init (IdeAutotoolsPipelineAddin *self)
{
}
