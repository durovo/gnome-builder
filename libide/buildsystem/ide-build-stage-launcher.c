/* ide-build-stage-launcher.c
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

#define G_LOG_DOMAIN "ide-build-stage-launcher"

#include "ide-debug.h"

#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-stage-launcher.h"
#include "subprocess/ide-subprocess.h"

struct _IdeBuildStageLauncher
{
  IdeBuildStage          parent_instance;
  IdeSubprocessLauncher *launcher;
};

enum {
  PROP_0,
  PROP_LAUNCHER,
  N_PROPS
};

G_DEFINE_TYPE (IdeBuildStageLauncher, ide_build_stage_launcher, IDE_TYPE_BUILD_STAGE)

static GParamSpec *properties [N_PROPS];

static void
ide_build_stage_launcher_wait_check_cb (GObject      *object,
                                        GAsyncResult *result,
                                        gpointer      user_data)
{
  IdeSubprocess *subprocess = (IdeSubprocess *)object;
  g_autoptr(GTask) task = user_data;
  g_autoptr(GError) error = NULL;

  IDE_ENTRY;

  g_assert (IDE_IS_SUBPROCESS (subprocess));
  g_assert (G_IS_ASYNC_RESULT (result));

  if (!ide_subprocess_wait_check_finish (subprocess, result, &error))
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  IDE_EXIT;
}

static void
ide_build_stage_launcher_execute_async (IdeBuildStage       *stage,
                                        IdeBuildPipeline    *pipeline,
                                        GCancellable        *cancellable,
                                        GAsyncReadyCallback  callback,
                                        gpointer             user_data)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)stage;
  g_autoptr(GTask) task = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(IdeSubprocess) subprocess = NULL;
  GSubprocessFlags flags;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_stage_launcher_execute_async);

  if (self->launcher == NULL)
    {
      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVAL,
                               "Improperly configured %s",
                               G_OBJECT_TYPE_NAME (self));
      IDE_EXIT;
    }

  flags = ide_subprocess_launcher_get_flags (self->launcher);

  /* Disable flags we do not want set for build pipeline stuff */

  if (flags & G_SUBPROCESS_FLAGS_STDERR_SILENCE)
    flags &= ~G_SUBPROCESS_FLAGS_STDERR_SILENCE;

  if (flags & G_SUBPROCESS_FLAGS_STDERR_MERGE)
    flags &= ~G_SUBPROCESS_FLAGS_STDERR_MERGE;

  if (flags & G_SUBPROCESS_FLAGS_STDIN_INHERIT)
    flags &= ~G_SUBPROCESS_FLAGS_STDIN_INHERIT;

  /* Ensure we have access to stdin/stdout streams */

  flags |= G_SUBPROCESS_FLAGS_STDOUT_PIPE;
  flags |= G_SUBPROCESS_FLAGS_STDERR_PIPE;

  ide_subprocess_launcher_set_flags (self->launcher, flags);

  /* Now launch the process */

  subprocess = ide_subprocess_launcher_spawn (self->launcher, cancellable, &error);

  if (subprocess == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      IDE_EXIT;
    }

  ide_build_stage_log_subprocess (IDE_BUILD_STAGE (self), subprocess);

  ide_subprocess_wait_check_async (subprocess,
                                   cancellable,
                                   ide_build_stage_launcher_wait_check_cb,
                                   g_steal_pointer (&task));

  IDE_EXIT;
}

static gboolean
ide_build_stage_launcher_execute_finish (IdeBuildStage  *stage,
                                         GAsyncResult   *result,
                                         GError        **error)
{
  gboolean ret;

  IDE_ENTRY;

  g_assert (IDE_IS_BUILD_STAGE_LAUNCHER (stage));
  g_assert (G_IS_TASK (result));

  ret = g_task_propagate_boolean (G_TASK (result), error);

  IDE_RETURN (ret);
}

static void
ide_build_stage_launcher_finalize (GObject *object)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;

  g_clear_object (&self->launcher);

  G_OBJECT_CLASS (ide_build_stage_launcher_parent_class)->finalize (object);
}

static void
ide_build_stage_launcher_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_LAUNCHER:
      g_value_set_object (value, ide_build_stage_launcher_get_launcher (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_launcher_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  IdeBuildStageLauncher *self = (IdeBuildStageLauncher *)object;

  switch (prop_id)
    {
    case PROP_LAUNCHER:
      self->launcher = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_launcher_class_init (IdeBuildStageLauncherClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  IdeBuildStageClass *build_stage_class = IDE_BUILD_STAGE_CLASS (klass);

  object_class->finalize = ide_build_stage_launcher_finalize;
  object_class->get_property = ide_build_stage_launcher_get_property;
  object_class->set_property = ide_build_stage_launcher_set_property;

  build_stage_class->execute_async = ide_build_stage_launcher_execute_async;
  build_stage_class->execute_finish = ide_build_stage_launcher_execute_finish;

  properties [PROP_LAUNCHER] =
    g_param_spec_object ("launcher",
                         "Launcher",
                         "The subprocess launcher to execute",
                         IDE_TYPE_SUBPROCESS_LAUNCHER,
                         (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_build_stage_launcher_init (IdeBuildStageLauncher *self)
{
}

/**
 * ide_build_stage_launcher_get_launcher:
 *
 * Returns: (transfer none): An #IdeSubprocessLauncher
 */
IdeSubprocessLauncher *
ide_build_stage_launcher_get_launcher (IdeBuildStageLauncher *self)
{
  g_return_val_if_fail (IDE_IS_BUILD_STAGE_LAUNCHER (self), NULL);

  return self->launcher;
}

IdeBuildStage *
ide_build_stage_launcher_new (IdeContext            *context,
                              IdeSubprocessLauncher *launcher)
{
  return g_object_new (IDE_TYPE_BUILD_STAGE_LAUNCHER,
                       "context", context,
                       "launcher", launcher,
                       NULL);
}
