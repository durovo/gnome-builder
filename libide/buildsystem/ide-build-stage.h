/* ide-build-stage.h
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

#ifndef IDE_BUILD_STAGE_H
#define IDE_BUILD_STAGE_H

#include <gio/gio.h>

#include "ide-types.h"
#include "ide-object.h"

#include "buildsystem/ide-build-log.h"

G_BEGIN_DECLS

#define IDE_TYPE_BUILD_STAGE (ide_build_stage_get_type())

G_DECLARE_DERIVABLE_TYPE (IdeBuildStage, ide_build_stage, IDE, BUILD_STAGE, IdeObject)

struct _IdeBuildStageClass
{
  IdeObjectClass parent_class;

  /**
   * IdeBuildStage::execute:
   *
   * This vfunc will be run in a thread by the default
   * IdeBuildStage::execute_async() and IdeBuildStage::execute_finish()
   * vfuncs.
   *
   * Only use thread-safe API from this function.
   */
  gboolean (*execute)        (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable,
                              GError              **error);

  /**
   * IdeBuildStage::execute_async:
   *
   * Asynchronous version of the #IdeBuildStage API. This is the preferred
   * way to subclass #IdeBuildStage.
   */
  void     (*execute_async)  (IdeBuildStage        *self,
                              IdeBuildPipeline     *pipeline,
                              GCancellable         *cancellable,
                              GAsyncReadyCallback   callback,
                              gpointer              user_data);

  /**
   * IdeBuildStage::execute_finish:
   *
   * Completes an asynchronous call to ide_build_stage_execute_async().
   *
   * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
   *   Upon failure, the pipeline will be stopped.
   */
  gboolean (*execute_finish) (IdeBuildStage        *self,
                              GAsyncResult         *result,
                              GError              **error);

  gpointer _reserved1;
  gpointer _reserved2;
  gpointer _reserved3;
  gpointer _reserved4;
  gpointer _reserved5;
  gpointer _reserved6;
  gpointer _reserved7;
  gpointer _reserved8;
  gpointer _reserved9;
  gpointer _reserved10;
  gpointer _reserved11;
  gpointer _reserved12;
};

const gchar   *ide_build_stage_get_name         (IdeBuildStage        *self);
void           ide_build_stage_set_name         (IdeBuildStage        *self,
                                                 const gchar          *name);
void           ide_build_stage_log              (IdeBuildStage        *self,
                                                 IdeBuildLogStream     stream,
                                                 const gchar          *message,
                                                 gssize                message_len);
void           ide_build_stage_log_subprocess   (IdeBuildStage        *self,
                                                 IdeSubprocess        *subprocess);
void           ide_build_stage_set_log_observer (IdeBuildStage        *self,
                                                 IdeBuildLogObserver   observer,
                                                 gpointer              observer_data,
                                                 GDestroyNotify        observer_data_destroy);
gboolean       ide_build_stage_get_completed    (IdeBuildStage        *self);
void           ide_build_stage_set_completed    (IdeBuildStage        *self,
                                                 gboolean              completed);
gboolean       ide_build_stage_get_transient    (IdeBuildStage        *self);
void           ide_build_stage_set_transient    (IdeBuildStage        *self,
                                                 gboolean              transient);
void           ide_build_stage_execute_async    (IdeBuildStage        *self,
                                                 IdeBuildPipeline     *pipeline,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
gboolean       ide_build_stage_execute_finish   (IdeBuildStage        *self,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_END_DECLS

#endif /* IDE_BUILD_STAGE_H */

