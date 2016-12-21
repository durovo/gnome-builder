/* ide-build-stage.c
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

#define G_LOG_DOMAIN "ide-build-stage"

#include "buildsystem/ide-build-pipeline.h"
#include "buildsystem/ide-build-stage.h"
#include "subprocess/ide-subprocess.h"

typedef struct
{
  gchar               *name;
  IdeBuildLogObserver  observer;
  gpointer             observer_data;
  GDestroyNotify       observer_data_destroy;
  guint                completed : 1;
  guint                transient : 1;
} IdeBuildStagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (IdeBuildStage, ide_build_stage, IDE_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_COMPLETED,
  PROP_NAME,
  PROP_TRANSIENT,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];

typedef struct
{
  IdeBuildStage     *self;
  IdeBuildLogStream  stream_type;
} Tail;

static Tail *
tail_new (IdeBuildStage     *self,
          IdeBuildLogStream  stream_type)
{
  Tail *tail;

  tail = g_slice_new0 (Tail);
  tail->self = g_object_ref (self);
  tail->stream_type = stream_type;

  return tail;
}

static void
tail_free (Tail *tail)
{
  g_clear_object (&tail->self);
  g_slice_free (Tail, tail);
}

static void
ide_build_stage_clear_observer (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);
  GDestroyNotify notify = priv->observer_data_destroy;
  gpointer data = priv->observer_data;

  priv->observer_data_destroy = NULL;
  priv->observer_data = NULL;
  priv->observer = NULL;

  if (notify != NULL)
    notify (data);
}

static gboolean
ide_build_stage_real_execute (IdeBuildStage     *self,
                              IdeBuildPipeline  *pipeline,
                              GCancellable      *cancellable,
                              GError           **error)
{
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  return TRUE;
}

static void
ide_build_stage_real_execute_worker (GTask        *task,
                                     gpointer      source_object,
                                     gpointer      task_data,
                                     GCancellable *cancellable)
{
  IdeBuildStage *self = source_object;
  IdeBuildPipeline *pipeline = task_data;
  g_autoptr(GError) error = NULL;

  g_assert (G_IS_TASK (task));
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  if (IDE_BUILD_STAGE_GET_CLASS (self)->execute (self, pipeline, cancellable, &error))
    g_task_return_boolean (task, TRUE);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

static void
ide_build_stage_real_execute_async (IdeBuildStage       *self,
                                    IdeBuildPipeline    *pipeline,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));
  g_assert (IDE_IS_BUILD_PIPELINE (pipeline));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_source_tag (task, ide_build_stage_real_execute_async);
  g_task_set_task_data (task, g_object_ref (pipeline), g_object_unref);
  g_task_run_in_thread (task, ide_build_stage_real_execute_worker);
}

static gboolean
ide_build_stage_real_execute_finish (IdeBuildStage  *self,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_boolean (G_TASK (result), error);
}

const gchar *
ide_build_stage_get_name (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), NULL);

  return priv->name;
}

void
ide_build_stage_set_name (IdeBuildStage *self,
                          const gchar   *name)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  if (g_strcmp0 (name, priv->name) != 0)
    {
      g_free (priv->name);
      priv->name = g_strdup (name);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_NAME]);
    }
}

static void
ide_build_stage_finalize (GObject *object)
{
  IdeBuildStage *self = (IdeBuildStage *)object;
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  ide_build_stage_clear_observer (self);

  g_clear_pointer (&priv->name, g_free);

  G_OBJECT_CLASS (ide_build_stage_parent_class)->finalize (object);
}

static void
ide_build_stage_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  IdeBuildStage *self = IDE_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, ide_build_stage_get_name (self));
      break;

    case PROP_COMPLETED:
      g_value_set_boolean (value, ide_build_stage_get_completed (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  IdeBuildStage *self = IDE_BUILD_STAGE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      ide_build_stage_set_name (self, g_value_get_string (value));
      break;

    case PROP_COMPLETED:
      ide_build_stage_set_completed (self, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
ide_build_stage_class_init (IdeBuildStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = ide_build_stage_finalize;
  object_class->get_property = ide_build_stage_get_property;
  object_class->set_property = ide_build_stage_set_property;

  klass->execute = ide_build_stage_real_execute;
  klass->execute_async = ide_build_stage_real_execute_async;
  klass->execute_finish = ide_build_stage_real_execute_finish;

  /**
   * IdeBuildStage:completed:
   *
   * The "completed" property is set to %TRUE after the pipeline has
   * completed processing the stage. When the pipeline invalidates
   * phases, completed may be reset to %FALSE.
   */
  properties [PROP_COMPLETED] =
    g_param_spec_boolean ("completed",
                          "Completed",
                          "If the stage has been completed",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:name:
   *
   * The name of the build stage. This is only used by UI to view
   * the build pipeline.
   */
  properties [PROP_NAME] =
    g_param_spec_string ("name",
                         "Name",
                         "The user visible name of the stage",
                         NULL,
                         (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * IdeBuildStage:transient:
   *
   * If the build stage is transient.
   *
   * A transient build stage is removed after the completion of
   * ide_build_pipeline_execute_async(). This can be a convenient
   * way to add a temporary item to a build pipeline that should
   * be immediately discarded.
   */
  properties [PROP_TRANSIENT] =
    g_param_spec_boolean ("transient",
                          "Transient",
                          "If the stage should be removed after execution",
                          FALSE,
                          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
ide_build_stage_init (IdeBuildStage *self)
{
}

void
ide_build_stage_execute_async (IdeBuildStage       *self,
                               IdeBuildPipeline    *pipeline,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
  g_autoptr(GTask) task = NULL;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_BUILD_PIPELINE (pipeline));
  g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

  IDE_BUILD_STAGE_GET_CLASS (self)->execute_async (self, pipeline, cancellable, callback, user_data);
}

gboolean
ide_build_stage_execute_finish (IdeBuildStage  *self,
                                GAsyncResult   *result,
                                GError        **error)
{
  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  return IDE_BUILD_STAGE_GET_CLASS (self)->execute_finish (self, result, error);
}

/**
 * ide_build_stage_set_log_observer:
 * @self: An #IdeBuildStage
 * @observer: (scope async): The observer for the log entries
 * @observer_data: data for @observer
 * @observer_data_destroy: destroy callback for @observer_data
 *
 * Sets the log observer to handle calls to the various stage logging
 * functions. This will be set by the pipeline to mux logs from all
 * stages into a unified build log.
 *
 * Plugins that need to handle logging from a build stage should set
 * an observer on the pipeline so that log distribution may be fanned
 * out to all observers.
 */
void
ide_build_stage_set_log_observer (IdeBuildStage       *self,
                                  IdeBuildLogObserver  observer,
                                  gpointer             observer_data,
                                  GDestroyNotify       observer_data_destroy)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  ide_build_stage_clear_observer (self);

  priv->observer = observer;
  priv->observer_data = observer_data;
  priv->observer_data_destroy = observer_data_destroy;
}

void
ide_build_stage_log (IdeBuildStage        *self,
                     IdeBuildLogStream     stream,
                     const gchar          *message,
                     gssize                message_len)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  if G_LIKELY (priv->observer)
    priv->observer (stream, message, message_len, priv->observer_data);
}

gboolean
ide_build_stage_get_completed (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->completed;
}

void
ide_build_stage_set_completed (IdeBuildStage *self,
                               gboolean       completed)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  completed = !!completed;

  if (completed != priv->completed)
    {
      priv->completed = completed;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_COMPLETED]);
    }
}

void
ide_build_stage_set_transient (IdeBuildStage *self,
                               gboolean       transient)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));

  transient = !!transient;

  if (priv->transient != transient)
    {
      priv->transient = transient;
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TRANSIENT]);
    }
}

gboolean
ide_build_stage_get_transient (IdeBuildStage *self)
{
  IdeBuildStagePrivate *priv = ide_build_stage_get_instance_private (self);

  g_return_val_if_fail (IDE_IS_BUILD_STAGE (self), FALSE);

  return priv->transient;
}

static void
ide_build_stage_observe_stream_cb (GObject      *object,
                                   GAsyncResult *result,
                                   gpointer      user_data)
{
  GDataInputStream *stream = (GDataInputStream *)object;
  g_autofree gchar *line = NULL;
  Tail *tail = user_data;
  gsize n_read = 0;

  g_assert (G_IS_DATA_INPUT_STREAM (stream));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (tail != NULL);

  line = g_data_input_stream_read_line_finish_utf8 (stream, result, &n_read, NULL);

  if G_LIKELY (line != NULL && n_read > 0 && n_read < G_MAXSSIZE)
    {
      ide_build_stage_log (tail->self, tail->stream_type, line, (gssize)n_read);
      g_data_input_stream_read_line_async (stream,
                                           G_PRIORITY_DEFAULT,
                                           NULL,
                                           ide_build_stage_observe_stream_cb,
                                           tail);
      return;
    }

  tail_free (tail);
}


static void
ide_build_stage_observe_stream (IdeBuildStage     *self,
                                IdeBuildLogStream  stream_type,
                                GInputStream      *stream)
{
  g_autoptr(GDataInputStream) data_stream = NULL;
  Tail *tail;

  g_assert (IDE_IS_BUILD_STAGE (self));
  g_assert (stream_type == IDE_BUILD_LOG_STDOUT || stream_type == IDE_BUILD_LOG_STDERR);
  g_assert (G_IS_INPUT_STREAM (stream));

  if (G_IS_DATA_INPUT_STREAM (stream))
    data_stream = g_object_ref (stream);
  else
    data_stream = g_data_input_stream_new (stream);

  tail = tail_new (self, stream_type);

  g_data_input_stream_read_line_async (data_stream,
                                       G_PRIORITY_DEFAULT,
                                       NULL,
                                       ide_build_stage_observe_stream_cb,
                                       tail);
}

/**
 * ide_build_stage_log_subprocess:
 * @self: An #IdeBuildStage
 * @subprocess: An #IdeSubprocess
 *
 * This function will begin logging @subprocess by reading from the
 * stdout and stderr streams of the subprocess. You must have created
 * the subprocess with %G_SUBPROCESS_FLAGS_STDERR_PIPE and
 * %G_SUBPROCESS_FLAGS_STDOUT_PIPE so that the streams may be read.
 */
void
ide_build_stage_log_subprocess (IdeBuildStage *self,
                                IdeSubprocess *subprocess)
{
  GInputStream *stdout_stream;
  GInputStream *stderr_stream;

  g_return_if_fail (IDE_IS_BUILD_STAGE (self));
  g_return_if_fail (IDE_IS_SUBPROCESS (subprocess));

  stderr_stream = ide_subprocess_get_stderr_pipe (subprocess);
  stdout_stream = ide_subprocess_get_stdout_pipe (subprocess);

  if (stderr_stream != NULL)
    ide_build_stage_observe_stream (self, IDE_BUILD_LOG_STDERR, stderr_stream);

  if (stdout_stream != NULL)
    ide_build_stage_observe_stream (self, IDE_BUILD_LOG_STDOUT, stdout_stream);
}
