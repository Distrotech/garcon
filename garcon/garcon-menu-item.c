/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2010 Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2009-2010 Nick Schermer <nick@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <garcon/garcon-environment.h>
#include <garcon/garcon-menu-element.h>
#include <garcon/garcon-menu-item.h>
#include <garcon/garcon-private.h>



#define GARCON_MENU_ITEM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GARCON_TYPE_MENU_ITEM, GarconMenuItemPrivate))



#define GET_LOCALE_KEY(type, key) (g_key_file_get_locale_##type (rc, G_KEY_FILE_DESKTOP_GROUP, key, NULL, NULL))
#define GET_KEY(type, key)        (g_key_file_get_##type (rc, G_KEY_FILE_DESKTOP_GROUP, key, NULL))
#define GET_STRING_LIST(key)      (g_key_file_get_string_list (rc, G_KEY_FILE_DESKTOP_GROUP, key, NULL, NULL))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_DESKTOP_ID,
  PROP_REQUIRES_TERMINAL,
  PROP_NO_DISPLAY,
  PROP_STARTUP_NOTIFICATION,
  PROP_NAME,
  PROP_GENERIC_NAME,
  PROP_COMMENT,
  PROP_ICON_NAME,
  PROP_COMMAND,
  PROP_TRY_EXEC,
  PROP_HIDDEN,
  PROP_PATH,
};

/* Signal identifiers */
enum
{
  CHANGED,
  LAST_SIGNAL,
};



static void         garcon_menu_item_element_init                    (GarconMenuElementIface *iface);
static void         garcon_menu_item_finalize                        (GObject                *object);
static void         garcon_menu_item_get_property                    (GObject                *object,
                                                                      guint                   prop_id,
                                                                      GValue                 *value,
                                                                      GParamSpec             *pspec);
static void         garcon_menu_item_set_property                    (GObject                *object,
                                                                      guint                   prop_id,
                                                                      const GValue           *value,
                                                                      GParamSpec             *pspec);
static const gchar *garcon_menu_item_get_element_name                (GarconMenuElement      *element);
static const gchar *garcon_menu_item_get_element_comment             (GarconMenuElement      *element);
static const gchar *garcon_menu_item_get_element_icon_name           (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_visible             (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_show_in_environment (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_no_display          (GarconMenuElement      *element);
static gboolean     garcon_menu_item_get_element_equal               (GarconMenuElement      *element,
                                                                      GarconMenuElement      *other);



static guint item_signals[LAST_SIGNAL];



struct _GarconMenuItemPrivate
{
  /* Source file of the menu item */
  GFile    *file;

  /* Desktop file id */
  gchar    *desktop_id;

  /* List of categories */
  GList    *categories;

  /* Whether this application requires a terminal to be started in */
  guint     requires_terminal : 1;

  /* Whether this menu item should be hidden */
  guint     no_display : 1;

  /* Whether this application supports startup notification */
  guint     supports_startup_notification : 1;

  /* Name to be displayed for the menu item */
  gchar    *name;

  /* Generic name of the menu item */
  gchar    *generic_name;

  /* Comment/description of the item */
  gchar    *comment;

  /* Command to be executed when the menu item is clicked */
  gchar    *command;

  /* TryExec value */
  gchar    *try_exec;

  /* Menu item icon name */
  gchar    *icon_name;

  /* Environments in which the menu item should be displayed only */
  gchar   **only_show_in;

  /* Environments in which the menu item should be hidden */
  gchar   **not_show_in;

  /* Working directory */
  gchar    *path;

  /* Hidden value */
  guint     hidden : 1;

  /* Counter keeping the number of menus which use this item. This works
   * like a reference counter and should be increased / decreased by GarconMenu
   * items whenever the item is added to or removed from the menu. */
  guint     num_allocated;
};



G_DEFINE_TYPE_WITH_CODE (GarconMenuItem, garcon_menu_item, G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (GARCON_TYPE_MENU_ELEMENT, garcon_menu_item_element_init))



static void
garcon_menu_item_class_init (GarconMenuItemClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GarconMenuItemPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = garcon_menu_item_finalize;
  gobject_class->get_property = garcon_menu_item_get_property;
  gobject_class->set_property = garcon_menu_item_set_property;

  /**
   * GarconMenu:file:
   *
   * The #GFile from which the %GarconMenuItem was loaded.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        G_TYPE_FILE,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS |
                                                        G_PARAM_CONSTRUCT_ONLY));

  /**
   * GarconMenuItem:desktop-id:
   *
   * The desktop-file id of this application.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DESKTOP_ID,
                                   g_param_spec_string ("desktop-id",
                                                        "Desktop-File Id",
                                                        "Desktop-File Id of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:requires-terminal:
   *
   * Whether this application requires a terinal to be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REQUIRES_TERMINAL,
                                   g_param_spec_boolean ("requires-terminal",
                                                         "Requires a terminal",
                                                         "Whether this application requires a terminal",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         "No Display",
                                                         "Visibility state of the menu item",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:startup-notification:
   *
   * Whether this application supports startup notification.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_STARTUP_NOTIFICATION,
                                   g_param_spec_boolean ("supports-startup-notification",
                                                         "Startup notification",
                                                         "Startup notification support",
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:name:
   *
   * Name of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:generic-name:
   *
   * GenericName of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GENERIC_NAME,
                                   g_param_spec_string ("generic-name",
                                                        "Generic name",
                                                        "Generic name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:comment:
   *
   * Comment/description for the application. To be displayed e.g. in tooltips of
   * GtkMenuItems.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMENT,
                                   g_param_spec_string ("comment",
                                                        "Comment",
                                                        "Comment/description for the application",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:command:
   *
   * Command to be executed when the menu item is clicked.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "Command",
                                                        "Application command",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:try-exec:
   *
   * Path to an executable file on disk used to determine if the program
   * is actually installed. If the path is not an absolute path, the file
   * is looked up in the $PATH environment variable. If the file is not
   * present or if it is not executable, the entry may be ignored (not be
   * used in menus, for example).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TRY_EXEC,
                                   g_param_spec_string ("try-exec",
                                                        "TryExec",
                                                        "Command to check if application is installed",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem:icon-name:
   *
   * Name of the icon to be displayed for this menu item.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "Icon name",
                                                        "Name of the application icon",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

 /**
   * GarconMenuItem:hidden:
   *
   * It means the user deleted (at his level) something that was present
   * (at an upper level, e.g. in the system dirs). It's strictly equivalent
   * to the .desktop file not existing at all, as far as that user is concerned.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_HIDDEN,
                                   g_param_spec_boolean ("hidden",
                                                         "Hidden",
                                                         "Whether the application has been deleted",
                                                          FALSE,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS));

 /**
   * GarconMenuItem:path:
   *
   * Working directory the application should be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        "Path",
                                                        "Working directory path",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GarconMenuItem::changed:
   * @item : a #GarconMenuItem.
   *
   * Emitted when #GarconMenuItem has been reloaded.
   **/
  item_signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (GarconMenuItemClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
garcon_menu_item_element_init (GarconMenuElementIface *iface)
{
  iface->get_name = garcon_menu_item_get_element_name;
  iface->get_comment = garcon_menu_item_get_element_comment;
  iface->get_icon_name = garcon_menu_item_get_element_icon_name;
  iface->get_visible = garcon_menu_item_get_element_visible;
  iface->get_show_in_environment = garcon_menu_item_get_element_show_in_environment;
  iface->get_no_display = garcon_menu_item_get_element_no_display;
  iface->equal = garcon_menu_item_get_element_equal;
}



static void
garcon_menu_item_init (GarconMenuItem *item)
{
  item->priv = GARCON_MENU_ITEM_GET_PRIVATE (item);
}



static void
garcon_menu_item_finalize (GObject *object)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  g_free (item->priv->desktop_id);
  g_free (item->priv->name);
  g_free (item->priv->generic_name);
  g_free (item->priv->comment);
  g_free (item->priv->command);
  g_free (item->priv->try_exec);
  g_free (item->priv->icon_name);
  g_free (item->priv->path);

  g_strfreev (item->priv->only_show_in);
  g_strfreev (item->priv->not_show_in);

  g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
  g_list_free (item->priv->categories);

  if (item->priv->file != NULL)
    g_object_unref (G_OBJECT (item->priv->file));

  (*G_OBJECT_CLASS (garcon_menu_item_parent_class)->finalize) (object);
}



static void
garcon_menu_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, item->priv->file);
      break;

    case PROP_DESKTOP_ID:
      g_value_set_string (value, garcon_menu_item_get_desktop_id (item));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, garcon_menu_item_get_comment (item));
      break;

    case PROP_REQUIRES_TERMINAL:
      g_value_set_boolean (value, garcon_menu_item_requires_terminal (item));
      break;

    case PROP_NO_DISPLAY:
      g_value_set_boolean (value, garcon_menu_item_get_no_display (item));
      break;

    case PROP_STARTUP_NOTIFICATION:
      g_value_set_boolean (value, garcon_menu_item_supports_startup_notification (item));
      break;

    case PROP_NAME:
      g_value_set_string (value, garcon_menu_item_get_name (item));
      break;

    case PROP_GENERIC_NAME:
      g_value_set_string (value, garcon_menu_item_get_generic_name (item));
      break;

    case PROP_COMMAND:
      g_value_set_string (value, garcon_menu_item_get_command (item));
      break;

    case PROP_ICON_NAME:
      g_value_set_string (value, garcon_menu_item_get_icon_name (item));
      break;

    case PROP_TRY_EXEC:
      g_value_set_string (value, garcon_menu_item_get_try_exec (item));
      break;

    case PROP_HIDDEN:
      g_value_set_boolean (value, garcon_menu_item_get_hidden (item));
      break;

    case PROP_PATH:
      g_value_set_string (value, garcon_menu_item_get_path (item));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
garcon_menu_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GarconMenuItem *item = GARCON_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_FILE:
      item->priv->file = g_value_dup_object (value);
      break;

    case PROP_DESKTOP_ID:
      garcon_menu_item_set_desktop_id (item, g_value_get_string (value));
      break;

    case PROP_REQUIRES_TERMINAL:
      garcon_menu_item_set_requires_terminal (item, g_value_get_boolean (value));
      break;

    case PROP_NO_DISPLAY:
      garcon_menu_item_set_no_display (item, g_value_get_boolean (value));
      break;

    case PROP_STARTUP_NOTIFICATION:
      garcon_menu_item_set_supports_startup_notification (item, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      garcon_menu_item_set_name (item, g_value_get_string (value));
      break;

    case PROP_GENERIC_NAME:
      garcon_menu_item_set_generic_name (item, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      garcon_menu_item_set_comment (item, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      garcon_menu_item_set_command (item, g_value_get_string (value));
      break;

    case PROP_TRY_EXEC:
      garcon_menu_item_set_try_exec (item, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      garcon_menu_item_set_icon_name (item, g_value_get_string (value));
      break;

    case PROP_HIDDEN:
      garcon_menu_item_set_hidden (item, g_value_get_boolean (value));
      break;

    case PROP_PATH:
      garcon_menu_item_set_path (item, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
garcon_menu_item_get_element_visible (GarconMenuElement *element)
{
  GarconMenuItem  *item;
  const gchar     *try_exec;
  gchar          **mt;
  gboolean         result = TRUE;
  gchar           *command;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);

  item = GARCON_MENU_ITEM (element);

  if (garcon_menu_item_get_hidden (item)
      || garcon_menu_item_get_no_display (item)
      || !garcon_menu_item_get_show_in_environment (item))
    return FALSE;

  /* Check the TryExec field */
  try_exec = garcon_menu_item_get_try_exec (item);
  if (try_exec != NULL && g_shell_parse_argv (try_exec, NULL, &mt, NULL))
    {
      /* Check if we have an absolute path to an existing file */
      result = g_file_test (mt[0], G_FILE_TEST_EXISTS);

      /* Else, we may have a program in $PATH */
      if (!result)
        {
          command = g_find_program_in_path (mt[0]);
          result = (command != NULL);
          g_free (command);
        }

      /* Cleanup */
      g_strfreev (mt);
    }

  return result;
}



static gboolean
garcon_menu_item_get_element_show_in_environment (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  return garcon_menu_item_get_show_in_environment (GARCON_MENU_ITEM (element));
}



static gboolean
garcon_menu_item_get_element_no_display (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  return garcon_menu_item_get_no_display (GARCON_MENU_ITEM (element));
}



static gboolean
garcon_menu_item_get_element_equal (GarconMenuElement *element,
                                    GarconMenuElement *other)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), FALSE);
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (other), FALSE);

  return g_file_equal (GARCON_MENU_ITEM (element)->priv->file, 
                       GARCON_MENU_ITEM (other)->priv->file);
}



static const gchar*
garcon_menu_item_get_element_name (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->name;
}



static const gchar*
garcon_menu_item_get_element_comment (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->comment;
}



static const gchar*
garcon_menu_item_get_element_icon_name (GarconMenuElement *element)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (element), NULL);
  return GARCON_MENU_ITEM (element)->priv->icon_name;
}



GarconMenuItem *
garcon_menu_item_new (GFile *file)
{
  GarconMenuItem *item = NULL;
  GKeyFile       *rc;
  gchar          *contents;
  gsize           length = 0;
  gboolean        succeed;
  GList          *categories = NULL;
  gboolean        terminal;
  gboolean        no_display;
  gboolean        startup_notify;
  gboolean        hidden;
  gchar          *path;
  gchar          *name;
  gchar          *generic_name;
  gchar          *comment;
  gchar          *exec;
  gchar          *try_exec;
  gchar          *icon;
  gchar         **mt;
  gchar         **str_list;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  /* Load the contents of the file */
  if (!g_file_load_contents (file, NULL, &contents, &length, NULL, NULL) || length == 0)
    return NULL;

  /* Open the keyfile */
  rc = g_key_file_new ();
  succeed = g_key_file_load_from_data (rc, contents, length, G_KEY_FILE_NONE, NULL);
  g_free (contents);
  if (G_UNLIKELY (!succeed))
    {
      g_key_file_free (rc);
      return NULL;
    }

  /* Parse name and exec command */
  name = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_NAME);
  exec = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_EXEC);

  /* Validate Name and Exec fields */
  if (G_LIKELY (exec != NULL && name != NULL && g_utf8_validate (name, -1, NULL)))
    {
      /* Determine other application properties */
      generic_name = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME);
      comment = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_COMMENT);
      try_exec = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC);
      icon = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_ICON);
      path = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_PATH);
      terminal = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_TERMINAL);
      no_display = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY);
      startup_notify = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY)
                       || GET_KEY (boolean, "X-KDE-StartupNotify");
      hidden = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_HIDDEN);

      /* Allocate a new menu item instance */
      item = g_object_new (GARCON_TYPE_MENU_ITEM,
                           "file", file,
                           "command", exec,
                           "try-exec", try_exec,
                           "name", name,
                           "generic-name", generic_name,
                           "comment", comment,
                           "icon-name", icon,
                           "requires-terminal", terminal,
                           "no-display", no_display,
                           "supports-startup-notification", startup_notify,
                           "path", path,
                           "hidden", hidden,
                           NULL);

      /* Free strings */
      g_free (generic_name);
      g_free (comment);
      g_free (try_exec);
      g_free (icon);
      g_free (path);

      /* Determine the categories this application should be shown in */
      str_list = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_CATEGORIES);
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              if (**mt != '\0')
                categories = g_list_prepend (categories, g_strdup (*mt));
            }

          /* Free list */
          g_strfreev (str_list);

          /* Assign categories list to the menu item */
          garcon_menu_item_set_categories (item, categories);
        }

      /* Set the rest of the private data directly */
      item->priv->only_show_in = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN);
      item->priv->not_show_in = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN);
    }

  /* Free strings */
  g_free (name);
  g_free (exec);

  /* Cleanup */
  g_key_file_free (rc);

  return item;
}



GarconMenuItem *
garcon_menu_item_new_for_path (const gchar *filename)
{
  GFile          *file;
  GarconMenuItem *item;

  g_return_val_if_fail (filename != NULL, NULL);

  file = g_file_new_for_path (filename);
  item = garcon_menu_item_new (file);
  g_object_unref (G_OBJECT (file));

  return item;
}



GarconMenuItem *
garcon_menu_item_new_for_uri (const gchar *uri)
{
  GFile          *file;
  GarconMenuItem *item;

  g_return_val_if_fail (uri != NULL, NULL);

  file = g_file_new_for_uri (uri);
  item = garcon_menu_item_new (file);
  g_object_unref (G_OBJECT (file));

  return item;
}



gboolean
garcon_menu_item_reload (GarconMenuItem  *item,
                         GError         **error)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return garcon_menu_item_reload_from_file (item, item->priv->file, error);
}



gboolean
garcon_menu_item_reload_from_file (GarconMenuItem  *item,
                                   GFile           *file,
                                   GError         **error)
{
  GKeyFile *rc;
  gboolean  boolean;
  gboolean  succeed;
  GList    *categories = NULL;
  gchar   **mt;
  gchar   **str_list;
  gchar    *contents;
  gchar    *string;
  gchar    *name;
  gchar    *exec;
  gsize     length = 0;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* Load the contents of the file */
  if (!g_file_load_contents (file, NULL, &contents, &length, NULL, error))
    return FALSE;

  /* Leave when the file is empty */
  if (G_UNLIKELY (length == 0))
    {
      g_set_error_literal (error, 0, 0, "The desktop file is empty.");
      return FALSE;
    }

  /* Open the keyfile */
  rc = g_key_file_new ();
  succeed = g_key_file_load_from_data (rc, contents, length, G_KEY_FILE_NONE, error);
  g_free (contents);
  if (G_UNLIKELY (!succeed))
    return FALSE;

  /* Check if there is a name and exec key */
  name = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_NAME);
  exec = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_EXEC);
  if (G_UNLIKELY (name == NULL || exec == NULL))
    {
      g_set_error_literal (error, 0, 0, "Either the name or exec key was not defined.");

      g_free (name);
      g_free (exec);
      g_key_file_free (rc);

      return FALSE;
    }

  /* Queue property notifications */
  g_object_freeze_notify (G_OBJECT (item));

  /* Set the new file if needed */
  if (!g_file_equal (file, item->priv->file))
    {
      if (G_LIKELY (item->priv->file != NULL))
        g_object_unref (G_OBJECT (item->priv->file));
      item->priv->file = g_object_ref (G_OBJECT (file));

      g_object_notify (G_OBJECT (item), "file");
    }

  /* Update properties */
  garcon_menu_item_set_name (item, name);
  g_free (name);

  garcon_menu_item_set_command (item, exec);
  g_free (exec);

  string = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_GENERIC_NAME);
  garcon_menu_item_set_generic_name (item, string);
  g_free (string);

  string = GET_LOCALE_KEY (string, G_KEY_FILE_DESKTOP_KEY_COMMENT);
  garcon_menu_item_set_comment (item, string);
  g_free (string);

  string = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_TRY_EXEC);
  garcon_menu_item_set_try_exec (item, string);
  g_free (string);

  string = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_ICON);
  garcon_menu_item_set_icon_name (item, string);
  g_free (string);

  string = GET_KEY (string, G_KEY_FILE_DESKTOP_KEY_PATH);
  garcon_menu_item_set_path (item, string);
  g_free (string);

  boolean = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_TERMINAL);
  garcon_menu_item_set_requires_terminal (item, boolean);

  boolean = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY);
  garcon_menu_item_set_no_display (item, boolean);

  boolean = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY)
            || GET_KEY (boolean, "X-KDE-StartupNotify");
  garcon_menu_item_set_supports_startup_notification (item, boolean);

  boolean = GET_KEY (boolean, G_KEY_FILE_DESKTOP_KEY_HIDDEN);
  garcon_menu_item_set_hidden (item, boolean);

  /* Determine the categories this application should be shown in */
  str_list = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_CATEGORIES);
  if (G_LIKELY (str_list != NULL))
    {
      for (mt = str_list; *mt != NULL; ++mt)
        {
          if (**mt != '\0')
            categories = g_list_prepend (categories, g_strdup (*mt));
        }

      /* Free list */
      g_strfreev (str_list);

      /* Assign categories list to the menu item */
      garcon_menu_item_set_categories (item, categories);
    }
  else
    {
      /* Assign empty categories list to the menu item */
      garcon_menu_item_set_categories (item, NULL);
    }

  /* Set the rest of the private data directly */
  item->priv->only_show_in = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN);
  item->priv->not_show_in = GET_STRING_LIST (G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN);

  /* Flush property notifications */
  g_object_thaw_notify (G_OBJECT (item));

  /* Emit signal to everybody knows we reloaded the file */
  g_signal_emit (G_OBJECT (item), item_signals[CHANGED], 0);

  g_key_file_free (rc);

  return TRUE;
}



/**
 * garcon_menu_item_get_file:
 *
 * Get the file for @item.
 *
 * Return value: a #GFile. The returned object
 * should be unreffed with g_object_unref() when no longer needed.
 */
GFile *
garcon_menu_item_get_file (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return g_object_ref (item->priv->file);
}


gchar *
garcon_menu_item_get_uri (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return g_file_get_uri (item->priv->file);
}



const gchar *
garcon_menu_item_get_desktop_id (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->desktop_id;
}



void
garcon_menu_item_set_desktop_id (GarconMenuItem *item,
                                 const gchar    *desktop_id)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (desktop_id != NULL);

  /* Abort if old and new desktop_id are equal */
  if (g_strcmp0 (item->priv->desktop_id, desktop_id) == 0)
    return;

  /* Assign the new desktop_id */
  g_free (item->priv->desktop_id);
  item->priv->desktop_id = g_strdup (desktop_id);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "desktop-id");
}



GList*
garcon_menu_item_get_categories (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->categories;
}



void
garcon_menu_item_set_categories (GarconMenuItem *item,
                                 GList          *categories)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if lists are equal */
  if (G_UNLIKELY (item->priv->categories == categories))
    return;

  /* Free old list */
  g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
  g_list_free (item->priv->categories);

  /* Assign new list */
  item->priv->categories = categories;
}



const gchar*
garcon_menu_item_get_command (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->command;
}



void
garcon_menu_item_set_command (GarconMenuItem *item,
                              const gchar    *command)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (command != NULL);

  /* Abort if old and new command are equal */
  if (g_strcmp0 (item->priv->command, command) == 0)
    return;

  /* Assign new command */
  g_free (item->priv->command);
  item->priv->command = g_strdup (command);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "command");
}



const gchar*
garcon_menu_item_get_try_exec (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->try_exec;
}



void
garcon_menu_item_set_try_exec (GarconMenuItem *item,
                               const gchar    *try_exec)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new try_exec are equal */
  if (g_strcmp0 (item->priv->try_exec, try_exec) == 0)
    return;

  /* Assign new try_exec */
  g_free (item->priv->try_exec);
  item->priv->try_exec = g_strdup (try_exec);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "try-exec");
}



const gchar*
garcon_menu_item_get_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->name;
}



void
garcon_menu_item_set_name (GarconMenuItem *item,
                           const gchar    *name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (g_utf8_validate (name, -1, NULL));

  /* Abort if old and new name are equal */
  if (g_strcmp0 (item->priv->name, name) == 0)
    return;

  /* Assign new name */
  g_free (item->priv->name);
  item->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "name");
}



const gchar*
garcon_menu_item_get_generic_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->generic_name;
}



void
garcon_menu_item_set_generic_name (GarconMenuItem *item,
                                   const gchar    *generic_name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (generic_name == NULL || g_utf8_validate (generic_name, -1, NULL));

  /* Abort if old and new generic name are equal */
  if (g_strcmp0 (item->priv->generic_name, generic_name) == 0)
    return;

  /* Assign new generic_name */
  g_free (item->priv->generic_name);
  item->priv->generic_name = g_strdup (generic_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "generic-name");
}



const gchar*
garcon_menu_item_get_comment (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->comment;
}



void
garcon_menu_item_set_comment (GarconMenuItem *item,
                              const gchar    *comment)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  g_return_if_fail (comment == NULL || g_utf8_validate (comment, -1, NULL));

  /* Abort if old and new comment are equal */
  if (g_strcmp0 (item->priv->comment, comment) == 0)
    return;

  /* Assign new comment */
  g_free (item->priv->comment);
  item->priv->comment = g_strdup (comment);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "comment");
}



const gchar*
garcon_menu_item_get_icon_name (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->icon_name;
}



void
garcon_menu_item_set_icon_name (GarconMenuItem *item,
                                const gchar    *icon_name)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new icon name are equal */
  if (g_strcmp0 (item->priv->icon_name, icon_name) == 0)
    return;

  /* Assign new icon name */
  g_free (item->priv->icon_name);
  item->priv->icon_name = g_strdup (icon_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "icon-name");
}



const gchar*
garcon_menu_item_get_path (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), NULL);
  return item->priv->path;
}



void
garcon_menu_item_set_path (GarconMenuItem *item,
                           const gchar    *path)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new path are equal */
  if (g_strcmp0 (item->priv->path, path) == 0)
    return;

  /* Assign new path */
  g_free (item->priv->path);
  item->priv->path = g_strdup (path);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "path");
}



gboolean
garcon_menu_item_get_hidden (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), TRUE);
  return item->priv->hidden;
}



void
garcon_menu_item_set_hidden (GarconMenuItem *item,
                             gboolean        hidden)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->hidden == hidden)
    return;

  /* Assign new value */
  item->priv->hidden = !!hidden;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "hidden");
}



gboolean
garcon_menu_item_requires_terminal (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->requires_terminal;
}



void
garcon_menu_item_set_requires_terminal (GarconMenuItem *item,
                                        gboolean        requires_terminal)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->requires_terminal == requires_terminal)
    return;

  /* Assign new value */
  item->priv->requires_terminal = !!requires_terminal;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "requires-terminal");
}



gboolean
garcon_menu_item_get_no_display (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->no_display;
}



void
garcon_menu_item_set_no_display (GarconMenuItem *item,
                                 gboolean        no_display)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->no_display == no_display)
    return;

  /* Assign new value */
  item->priv->no_display = !!no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "no-display");
}



gboolean
garcon_menu_item_supports_startup_notification (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->supports_startup_notification;
}



void
garcon_menu_item_set_supports_startup_notification (GarconMenuItem *item,
                                                    gboolean        supports_startup_notification)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Abort if old and new value are equal */
  if (item->priv->supports_startup_notification == supports_startup_notification)
    return;

  /* Assign new value */
  item->priv->supports_startup_notification = !!supports_startup_notification;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "supports-startup-notification");
}



gboolean
garcon_menu_item_has_category (GarconMenuItem *item,
                               const gchar    *category)
{
  GList   *iter;
  gboolean found = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (category != NULL, FALSE);

  for (iter = item->priv->categories; !found && iter != NULL; iter = g_list_next (iter))
    if (g_strcmp0 (iter->data, category) == 0)
      found = TRUE;

  return found;
}



gboolean
garcon_menu_item_get_show_in_environment (GarconMenuItem *item)
{
  const gchar *env;
  guint        i;
  gboolean     show = TRUE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = garcon_get_environment ();

  /* If no environment has been set, the menu is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* According to the spec there is either a OnlyShowIn or a NotShowIn list */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Check if your environemnt is in OnlyShowIn list */
      for (i = 0, show = FALSE; !show && item->priv->only_show_in[i] != NULL; i++)
        if (g_strcmp0 (item->priv->only_show_in[i], env) == 0)
          show = TRUE;
    }
  else if (G_UNLIKELY (item->priv->not_show_in != NULL))
    {
      /* Check if your environemnt is in NotShowIn list */
      for (i = 0, show = TRUE; show && item->priv->not_show_in[i] != NULL; i++)
        if (g_strcmp0 (item->priv->not_show_in[i], env) == 0)
          show = FALSE;
    }

  return show;
}



gboolean
garcon_menu_item_only_show_in_environment (GarconMenuItem *item)
{
  const gchar *env;
  guint        i;
  gboolean     show = FALSE;

  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = garcon_get_environment ();

  /* If no environment has been set, the contents of OnlyShowIn don't matter */
  if (G_LIKELY (env == NULL))
    return FALSE;

  /* Check if we have an OnlyShowIn list */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Check if your environemnt is in OnlyShowIn list */
      for (i = 0, show = FALSE; !show && item->priv->only_show_in[i] != NULL; i++)
        if (g_strcmp0 (item->priv->only_show_in[i], env) == 0)
          show = TRUE;
    }

  return show;
}



void
garcon_menu_item_ref (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Increment the allocation counter */
  garcon_menu_item_increment_allocated (item);

  /* Grab a reference on the object */
  g_object_ref (G_OBJECT (item));
}



void
garcon_menu_item_unref (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  /* Decrement the allocation counter */
  garcon_menu_item_decrement_allocated (item);

  /* Decrement the reference counter */
  g_object_unref (G_OBJECT (item));
}



gint
garcon_menu_item_get_allocated (GarconMenuItem *item)
{
  g_return_val_if_fail (GARCON_IS_MENU_ITEM (item), FALSE);
  return item->priv->num_allocated;
}



void
garcon_menu_item_increment_allocated (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));
  item->priv->num_allocated++;
}



void
garcon_menu_item_decrement_allocated (GarconMenuItem *item)
{
  g_return_if_fail (GARCON_IS_MENU_ITEM (item));

  if (item->priv->num_allocated > 0)
    item->priv->num_allocated--;
}
