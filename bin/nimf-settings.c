/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2; tab-width: 2 -*- */
/*
 * nimf-settings.c
 * This file is part of Nimf.
 *
 * Copyright (C) 2016-2018 Hodong Kim <cogniti@gmail.com>
 *
 * Nimf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Nimf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program;  If not, see <http://www.gnu.org/licenses/>.
 */
#include <glib-object.h> // GType
#include <glib.h> // g_strjoin
#include <glib/gprintf.h> // g_strjoin
#include <gdk-pixbuf/gdk-pixbuf.h> // GdkPixbuf

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <hangul.h>
#include "config.h"
#include "nimf.h"

#define NIMF_TYPE_SETTINGS             (nimf_settings_get_type ())
#define NIMF_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), NIMF_TYPE_SETTINGS, NimfSettings))
#define NIMF_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), NIMF_TYPE_SETTINGS, NimfSettingsClass))
#define NIMF_IS_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NIMF_TYPE_SETTINGS))
#define NIMF_IS_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), NIMF_TYPE_SETTINGS))
#define NIMF_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), NIMF_TYPE_SETTINGS, NimfSettingsClass))

typedef struct _NimfSettings      NimfSettings;
typedef struct _NimfSettingsClass NimfSettingsClass;

struct _NimfSettings
{
  GObject parent_instance;

  GApplication          *app;
  GPtrArray             *pages;
  GSettingsSchemaSource *schema_source; /* do not free */
};

struct _NimfSettingsClass
{
  GObjectClass parent_class;
};

GType nimf_settings_get_type (void) G_GNUC_CONST;

enum {
  SCHEMA_COLUMN = 0
};

typedef struct _NimfSettingsPage
{
  GSettings *gsettings;
  GtkWidget *box;
  gchar     *title;
  GPtrArray *page_keys;
} NimfSettingsPage;

typedef struct _NimfSettingsPageKey {
  GSettings *gsettings;
  gchar     *key;
  GtkWidget *label;
  GtkWidget *treeview;
} NimfSettingsPageKey;

typedef struct _KeyboardInfo {
  gchar *id;
  gchar *name;
  NimfSettingsPageKey *page_key;
  gchar **initial_keys;
  guint initial_keys_length;
  GtkWidget *dialog;
} KeyboardInfo;

static GtkWidget *nimf_settings_window = NULL;

G_DEFINE_TYPE (NimfSettings, nimf_settings, G_TYPE_OBJECT);


enum
{
  INDEX,
  ID,
  NAME
};

#define VIEW_WIDTH 320
#define VIEW_HEIGHT 240

#define N_ROWS 10
#define BIG_N_ROWS N_ROWS * 5

void 
listbox_activated_row (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data);

void
layout_view_clicked(GtkDialog *dialog, KeyboardInfo *keyboard_info)
{
  gchar *extension_image = ".png";
  gchar *extension_text = ".txt";
  gchar *layout = NULL;

  gchar *filename_layout = NULL;
  gchar *filename_description = NULL;

  if (g_str_has_prefix(keyboard_info->id, "2"))
  {
    layout = "2set_";
  }
  else if (g_str_has_prefix(keyboard_info->id, "3") |
            g_str_has_prefix(keyboard_info->id, "ahn"))
  {
    layout = "3set_";
  }
  else if (g_str_has_prefix(keyboard_info->id, "ro"))
  {
    layout = "kb_us_";
  }
  else
  {
    return;
  }


  gchar *help_layout = NULL;
  gchar *help_txt = NULL;

  filename_layout = g_strjoin(NULL, layout, keyboard_info->id, extension_image, NULL);
  filename_description = g_strjoin(NULL, layout, keyboard_info->id, extension_text, NULL);
  help_layout = g_strjoin("/", LIBHANGUL_DATA_DIR, "keyboards_info", filename_layout, NULL);
  help_txt = g_strjoin("/", LIBHANGUL_DATA_DIR, "keyboards_info", filename_description, NULL);

  GtkWidget *dialog_layout;
  GtkWidget *content_area;
  GtkDialogFlags flags;

  #if GTK_CHECK_VERSION (3, 12, 0)
  flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;
#else
  flags = GTK_DIALOG_MODAL;
#endif

  dialog_layout = gtk_dialog_new_with_buttons (
                        _(keyboard_info->name),
                        GTK_WINDOW (nimf_settings_window),
                        flags,
                        _("_OK"), GTK_RESPONSE_CANCEL,
                        NULL);
  g_signal_connect (dialog_layout, "destroy",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_window_set_icon_name (GTK_WINDOW (dialog_layout), "nimf-logo");
  gtk_widget_set_size_request (GTK_WIDGET (dialog_layout), -1, -1);
  gtk_window_set_resizable (GTK_WINDOW (dialog_layout), FALSE);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog_layout));

  GtkWidget *label;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  if (g_file_test (help_layout, G_FILE_TEST_IS_REGULAR))
  {
    pixbuf = gdk_pixbuf_new_from_file_at_size (help_layout, 520, 420, &error);
    label = gtk_image_new_from_pixbuf (pixbuf);
    gtk_box_pack_start (GTK_BOX (content_area), label, TRUE, TRUE, 0);
  }
  else
  {
    label = gtk_label_new(help_layout);
    gtk_box_pack_start (GTK_BOX (content_area), label, TRUE, TRUE, 0);
  }

  GtkWidget *scrolled_window;
  GtkWidget *text_view;
  GtkTextTagTable *tab_table;
  GtkTextBuffer *buffer;
  GtkTextIter iter;
  GString *lines = g_string_new (NULL);

  if (g_file_test (help_txt, G_FILE_TEST_IS_REGULAR))
  {
    char line[1000];

    tab_table = gtk_text_tag_table_new ();
    buffer = gtk_text_buffer_new (tab_table);
    FILE *file;

    if ((file = fopen(help_txt, "r")) != NULL)
    {
      while (!feof (file))
      {	
        if (fgets (line, sizeof(line), file) != NULL)
        {
          g_string_append (lines, (gchar *)line);
        }
        
      }
    }
    fclose(file);

    gtk_text_buffer_set_text (buffer, lines->str, lines->len);
    gtk_text_buffer_get_iter_at_line (buffer, &iter, 0);
    gtk_text_buffer_place_cursor (buffer, &iter);

    text_view = gtk_text_view_new_with_buffer (buffer);
    gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_CHAR);

    scrolled_window = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (
                  GTK_SCROLLED_WINDOW (scrolled_window),
                  GTK_POLICY_NEVER,
                  GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(
                  GTK_SCROLLED_WINDOW (scrolled_window), 
                  GTK_SHADOW_IN);
    gtk_scrolled_window_set_min_content_height (
                  GTK_SCROLLED_WINDOW (scrolled_window), 
                  VIEW_HEIGHT * 1.5);
    gtk_container_add (
                  GTK_CONTAINER (scrolled_window), 
                  GTK_WIDGET(text_view));
    gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 5);

    gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 0);
  }
  else
  {
    label = gtk_label_new(help_txt);
    gtk_box_pack_end (GTK_BOX (content_area), label, TRUE, TRUE, 0);
  }
  
  gtk_widget_show_all (dialog_layout);
  gtk_dialog_run (GTK_DIALOG (dialog_layout));

  gtk_widget_destroy (GTK_WIDGET (dialog_layout));

  g_free (filename_layout);
  g_free (filename_description);

  g_free (help_layout);
  g_free (help_txt);

  g_string_free (lines, TRUE);

}

void
keyboard_info_free (KeyboardInfo *keyboard_info)
{
  keyboard_info->page_key = NULL;
  keyboard_info->dialog = NULL;
  g_strfreev (keyboard_info->initial_keys);
  g_free (keyboard_info->id);
  g_free (keyboard_info->name);
  g_slice_free (KeyboardInfo, keyboard_info);
}

void 
on_dialog_response (GtkDialog *dialog,
                    gint       response_id,
                    gpointer   user_data)
{
  KeyboardInfo *keyboard_info = (KeyboardInfo *) user_data;

  switch (response_id)
  {
    case GTK_RESPONSE_HELP:
      {
        layout_view_clicked(dialog, keyboard_info);
      }
      break;
    case GTK_RESPONSE_OK:
      {
        if (keyboard_info->id != NULL)
        {
          g_settings_set_string (
              keyboard_info->page_key->gsettings, 
              keyboard_info->page_key->key, 
              keyboard_info->id);
        }
        keyboard_info_free (keyboard_info);
        gtk_widget_destroy (GTK_WIDGET (dialog));
      }
      break;
    case GTK_RESPONSE_CANCEL:
    default:
      {
        keyboard_info_free (keyboard_info);
        gtk_widget_destroy (GTK_WIDGET (dialog));
      }
      break;
  }
  
}

void 
listbox_selected_dialog (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  gtk_dialog_set_response_sensitive (GTK_DIALOG (user_data),
                                    GTK_RESPONSE_OK, TRUE);
}

void 
listbox_selected_row (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  if (strcmp (gtk_widget_get_name (GTK_WIDGET (row)), "more") == 0)
  {
    return;
  }

  KeyboardInfo *keyboard_info = (KeyboardInfo *) user_data;

  GtkWidget *child;
  const gchar *id = NULL;
  const gchar *name = NULL;

  child = gtk_bin_get_child (GTK_BIN (row));
  id = gtk_widget_get_tooltip_markup (GTK_WIDGET (child));
  name = gtk_label_get_text(GTK_LABEL (child));

  keyboard_info->id = g_strdup(id);
  keyboard_info->name = g_strdup(name);
}

GtkWidget *
more_row_new (void)
{
  GtkWidget *row = gtk_list_box_row_new ();
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);

  gtk_container_add (GTK_CONTAINER (row), GTK_WIDGET (box));
  gtk_widget_set_tooltip_markup ( GTK_WIDGET (row), _("More..."));
  gtk_widget_set_name (GTK_WIDGET (row), "more");

  GtkWidget *arrow = gtk_image_new_from_icon_name ("view-more-symbolic", GTK_ICON_SIZE_MENU);
  gtk_widget_set_margin_start (GTK_WIDGET (arrow), 20);
  gtk_widget_set_margin_end (GTK_WIDGET (arrow), 20);
  gtk_widget_set_margin_top (GTK_WIDGET (arrow), 6);
  gtk_widget_set_margin_bottom (GTK_WIDGET (arrow), 6);
  gtk_widget_set_halign (GTK_WIDGET (arrow), GTK_ALIGN_CENTER);
  gtk_widget_set_valign (GTK_WIDGET (arrow), GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (arrow), TRUE, TRUE, 0);

  return row;
}

void
add_list_item (GtkListBox *listbox, const gchar *text, const gchar *tooltip)
{
    GtkWidget     *item;
    gchar *name = g_strdup(text);
    gchar *id = g_strdup(tooltip);

    item = gtk_label_new(name);
    gtk_widget_set_tooltip_markup(item, id);
    gtk_container_add (GTK_CONTAINER (listbox), item);

    gtk_widget_show (item);

    g_free(id);
    g_free(name);
}

void
build_content_area (KeyboardInfo *keyboard_info, gboolean showing_extra)
{
  gchar       *id1;

  id1 = g_settings_get_string (
          keyboard_info->page_key->gsettings, 
          keyboard_info->page_key->key);


  GtkWidget *dialog;
  GtkDialogFlags flags;
  GtkWidget *content_area;

  GtkWidget *scrolled_window;
  GtkListBox *list_box;
  GtkListBoxRow *list_box_row;


#if GTK_CHECK_VERSION (3, 12, 0)
  flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;
#else
  flags = GTK_DIALOG_MODAL;
#endif

  dialog = gtk_dialog_new_with_buttons (
                      _("Select a Keyboard Layout"),
                      GTK_WINDOW (nimf_settings_window),
                      flags,
                      _("_Layout"), GTK_RESPONSE_HELP,
                      _("_OK"), GTK_RESPONSE_OK,
                      _("_Cancel"), GTK_RESPONSE_CANCEL,
                      NULL);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "nimf-logo");
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 420, -1);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK, FALSE);

  keyboard_info->dialog = GTK_WIDGET (dialog);

  list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (list_box), GTK_SELECTION_BROWSE);

  unsigned layoutCount = hangul_keyboard_list_get_count();
  unsigned index;
  for (index = 0; index < layoutCount; index++)
  {
    const gchar *id2 = hangul_keyboard_list_get_keyboard_id(index);
    const gchar *name = hangul_keyboard_list_get_keyboard_name(index);


    if (showing_extra == FALSE)
    {
      int i;
      for(i = 0; i < keyboard_info->initial_keys_length; i++)
      {
        gchar *key = (gchar *)keyboard_info->initial_keys[i];
        if(strcmp(key, id2) == 0)
        {
          add_list_item (list_box, name, id2);
          list_box_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box), index);

          if (g_strcmp0 (id1, id2) == 0) 
          {
            gtk_list_box_select_row(GTK_LIST_BOX (list_box), list_box_row);
            keyboard_info->id = g_strdup(id2);
            keyboard_info->name = g_strdup(name);
          }
        }
      }
    }
    else 
    {
      add_list_item (list_box, name, id2);
      list_box_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(list_box), index);

      if (g_strcmp0 (id1, id2) == 0) 
      {
        gtk_list_box_select_row(GTK_LIST_BOX (list_box), list_box_row);
        keyboard_info->id = g_strdup(id2);
        keyboard_info->name = g_strdup(name);
      }
    }

  }

  if (showing_extra == FALSE)
  {
    gtk_list_box_insert (GTK_LIST_BOX (list_box), GTK_WIDGET (more_row_new()), -1);
  }
  

  g_signal_connect (GTK_LIST_BOX (list_box), "row-selected",
          G_CALLBACK (listbox_selected_dialog), dialog);
  g_signal_connect (GTK_LIST_BOX (list_box), "row-selected",
          G_CALLBACK (listbox_selected_row), keyboard_info);
  g_signal_connect (GTK_LIST_BOX (list_box), "row-activated",
          G_CALLBACK (listbox_activated_row), keyboard_info);

  g_signal_connect (GTK_DIALOG (dialog), "response", 
                    G_CALLBACK (on_dialog_response), keyboard_info);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                                  GTK_POLICY_NEVER,
                                  GTK_POLICY_ALWAYS);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);

  gtk_container_add (GTK_CONTAINER (scrolled_window), GTK_WIDGET(list_box));
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 10);

	gtk_scrolled_window_set_min_content_width (GTK_SCROLLED_WINDOW (scrolled_window), VIEW_WIDTH);
	gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (scrolled_window), VIEW_HEIGHT);


  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 0);

  gtk_widget_show_all (dialog);

  g_free (id1);
}

void 
listbox_activated_row (GtkListBox    *box,
               GtkListBoxRow *row,
               gpointer       user_data)
{
  GtkWidget *dialog;

  if (strcmp (gtk_widget_get_name (GTK_WIDGET (row)), "more") != 0)
  {
    return;
  }

  KeyboardInfo *keyboard_info = (KeyboardInfo *) user_data;

  dialog = keyboard_info->dialog;

  if (dialog != NULL && GTK_IS_WIDGET (dialog))
  {
    keyboard_info->dialog = NULL;
    gtk_widget_destroy (GTK_WIDGET (dialog));
    build_content_area (keyboard_info, TRUE);
  }
}

static void
on_gsettings_changed (GSettings *settings,
                      gchar     *key,
                      GtkWidget *widget)
{
  if (GTK_IS_SWITCH (widget))
  {
    gboolean active1 = g_settings_get_boolean (settings, key);
    gboolean active2 = gtk_switch_get_active (GTK_SWITCH (widget));

    if (active1 != active2)
      gtk_switch_set_active (GTK_SWITCH (widget), active1);
  }
  else if (GTK_IS_COMBO_BOX (widget))
  {
    gchar    *id;
    gboolean  retval;

    id = g_settings_get_string (settings, key);
    retval = gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), id);

    if (retval == FALSE && g_strcmp0 (key, "default-engine") == 0)
      g_settings_set_string (settings, key, "nimf-system-keyboard");

    g_free (id);
  }
  else if (GTK_IS_TREE_VIEW (widget))
  {
    GtkTreeModel  *model;
    gchar        **vals;
    GtkTreeIter    iter;
    gint           i;

    vals = g_settings_get_strv (settings, key);
    model = gtk_tree_view_get_model (GTK_TREE_VIEW (widget));
    gtk_list_store_clear (GTK_LIST_STORE (model));

    for (i = 0; vals[i] != NULL; i++)
    {
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set    (GTK_LIST_STORE (model), &iter, 0, vals[i], -1);
    }

    g_strfreev (vals);
  }
  else if (GTK_IS_ENTRY (widget))
  {
    gchar    *id;

    id = g_settings_get_string (settings, key);

    unsigned layoutCount = hangul_keyboard_list_get_count ();
    unsigned index;
    for (index = 0; index < layoutCount; index++)
    {
      const gchar *id2 = hangul_keyboard_list_get_keyboard_id (index);
      const gchar *val = hangul_keyboard_list_get_keyboard_name (index);

      if (g_strcmp0 (id, id2) == 0) 
      {
        gtk_entry_set_text (GTK_ENTRY (widget), val);
      }

    }

    g_free (id);
  }
}

static gint
on_comparison (const char *a,
               const char *b)
{
  return g_strcmp0 (a, b);
}

static void
on_combo_box_changed (GtkComboBox        *widget,
                     NimfSettingsPageKey *page_key)
{
  const gchar *id1;
  gchar       *id2;

  id1 = gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));
  id2 = g_settings_get_string (page_key->gsettings, page_key->key);

  if (id1 && g_strcmp0 (id1, id2) != 0)
    g_settings_set_string (page_key->gsettings, page_key->key, id1);

  g_free (id2);
}


static void
on_entry_box_changed (GtkButton        *widget,
                     NimfSettingsPageKey *page_key)
{
  KeyboardInfo *keyboard_info = g_slice_new0(KeyboardInfo);
  keyboard_info->page_key = page_key;
  
  keyboard_info->initial_keys_length = libhangul_get_init_keyboard_ids_length ();
  keyboard_info->initial_keys = g_strdupv ((gchar **)libhangul_get_init_keyboard_ids ());

  build_content_area (keyboard_info, FALSE);
}

static void
on_notify_active (GtkSwitch           *widget,
                  GParamSpec          *pspec,
                  NimfSettingsPageKey *page_key)
{
  gboolean active1;
  gboolean active2;

  active1 = gtk_switch_get_active (GTK_SWITCH (widget));
  active2 = g_settings_get_boolean (page_key->gsettings, page_key->key);

  if (active1 != active2)
    g_settings_set_boolean (page_key->gsettings, page_key->key, active1);
}

void
on_cursor_changed (GtkTreeView *tree_view,
                   GtkStack    *stack)
{
  GtkTreeSelection *selection; /* do not free */
  GtkTreeModel *model;
  gchar        *text = NULL;
  GtkTreeIter iter;

  selection = gtk_tree_view_get_selection (tree_view);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    gtk_tree_model_get (model, &iter, SCHEMA_COLUMN, &text, -1);

  if (text)
    gtk_stack_set_visible_child_name (stack, text);
}

static NimfSettingsPageKey *
nimf_settings_page_key_new (GSettings   *gsettings,
                            const gchar *key,
                            const gchar *summary,
                            const gchar *desc)
{
  NimfSettingsPageKey *page_key;

  page_key            = g_slice_new (NimfSettingsPageKey);
  page_key->gsettings = gsettings;
  page_key->key       = g_strdup (key);
  page_key->label     = gtk_label_new (summary);
  gtk_widget_set_halign (page_key->label, GTK_ALIGN_START);
  gtk_widget_set_tooltip_text (page_key->label, desc);

  return page_key;
}

static void
nimf_settings_page_key_free (NimfSettingsPageKey *page_key)
{
  g_free (page_key->key);
  g_slice_free (NimfSettingsPageKey, page_key);
}

static GtkWidget *
nimf_settings_page_key_build_boolean (NimfSettingsPageKey *page_key)
{
  GtkWidget *gswitch;
  GtkWidget *hbox;
  gchar     *detailed_signal;
  gboolean   is_active;

  gswitch = gtk_switch_new ();
  is_active = g_settings_get_boolean (page_key->gsettings, page_key->key);
  gtk_switch_set_active (GTK_SWITCH (gswitch), is_active);
  gtk_widget_set_halign  (gswitch, GTK_ALIGN_END);
  detailed_signal = g_strdup_printf ("changed::%s", page_key->key);

  g_signal_connect (gswitch, "notify::active",
                    G_CALLBACK (on_notify_active), page_key);
  g_signal_connect (page_key->gsettings, detailed_signal,
                    G_CALLBACK (on_gsettings_changed), gswitch);

  g_free (detailed_signal);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 15);
  gtk_box_pack_start (GTK_BOX (hbox), page_key->label, FALSE, FALSE, 0);
  gtk_box_pack_end   (GTK_BOX (hbox), gswitch, FALSE, FALSE, 0);

  return hbox;
}

static void
on_gsettings_changed_active (GSettings *settings,
                             gchar     *key,
                             GtkWidget *widget)
{
  GtkListStore          *store;
  GSettingsSchemaSource *schema_source;
  GList                 *schema_list = NULL;
  gchar                **non_relocatable;
  const gchar           *id1;
  GtkTreeIter            iter;
  gint                   i;

  id1 = gtk_combo_box_get_active_id (GTK_COMBO_BOX (widget));
  store = (GtkListStore *) gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_list_store_clear (store);

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
  schema_source = g_settings_schema_source_get_default ();
  g_settings_schema_source_list_schemas (schema_source, TRUE,
                                         &non_relocatable, NULL);

  for (i = 0; non_relocatable[i] != NULL; i++)
    if (g_str_has_prefix (non_relocatable[i], "org.nimf.engines."))
      schema_list = g_list_prepend (schema_list, non_relocatable[i]);

  for (schema_list = g_list_sort (schema_list, (GCompareFunc) on_comparison);
       schema_list != NULL;
       schema_list = schema_list->next)
  {
    GSettingsSchema *schema;
    GSettings       *gsettings;
    gchar           *name;
    const gchar     *id2;

    schema = g_settings_schema_source_lookup (schema_source,
                                              schema_list->data, TRUE);
    gsettings = g_settings_new (schema_list->data);
    name = g_settings_get_string (gsettings, "hidden-schema-name");
    id2 = schema_list->data + strlen ("org.nimf.engines.");

    if (g_settings_schema_has_key (schema, "active") == FALSE ||
        g_settings_get_boolean (gsettings, "active"))
    {
      gtk_list_store_append (store, &iter);
      gtk_list_store_set (store, &iter, 0, name, 1, id2, -1);
    }

    g_settings_schema_unref (schema);
    g_free (name);
    g_object_unref (gsettings);
  }

  if (gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), id1) == FALSE)
    gtk_combo_box_set_active_id (GTK_COMBO_BOX (widget), "nimf-system-keyboard");

  g_strfreev (non_relocatable);
  g_list_free (schema_list);
}

static GtkWidget *
nimf_settings_page_key_build_string (NimfSettingsPageKey *page_key,
                                     const gchar         *schema_id,
                                     GList               *key_list)
{
  GtkListStore *store;
  GtkWidget    *combo;
  GtkWidget    *hbox;
  gchar        *detailed_signal;
  GtkTreeIter   iter;

  GtkWidget *entry;
  GtkWidget *button;
  entry = gtk_entry_new();
  button = gtk_button_new_with_label("...");

  store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  combo = gtk_combo_box_text_new ();
  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
  g_object_unref (store);
  gtk_combo_box_set_id_column (GTK_COMBO_BOX (combo), 1);
  gtk_tree_model_get_iter_first ((GtkTreeModel *) store, &iter);

  if (g_strcmp0 (page_key->key, "layout") == 0)
  {
    gchar *id1;

    id1 = g_settings_get_string (page_key->gsettings, page_key->key);

    unsigned layoutCount = hangul_keyboard_list_get_count();
    unsigned index;
    for (index = 0; index < layoutCount; index++)
    {
      const gchar *id2 = hangul_keyboard_list_get_keyboard_id(index);
      const gchar *val = hangul_keyboard_list_get_keyboard_name(index);

      if (g_strcmp0 (id1, id2) == 0) 
      {
        gtk_entry_set_text(GTK_ENTRY (entry), val);
      }

    }

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 15);
    gtk_box_pack_start (GTK_BOX (hbox), page_key->label, FALSE, FALSE, 0);
    gtk_box_pack_end   (GTK_BOX (hbox), button, FALSE, TRUE, 0);
    gtk_box_pack_end   (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

    detailed_signal = g_strdup_printf ("changed::%s", page_key->key);

    g_signal_connect (button, "clicked",
                      G_CALLBACK (on_entry_box_changed), page_key);
    g_signal_connect (page_key->gsettings, detailed_signal,
                      G_CALLBACK (on_gsettings_changed), entry);

    g_free (detailed_signal);

    g_free (id1);
  } 
  else {
  if (g_strcmp0 (schema_id, "org.nimf.engines") == 0 &&
      g_strcmp0 (page_key->key, "default-engine") == 0)
  {
    GSettingsSchemaSource *schema_source;
    GList                 *schema_list = NULL;
    gchar                **non_relocatable;
    gchar                 *id1;
    gint                   i;

    id1 = g_settings_get_string (page_key->gsettings, page_key->key);
    schema_source = g_settings_schema_source_get_default ();
    g_settings_schema_source_list_schemas (schema_source, TRUE,
                                           &non_relocatable, NULL);

    for (i = 0; non_relocatable[i] != NULL; i++)
      if (g_str_has_prefix (non_relocatable[i], "org.nimf.engines."))
        schema_list = g_list_prepend (schema_list, non_relocatable[i]);

    for (schema_list = g_list_sort (schema_list, (GCompareFunc) on_comparison);
         schema_list != NULL;
         schema_list = schema_list->next)
    {
      GSettingsSchema *schema;
      GSettings       *gsettings;
      gchar           *name;
      const gchar     *id2;

      schema = g_settings_schema_source_lookup (schema_source,
                                                schema_list->data, TRUE);
      gsettings = g_settings_new (schema_list->data);
      name = g_settings_get_string (gsettings, "hidden-schema-name");
      id2 = schema_list->data + strlen ("org.nimf.engines.");

      if (g_settings_schema_has_key (schema, "active") == FALSE ||
          g_settings_get_boolean (gsettings, "active"))
      {
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, name, 1, id2, -1);
      }

      if (g_settings_schema_has_key (schema, "active"))
        g_signal_connect (gsettings, "changed::active",
                          G_CALLBACK (on_gsettings_changed_active), combo);

      g_settings_schema_unref (schema);
      g_free (name);
      /*g_object_unref (gsettings);*/ /*** FIXME ***/
    }

    if (gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo), id1) == FALSE)
    {
      g_settings_set_string (page_key->gsettings, "default-engine",
                             "nimf-system-keyboard");
      gtk_combo_box_set_active_id (GTK_COMBO_BOX (combo),
                                   "nimf-system-keyboard");
    }

    g_strfreev (non_relocatable);
    g_list_free (schema_list);
    g_free (id1);
  } 
  else if (g_str_has_prefix (page_key->key, "hidden-") == FALSE)
  {
    gchar *id1;
    GList *list;

    id1 = g_settings_get_string (page_key->gsettings, page_key->key);

    for (list = key_list; list != NULL; list = list->next)
    {
      gchar *key2;
      gchar *prefix;

      key2 = list->data;
      prefix = g_strdup_printf ("hidden-%s-", page_key->key);

      if (g_str_has_prefix (key2, prefix))
      {
        gchar *val;
        const gchar *id2 = key2 + strlen (prefix);

        val = g_settings_get_string (page_key->gsettings, key2);
        gtk_list_store_append (store, &iter);
        gtk_list_store_set (store, &iter, 0, val,
                                          1, id2, -1);

        if (g_strcmp0 (id1, id2) == 0)
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &iter);

        g_free (val);
      }

      g_free (prefix);
    }

    g_free (id1);
  }

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 15);
  gtk_box_pack_start (GTK_BOX (hbox), page_key->label, FALSE, FALSE, 0);
  gtk_box_pack_end   (GTK_BOX (hbox), combo, FALSE, FALSE, 0);
  detailed_signal = g_strdup_printf ("changed::%s", page_key->key);

  g_signal_connect (combo, "changed",
                    G_CALLBACK (on_combo_box_changed), page_key);
  g_signal_connect (page_key->gsettings, detailed_signal,
                    G_CALLBACK (on_gsettings_changed), combo);

  g_free (detailed_signal);
  }

  return hbox;
}

static gboolean
on_key_press_event (GtkWidget *widget,
                    GdkEvent  *event,
                    GtkWidget *dialog)
{
  const gchar *keystr;
  GString     *combination;
  GFlagsClass *flags_class; /* do not free */
  guint        mod;
  guint        i;

  combination = g_string_new ("");
  mod = event->key.state & NIMF_MODIFIER_MASK;
  flags_class = (GFlagsClass *) g_type_class_ref (NIMF_TYPE_MODIFIER_TYPE);

  /* does not use virtual modifiers */
  for (i = 0; i <= 12; i++)
  {
    GFlagsValue *flags_value = g_flags_get_first_value (flags_class,
                                                        mod & (1 << i));
    if (flags_value)
      g_string_append_printf (combination, "%s ", flags_value->value_nick);
  }

  g_type_class_unref (flags_class);

  keystr = nimf_keyval_to_keysym_name (event->key.keyval);

  if (keystr == NULL)
  {
    gchar *text;

    g_string_free (combination, TRUE);
    gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_OK, FALSE);
    text = g_strdup_printf (_("Please report a bug. (keyval: %d)"),
                            event->key.keyval);
    gtk_entry_set_text (GTK_ENTRY (widget), text);

    g_free (text);

    g_return_val_if_fail (keystr, TRUE);
  }

  g_string_append_printf (combination, "%s", keystr);
  gtk_entry_set_text (GTK_ENTRY (widget), combination->str);
  gtk_editable_set_position (GTK_EDITABLE (widget), -1);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK, TRUE);
  g_string_free (combination, TRUE);

  return TRUE;
}

static void
nimf_settings_page_key_update_gsettings_strv (NimfSettingsPageKey *page_key,
                                              GtkTreeModel        *model)
{
  gchar       **vals;
  GtkTreeIter   iter;
  gint          i;

  vals = g_malloc0_n (1, sizeof (gchar *));

  if (gtk_tree_model_get_iter_first (model, &iter))
  {
    i = 0;
    do {
      gtk_tree_model_get (model, &iter, 0, &vals[i], -1);
      vals = g_realloc_n (vals, sizeof (gchar *), i + 2);
      vals[i + 1] = NULL;
      i++;
    } while (gtk_tree_model_iter_next (model, &iter));
  }

  g_settings_set_strv (page_key->gsettings,
                       page_key->key, (const gchar **) vals);

  g_strfreev (vals);
}

static void
on_button_clicked_add (GtkButton           *button,
                       NimfSettingsPageKey *page_key)
{
  GtkWidget *dialog;
  GtkWidget *entry;
  GtkWidget *content_area;
  GtkDialogFlags flags;

#if GTK_CHECK_VERSION (3, 12, 0)
  flags = GTK_DIALOG_MODAL | GTK_DIALOG_USE_HEADER_BAR;
#else
  flags = GTK_DIALOG_MODAL;
#endif

  dialog = gtk_dialog_new_with_buttons (_("Press key combination"),
                                        GTK_WINDOW (nimf_settings_window),
                                        flags,
                                        _("_OK"),     GTK_RESPONSE_OK,
                                        _("_Cancel"), GTK_RESPONSE_CANCEL,
                                        NULL);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "nimf-logo");
  gtk_widget_set_size_request (GTK_WIDGET (dialog), 400, -1);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog),
                                     GTK_RESPONSE_OK, FALSE);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (entry),
                                  _("Click here and then press key combination"));
  gtk_box_pack_start (GTK_BOX (content_area), entry, TRUE, TRUE, 0);
  g_signal_connect (entry, "key-press-event",
                    G_CALLBACK (on_key_press_event), dialog);

  gtk_widget_show_all (content_area);

  switch (gtk_dialog_run (GTK_DIALOG (dialog)))
  {
    case GTK_RESPONSE_OK:
      {
        GtkTreeModel *model;
        const gchar  *text;
        GtkTreeIter   iter;

        model = gtk_tree_view_get_model (GTK_TREE_VIEW (page_key->treeview));
        text = gtk_entry_get_text (GTK_ENTRY (entry));
        gtk_list_store_append (GTK_LIST_STORE (model), &iter);
        gtk_list_store_set (GTK_LIST_STORE (model), &iter, 0, text, -1);
        nimf_settings_page_key_update_gsettings_strv (page_key, model);
      }
      break;
    default:
      break;
  }

  gtk_widget_destroy (dialog);
}

static void
on_button_clicked_remove (GtkButton           *button,
                          NimfSettingsPageKey *page_key)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_key->treeview));

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  {
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
    nimf_settings_page_key_update_gsettings_strv (page_key, model);
  }
}

static void
on_tree_view_realize (GtkWidget         *tree_view,
                      GtkScrolledWindow *scrolled_w)
{
  GtkTreeViewColumn *column;
  gint height;

  column = gtk_tree_view_get_column (GTK_TREE_VIEW (tree_view), 0);
  gtk_tree_view_column_cell_get_size (column, NULL, NULL, NULL, NULL, &height);
  gtk_widget_set_size_request (GTK_WIDGET (scrolled_w), -1, height * 3 );
}

static GtkWidget *
nimf_settings_page_key_build_string_array (NimfSettingsPageKey *page_key)
{
  GtkListStore      *store;
  GtkWidget         *treeview;
  GtkCellRenderer   *renderer;
  GtkTreeViewColumn *column;
  GtkWidget         *scrolled_w;
  GtkWidget         *hbox;
  GtkWidget         *vbox;
  GtkWidget         *button1;
  GtkWidget         *button2;
  gchar            **strv;
  gchar             *detailed_signal;
  GtkTreeIter        iter;
  gint               j;

  button1 = gtk_button_new_from_icon_name ("list-add",    GTK_ICON_SIZE_SMALL_TOOLBAR);
  button2 = gtk_button_new_from_icon_name ("list-remove", GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_button_set_relief (GTK_BUTTON (button1), GTK_RELIEF_NONE);
  gtk_button_set_relief (GTK_BUTTON (button2), GTK_RELIEF_NONE);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

#if GTK_CHECK_VERSION (3, 12, 0)
  gtk_widget_set_margin_end   (page_key->label, 15);
#else
  gtk_widget_set_margin_right (page_key->label, 15);
#endif

  gtk_box_pack_start (GTK_BOX (hbox), page_key->label,   FALSE, FALSE, 0);
  gtk_box_pack_end   (GTK_BOX (hbox), button2, FALSE, FALSE, 0);
  gtk_box_pack_end   (GTK_BOX (hbox), button1, FALSE, FALSE, 0);

  store = gtk_list_store_new (1, G_TYPE_STRING);
  strv = g_settings_get_strv (page_key->gsettings, page_key->key);

  for (j = 0; strv[j] != NULL; j++)
  {
    gtk_list_store_append (store, &iter);
    gtk_list_store_set    (store, &iter, 0, strv[j], -1);
  }

  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  g_object_unref (store);
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Hotkeys", renderer,
                                                     "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (treeview), FALSE);

  scrolled_w = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_w),
                                       GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_w),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
  gtk_container_add (GTK_CONTAINER (scrolled_w), treeview);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox,       FALSE, FALSE, 0);
  gtk_box_pack_end   (GTK_BOX (vbox), scrolled_w, FALSE, FALSE, 0);

  page_key->treeview = treeview;
  detailed_signal = g_strdup_printf ("changed::%s", page_key->key);

  g_signal_connect (treeview, "realize",
                    G_CALLBACK (on_tree_view_realize), scrolled_w);
  g_signal_connect (button1, "clicked", G_CALLBACK (on_button_clicked_add), page_key);
  g_signal_connect (button2, "clicked", G_CALLBACK (on_button_clicked_remove), page_key);
  g_signal_connect (page_key->gsettings, detailed_signal,
                    G_CALLBACK (on_gsettings_changed), page_key->treeview);
  g_strfreev (strv);
  g_free (detailed_signal);

  return vbox;
}

static gchar *
nimf_settings_page_build_title (NimfSettingsPage *page, const gchar *schema_id)
{
  GString *title;
  gchar   *str;
  gchar   *p;

  str   = g_settings_get_string (page->gsettings, "hidden-schema-name");
  title = g_string_new (str);

  for (p = (gchar *) schema_id; *p != 0; p++)
    if (*p == '.')
      g_string_prepend (title, "  ");

  g_string_append (title, "  ");

  g_free (str);

  return g_string_free (title, FALSE);
}

static NimfSettingsPage *
nimf_settings_page_new (NimfSettings *nsettings,
                        const gchar  *schema_id)
{
  NimfSettingsPage *page;
  GSettingsSchema  *schema;
  GList            *key_list = NULL;
  gchar           **keys;
  GList            *l;
  gint              i;

  page = g_slice_new0 (NimfSettingsPage);
  page->gsettings = g_settings_new (schema_id);
  page->title = nimf_settings_page_build_title (page, schema_id);
  page->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 15);
  page->page_keys = g_ptr_array_new_with_free_func ((GDestroyNotify) nimf_settings_page_key_free);

#if GTK_CHECK_VERSION (3, 12, 0)
  gtk_widget_set_margin_start  (page->box, 15);
  gtk_widget_set_margin_end    (page->box, 15);
#else
  gtk_widget_set_margin_left   (page->box, 15);
  gtk_widget_set_margin_right  (page->box, 15);
#endif

  gtk_widget_set_margin_top    (page->box, 15);
  gtk_widget_set_margin_bottom (page->box, 15);

  schema = g_settings_schema_source_lookup (nsettings->schema_source,
                                            schema_id, TRUE);
#if GLIB_CHECK_VERSION (2, 46, 0)
  keys = g_settings_schema_list_keys (schema);
#else
  keys = g_settings_list_keys (page->gsettings);
#endif

  for (i = 0; keys[i] != NULL; i++)
    key_list = g_list_prepend (key_list, keys[i]);

  key_list = g_list_sort (key_list, (GCompareFunc) on_comparison);

  for (i = 0, l = key_list; l != NULL; l = l->next, i++)
  {
    GVariant            *variant;
    GSettingsSchemaKey  *schema_key = NULL;
    NimfSettingsPageKey *page_key;
    const GVariantType  *type;
    const gchar         *key;
    const gchar         *summary;
    const gchar         *desc;

    key = l->data;

    if (g_str_has_prefix (key, "hidden-"))
      continue;

    variant = g_settings_get_value (page->gsettings, key);
    type = g_variant_get_type (variant);
    schema_key = g_settings_schema_get_key (schema, key);
    summary = g_settings_schema_key_get_summary     (schema_key);
    desc    = g_settings_schema_key_get_description (schema_key);

    page_key = nimf_settings_page_key_new (page->gsettings, key, summary, desc);
    g_ptr_array_add (page->page_keys, page_key);

    if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      GtkWidget *item;
      item = nimf_settings_page_key_build_boolean (page_key);
      gtk_box_pack_start (GTK_BOX (page->box), item, FALSE, FALSE, 0);
    }
    else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      GtkWidget *item;
      item = nimf_settings_page_key_build_string (page_key, schema_id, key_list);
      gtk_box_pack_start (GTK_BOX (page->box), item, FALSE, FALSE, 0);
    }
    else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING_ARRAY))
    {
      GtkWidget *item;
      item = nimf_settings_page_key_build_string_array (page_key);
      gtk_box_pack_start (GTK_BOX (page->box), item, FALSE, FALSE, 0);
    }
    else
      g_error (G_STRLOC ": %s: not supported variant type: \"%s\"",
               G_STRFUNC, (gchar *) type);

    g_settings_schema_key_unref (schema_key);
    g_variant_unref (variant);
  }

  g_strfreev (keys);
  g_list_free (key_list);
  g_settings_schema_unref (schema);

  return page;
}

void on_destroy (GtkWidget *widget, GApplication *app)
{
  g_application_release (app);
}

static GtkWidget *
nimf_settings_build_main_window (NimfSettings *nsettings)
{
  GtkWidget  *window;
  GtkWidget  *stack;
  GtkWidget  *sidebar;
  GtkWidget  *box;
  GList      *schema_list = NULL;
  gchar     **non_relocatable;
  gint        i;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
  gtk_window_set_title        (GTK_WINDOW (window), _("Nimf Settings"));
  gtk_window_set_icon_name    (GTK_WINDOW (window), "nimf-logo");

  stack   = gtk_stack_new ();
  sidebar = gtk_stack_sidebar_new ();
  gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (box), sidebar, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (box), stack,   TRUE,  TRUE,  0);

  g_settings_schema_source_list_schemas (nsettings->schema_source, TRUE,
                                         &non_relocatable, NULL);

  for (i = 0; non_relocatable[i] != NULL; i++)
    if (g_str_has_prefix (non_relocatable[i], "org.nimf"))
      schema_list = g_list_prepend (schema_list, non_relocatable[i]);

  for (schema_list = g_list_sort (schema_list, (GCompareFunc) on_comparison);
       schema_list != NULL;
       schema_list = schema_list->next)
  {
    NimfSettingsPage *page;
    GtkWidget        *scrolled_w;

    scrolled_w = gtk_scrolled_window_new (NULL, NULL);
    page = nimf_settings_page_new (nsettings,
                                   (const gchar *) schema_list->data);
    gtk_container_add (GTK_CONTAINER (scrolled_w), page->box);
    gtk_stack_add_titled (GTK_STACK (stack), scrolled_w,
                          (const gchar *) schema_list->data, page->title);
    g_ptr_array_add (nsettings->pages, page);
  }

  gtk_container_add (GTK_CONTAINER (window), box);

  g_strfreev (non_relocatable);
  g_list_free (schema_list);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (on_destroy), nsettings->app);

  return window;
}

static void
on_activate (GApplication *app, NimfSettings *nsettings)
{
  g_application_hold (app);

  if (nimf_settings_window)
  {
    gtk_window_present (GTK_WINDOW (nimf_settings_window));
    g_application_release (app);

    return;
  }

  nimf_settings_window = nimf_settings_build_main_window (nsettings);

  gtk_widget_show_all (nimf_settings_window);
}

int
nimf_settings_run (NimfSettings  *nsettings,
                   int            argc,
                   char         **argv)
{
  return g_application_run (G_APPLICATION (nsettings->app), argc, argv);
}

NimfSettings *
nimf_settings_new ()
{
  return g_object_new (NIMF_TYPE_SETTINGS, NULL);
}

static void nimf_settings_page_free (NimfSettingsPage *page)
{
  g_object_unref (page->gsettings);
  g_free (page->title);
  g_slice_free (NimfSettingsPage, page);
}

static void
nimf_settings_init (NimfSettings *nsettings)
{
  nsettings->schema_source = g_settings_schema_source_get_default ();
  nsettings->pages = g_ptr_array_new_with_free_func ((GDestroyNotify) nimf_settings_page_free);
  nsettings->app = g_application_new ("org.nimf.settings",
                                      G_APPLICATION_FLAGS_NONE);
  g_signal_connect (nsettings->app, "activate",
                    G_CALLBACK (on_activate), nsettings);
}

static void
nimf_settings_finalize (GObject *object)
{
  NimfSettings *nsettings = NIMF_SETTINGS (object);

  g_ptr_array_unref (nsettings->pages);
  g_object_unref (nsettings->app);

  G_OBJECT_CLASS (nimf_settings_parent_class)->finalize (object);
}

static void
nimf_settings_class_init (NimfSettingsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = nimf_settings_finalize;
}

int main (int argc, char **argv)
{
  NimfSettings *nsettings;
  int status;

  g_setenv ("GTK_IM_MODULE", "gtk-im-context-simple", TRUE);

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, NIMF_LOCALE_DIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  gtk_init (&argc, &argv);

  nsettings = nimf_settings_new ();
  status = nimf_settings_run (nsettings, argc, argv);

  g_object_unref (nsettings);

  return status;
}
