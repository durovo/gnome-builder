/* gbp-flatpak-prepare-stage.c
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

#define G_LOG_DOMAIN "gbp-flatpak-prepare-stage"

#include <glib.h>
#include <glib/stdio.h>

#include "gbp-flatpak-prepare-stage.h"

struct _GbpFlatpakPrepareStage
{
  IdeBuildStage     parent_instance;
  IdeBuildPipeline *pipeline;
};

G_DEFINE_TYPE (GbpFlatpakPrepareStage, gbp_flatpak_prepare_stage, IDE_TYPE_BUILD_STAGE)

static gboolean
gbp_flatpak_prepare_stage_execute_async (IdeBuildStage  *stage,
                                         GCancellable   *cancellable,
                                         GError        **error)
{
  GbpFlatpakPrepareStage *self = (GbpFlatpakPrepareStage *)stage;
  g_autofree gchar *build_repo = NULL;

  g_assert (GBP_IS_FLATPAK_PREPARE_STAGE (self));
  g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

  build_repo = ide_build_pipeline_build_builddir_path (self->pipeline, "build");

  if (-1 == g_mkdir_with_parents (build_repo, 0750))
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   g_strerror (errno));
      return FALSE;
    }


}

static void
gbp_flatpak_prepare_stage_finalize (GObject *object)
{
  GbpFlatpakPrepareStage *self = (GbpFlatpakPrepareStage *)object;

  g_clear_object (&self->pipeline);

  G_OBJECT_CLASS (gbp_flatpak_prepare_stage_parent_class)->finalize (object);
}

static void
gbp_flatpak_prepare_stage_class_init (GbpFlatpakPrepareStageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_prepare_stage_finalize;

  klass->execute = gbp_flatpak_prepare_stage_execute;
}

static void
gbp_flatpak_prepare_stage_init (GbpFlatpakPrepareStage *self)
{
}

IdeBuildStage *
gbp_flatpak_prepare_stage_new (IdeBuildPipeline *pipeline)
{
  GbpFlatpakPrepareStage *self;
  IdeContext *context;

  g_return_val_if_fail (IDE_IS_BUILD_PIPELINE (pipeline), NULL);

  context = ide_object_get_context (IDE_OBJECT (pipeline));

  self = g_object_new (GBP_TYPE_FLATPAK_PREPARE_STAGE,
                       "context", context,
                       NULL);
  self->pipeline = g_object_ref (pipeline);

  return IDE_BUILD_STAGE (self);
}
