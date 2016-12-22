/* gbp-flatpak-configuration.c
 *
 * Copyright (C) 2016 Matthew Leeds <mleeds@redhat.com>
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

#define G_LOG_DOMAIN "gbp-flatpak-configuration"

#include "ide-debug.h"

#include "gbp-flatpak-configuration.h"
#include "buildsystem/ide-configuration.h"

struct _GbpFlatpakConfiguration
{
  IdeConfiguration parent_instance;

  //TODO
};

G_DEFINE_TYPE (GbpFlatpakConfiguration, gbp_flatpak_configuration, IDE_TYPE_CONFIGURATION)

enum {
  PROP_0,
  N_PROPS
};

static void
gbp_flatpak_configuration_finalize (GObject *object)
{
  G_OBJECT_CLASS (gbp_flatpak_configuration_parent_class)->finalize (object);
}

static void
gbp_flatpak_configuration_class_init (GbpFlatpakConfigurationClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gbp_flatpak_configuration_finalize;
}

static void
gbp_flatpak_configuration_init (GbpFlatpakConfiguration *self)
{
}
