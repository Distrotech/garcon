/* $Id$ */
/* vi:set expandtab sw=2 sts=2: */
/*-
 * Copyright (c) 2008 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if !defined (GARCON_INSIDE_GARCON_H) && !defined (GARCON_COMPILATION)
#error "Only <garcon/garcon.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef __GARCON_CONFIG_H__
#define __GARCON_CONFIG_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define GARCON_MAJOR_VERSION 0
#define GARCON_MINOR_VERSION 1
#define GARCON_MICRO_VERSION 0

#define GARCON_CHECK_VERSION(major,minor,micro) \
  (GARCON_MAJOR_VERSION > (major) \
   || (GARCON_MAJOR_VERSION == (major) \
       && GARCON_MINOR_VERSION > (minor)) \
   || (GARCON_MAJOR_VERSION == (major) \
       && GARCON_MINOR_VERSION == (minor) \
       && GARCON_MICRO_VERSION >= (micro)))

extern const guint garcon_major_version;
extern const guint garcon_minor_version;
extern const guint garcon_micro_version;

const gchar *garcon_check_version      (guint        required_major,
                                        guint        required_minor,
                                        guint        required_micro);

gchar      **garcon_config_build_paths (const gchar *filename) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar       *garcon_config_lookup      (const gchar *filename) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif /* !__GARCON_CONFIG_H__ */